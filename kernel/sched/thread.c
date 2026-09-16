/*
 * thread.c — Thread lifecycle: refcount pinning, lookup, release
 *            (smp-first Phase 2b)
 *
 * Unifies thread lifetime across fork/exit/destroy via a single refcount:
 *   base=1 ("managed", dropped once via base_dropped by thread_exit) + 1 while
 *   running on a core (inc'd on switch-in, dec'd by the next schedule()'s reap
 *   of switch_prev). The TCB is freed when it hits 0 — guaranteeing a thread is
 *   never freed while it is still current on some core.
 *
 * The per-CPU run_lock guards run_queue + pick/switch; this global sched_lock
 * guards the all_threads list + the refcount re-check in thread_release (the
 * revival-race guard). Reap runs OUTSIDE run_lock, so thread_release nests only
 * sched_lock -> vmm_lock (process_put -> aspace_destroy).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/fpu.h>
#include <toyos/kernel/heap.h>
#include <toyos/kernel/kstack.h>
#include <toyos/kernel/list.h>
#include <toyos/kernel/process.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

spinlock_t sched_lock;
struct list_node all_threads;

void thread_subsys_init(void) {
  list_init(&all_threads);
  spinlock_init(&sched_lock);
}

void thread_register(struct thread* t) {
  /* Add to the global all_threads list under sched_lock so thread_get()'s
   * concurrent walk can't see a half-inserted node. */
  spinlock_acquire(&sched_lock);
  list_push_back(&all_threads, &t->all_threads_node);
  spinlock_release(&sched_lock);
}

/*
 * thread_release - Final cleanup for a thread whose refcount just hit 0.
 *
 * C1 (revival race): thread_get() can pin ANY tid, so between the atomic
 * dec-to-0 and this list_remove another core's thread_get() could walk
 * all_threads, find @t, and inc its refcount back above 0 ("revive" it); a
 * kfree after that hands that core a freed pointer. Guard: re-check refcount
 * UNDER sched_lock — thread_get()'s inc also happens under sched_lock, so the
 * two are mutually exclusive.
 *
 * The list_removes are idempotent on nodes thread_exit already unlinked
 * (thread_exit does list_remove; a self-referencing node left after list_init
 * is a no-op here).
 */
static void thread_release(struct thread* t) {
  spinlock_acquire(&sched_lock);
  if (refcount_read(&t->refcount) != 0) {
    spinlock_release(&sched_lock);
    return; /* revived by a concurrent thread_get() */
  }
  list_remove(&t->run_queue);
  list_remove(&t->all_threads_node);
  list_remove(&t->process_node);
  list_remove(
      &t->wait_node); /* a blocked TCB may still be on a futex/IPC queue */
  spinlock_release(&sched_lock);

  fpu_on_thread_exit(t);
  if (t->process) {
    process_put(
        t->process); /* -> aspace_destroy (vmm_lock), outside sched_lock */
    t->process = NULL;
  }
  if (t->stack_base)
    kstack_free(
        t->stack_base); /* return the guarded stack slot to the free-list */
  t->magic = 0;         /* clear before free (UAF signature guard) */
  kfree(t);
}

/*
 * thread_put - Drop a thread reference; frees the TCB + PCB ref when the last
 *              reference goes away. MUST NOT be called while holding
 * sched_lock, run_lock, vmm_lock, or process_lock (thread_release takes them).
 */
void thread_put(struct thread* t) {
  if (!t) return;
  if (refcount_put(&t->refcount)) thread_release(t);
}

/*
 * thread_get - Look up a thread by TID and pin it with a reference. Returns a
 *              TCB guaranteed valid until a matching thread_put(), or NULL if
 *              no live thread has @tid. Replaces bare thread_find_by_tid()
 *              (unpinned pointer that another core could reap/free — UAF).
 */
struct thread* thread_get(tid_t tid) {
  spinlock_acquire(&sched_lock);
  for (struct list_node* n = all_threads.next; n != &all_threads; n = n->next) {
    struct thread* t = container_of(n, struct thread, all_threads_node);
    if (t->tid == tid) {
      refcount_get(&t->refcount);
      spinlock_release(&sched_lock);
      return t;
    }
  }
  spinlock_release(&sched_lock);
  return NULL;
}
