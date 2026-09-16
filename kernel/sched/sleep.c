/*
 * sleep.c — Timed-sleep support for ToyOS64 (smp-first, Phase 2D-1)
 *
 * Blocks a thread until a given timer tick, then wakes it from timer_tick().
 * The sleep queue is a simple unordered list (threads linked via sleep_node);
 * the number of simultaneously sleeping threads is small, so an O(n) sweep
 * each tick is fine (n = sleepers).
 *
 * smp-first per-CPU adaptation (vs develop's global run_queue): every run_queue
 * mutation — the block path's list_remove and the wakeup sweep's list_push_back
 * — is wrapped in run_lock(this_cpu) -> sched_lock, and woken threads are
 * pushed onto THIS CPU's run_queue (the waker's), not a global one. sleep_lock
 * nests inside sched_lock (run_lock -> sched_lock -> sleep_lock). The joint
 * lock in sleep_check_wakeups is mandatory on SMP: it closes the window where a
 * concurrent FUTEX_WAKE (under run_lock->sched_lock) could double-enqueue one
 * run_queue node (list_push_back is not idempotent) — see the comment there.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/list.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

/* Queue of threads blocked in thread_sleep_until(), linked via sleep_node. */
static struct list_node sleep_queue;

/* Protects sleep_queue. Disjoint nesting from run_lock/sched_lock: the
 * timer-side sweeper (sleep_check_wakeups) holds all three (run_lock ->
 * sched_lock -> sleep_lock) while it scans, unlinks and enqueues. */
static spinlock_t sleep_lock;

void sleep_init(void) {
  list_init(&sleep_queue);
  spin_lock_init(&sleep_lock);
}

void thread_arm_timer(struct thread* t, uint64_t deadline_ticks) {
  /* Caller holds the wake-side lock (irq_save) and will set state + call
   * schedule() itself; this only arms the timer-side linkage. Used by
   * sys_futex to atomically park a thread on BOTH a futex queue and the
   * sleep queue (timed wait). */
  t->wakeup_ticks = deadline_ticks;
  spinlock_acquire(&sleep_lock);
  list_push_back(&sleep_queue, &t->sleep_node);
  spinlock_release(&sleep_lock);
}

/*
 * thread_cancel_timer - Remove @t from the sleep queue if armed.
 *
 * Encapsulates sleep_lock so callers (futex wake, signal wake) do not touch
 * sleep_queue directly. No-op if t has no pending wakeup.
 */
void thread_cancel_timer(struct thread* t) {
  spinlock_acquire(&sleep_lock);
  if (t->wakeup_ticks) {
    list_remove(&t->sleep_node);
    t->wakeup_ticks = 0;
  }
  spinlock_release(&sleep_lock);
}

void thread_sleep_until(uint64_t wakeup_ticks) {
  uint64_t flags = irq_save();
  struct thread* curr = sched_get_current();

  curr->wakeup_ticks = wakeup_ticks;
  spinlock_acquire(&sleep_lock);
  list_push_back(&sleep_queue, &curr->sleep_node);
  spinlock_release(&sleep_lock);

  /* Leave THIS CPU's run queue under run_lock -> sched_lock, then yield.
   * schedule() takes run_lock/sched_lock itself, so release both first. */
  spin_lock_irqsave(&this_cpu()->run_lock);
  spinlock_acquire(&sched_lock);
  curr->state = THREAD_BLOCKED;
  curr->on_cpu = -1;
  list_remove(&curr->run_queue);
  spinlock_release(&sched_lock);
  spin_unlock_irqrestore(&this_cpu()->run_lock);

  schedule();

  /* Resumed: sleep_check_wakeups() (or a signal/futex wake) moved us back onto
   * a run queue. */
  irq_restore(flags);
}

void sleep_check_wakeups(uint64_t now) {
  /* Called from timer_tick() on the BSP (IRQ context, IF=0). JOINT lock
   * run_lock(this_cpu) -> sched_lock -> sleep_lock: scan, unlink
   * sleep_node/wait_node, set futex_timed_out, and enqueue on this CPU's
   * run_queue — all under the three locks.
   *
   * This is mandatory on SMP. A two-phase design (collect under sleep_lock,
   * release, then enqueue under run_lock/sched_lock) opens a window where a
   * concurrent FUTEX_WAKE (under run_lock->sched_lock) enqueues the same
   * timed waiter first -> double-enqueue of one run_queue node
   * (list_push_back is NOT idempotent -> corruption), a wrong TIMEOUT return
   * (futex_timed_out set prematurely), or an ABA (waiter woken, runs,
   * re-blocks on another futex; phase 2 re-wakes it and breaks the new wait).
   *
   * Under the joint lock "is sleep_node still on sleep_queue?" is decided
   * atomically: if FUTEX_WAKE wins it cancel_timer's the sleep_node off
   * first, so we never scan it; if we win, FUTEX_WAKE is blocked on
   * sched_lock. Order run_lock -> sched_lock -> sleep_lock matches every
   * wake path; no reverse nesting exists. */
  spin_lock_irqsave(&this_cpu()->run_lock);
  spinlock_acquire(&sched_lock);
  spinlock_acquire(&sleep_lock);

  unsigned kicked = 0;
  for (struct list_node* n = sleep_queue.next; n != &sleep_queue;) {
    struct list_node* next = n->next; /* save: we may unlink n */
    struct thread* t = container_of(n, struct thread, sleep_node);

    if (t->wakeup_ticks && t->wakeup_ticks <= now) {
      t->wakeup_ticks = 0;
      list_remove(&t->sleep_node);
      /* If also parked on a futex/IPC wait queue, unlink so a later wake
       * can't re-wake it, and mark the timeout. list_remove is safe on a
       * self-referencing (never-linked) node. */
      list_remove(&t->wait_node);
      t->futex_timed_out = true;
      /* R1 guard: under the joint lock we are the sole waker (a racing
       * FUTEX_WAKE is blocked on sched_lock), so t is BLOCKED here. */
      if (t->state == THREAD_BLOCKED) {
        t->state = THREAD_RUNNABLE;
        t->on_cpu = -1;
        list_push_back(&this_cpu()->run_queue, &t->run_queue);
        kicked++;
      }
    }
    n = next;
  }

  spinlock_release(&sleep_lock);
  if (kicked)
    sched_kick_idle(
        kicked); /* stir up to `kicked` idle cores to drain the batch */
  spinlock_release(&sched_lock);
  spin_unlock_irqrestore(&this_cpu()->run_lock);
}
