/*
 * sched.h — Kernel thread scheduler
 *
 * Staged aarch64 copy of ToyOS64's sched.h: this header carries exactly the
 * TCB fields and prototypes the ported files reference (sched.c, thread.c,
 * sleep.c, timer.c). Fields whose only consumers are not-yet-ported modules
 * — user-mode state (R2 process/ipc), fpu_state (R2 fpu.c), saved_ctx /
 * fork_ctx (R2 ring-3 + fork), signal state (R3) — are added back
 * additively when those modules land; no upstream field is redesigned.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_SCHED_H
#define TOYOS_KERNEL_SCHED_H

#include <stdbool.h>
#include <toyos/kernel/list.h>
#include <toyos/kernel/refcount.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

typedef uint32_t tid_t;

#define THREAD_STACK_SIZE                             \
  16384 /* 4 pages; allocated via kstack with a guard \
         */
#define THREAD_NAME_MAX 16
#define THREAD_MAGIC \
  0x54485244u /* "THRD": stamped at alloc, cleared before kfree (UAF guard) */

typedef enum {
  THREAD_RUNNABLE = 0, /* on a run queue, waiting to be scheduled */
  THREAD_RUNNING,      /* currently executing on a CPU */
  THREAD_BLOCKED,      /* waiting for an event (sleep/wait/IPC) */
  THREAD_DEAD,         /* terminated, awaiting cleanup (reap) */
} thread_state_t;

/* Forward declaration — PCB defined in process.h (full copy lands with R2
 * paging); a forward decl is enough for the pointer field below. */
struct process;

/*
 * struct thread — Thread Control Block.
 *
 * R1: kernel threads (yield/preempt, per-CPU run queue, refcounted
 * lifecycle, timed sleep). kernel_rsp is the saved SP the aarch64
 * switch_to swaps via &kernel_rsp (the x86 kernel_rsp role, name kept).
 */
struct thread {
  tid_t tid;
  thread_state_t state;
  int on_cpu;          /* CPU index running this thread, -1 = not running */
  uint64_t kernel_rsp; /* saved SP; switch_to swaps via &kernel_rsp */
  void* stack_base;    /* kstack base (NULL for the boot thread's boot stack) */

  struct process*
      process; /* PCB this thread belongs to (NULL for kernel threads) */

  void (*entry)(void* arg); /* entry fn (used to build the initial frame) */
  void* arg; /* entry argument (used to build the initial frame) */
  struct list_node run_queue;

  /* Lifecycle. refcount = base 1 ("managed") + 1 per core running it; TCB
   * freed at 0 (thread_release). magic guards UAF. process_node links into
   * PCB->threads (fork); all_threads_node into the global list; wait_node
   * into a wait queue (futex / IPC, R2+). */
  struct refcount_t refcount;
  uint8_t base_dropped; /* guards the single base-ref drop (exit/destroy) */
  uint32_t magic;       /* THREAD_MAGIC while live, 0 after free */
  struct list_node process_node; /* linkage in process->threads (R2 fork) */
  struct list_node
      all_threads_node;       /* linkage in the global all_threads list */
  struct list_node wait_node; /* linkage in a wait queue (R2+ IPC/futex) */

  /* Timed-sleep support. wakeup_ticks arms the timer-side linkage on
   * sleep_queue; sleep_node links into it. futex_timed_out marks a
   * FUTEX_WAIT woken by timeout (vs woken by FUTEX_WAKE). */
  uint64_t wakeup_ticks;
  struct list_node sleep_node;
  bool futex_timed_out;

  char name[THREAD_NAME_MAX];
};

/* --- Lifecycle --- */

/*
 * sched_lock — global scheduler spinlock. Protects the all_threads list and
 * the refcount re-check in thread_release (the revival-race guard). The
 * per-CPU run_lock guards run_queue + pick/switch; sched_lock guards
 * lifecycle. Lock order: run_lock -> sched_lock.
 */
extern spinlock_t sched_lock;

/* all_threads — global list of every live thread (thread_register on create).
 * thread_get() walks it to locate a TCB by TID. */
extern struct list_node all_threads;

/* thread lifecycle (kernel/sched/thread.c). */
void thread_subsys_init(void); /* list_init all_threads + sched_lock */
void thread_register(
    struct thread* t);                /* push on all_threads under sched_lock */
struct thread* thread_get(tid_t tid); /* pin a TCB by TID; NULL if gone */
void thread_put(struct thread* t);    /* drop a ref; frees TCB at 0. NOT under
                                       * sched_lock/run_lock. */

/*
 * sched_init - Create the boot thread (which wraps the running kmain
 *              context and doubles as the BSP's idle thread) and bind it as
 *              this_cpu()->current. Per-CPU run queues are initialised by
 *              percpu_init(); this only seeds the TID counter.
 */
void sched_init(void);

/*
 * thread_create - Allocate a kernel thread (TCB on the heap, stack on a
 *                 dedicated guarded kstack) and queue it RUNNABLE on this
 *                 CPU's run queue. Returns the TCB, or NULL on allocation
 *                 failure.
 */
struct thread* thread_create(const char* name, void (*entry)(void*), void* arg);

/*
 * thread_create_on - Like thread_create, but queue the thread on @cpu's run
 *                    queue instead of the caller's (cross-CPU placement).
 */
struct thread* thread_create_on(uint32_t cpu, const char* name,
                                void (*entry)(void*), void* arg);

/*
 * thread_create_dormant - Allocate a thread (TCB + guarded stack + initial
 *                         frame) but do NOT queue it. Used for per-CPU idle
 *                         threads, which are fallbacks selected when
 * pick_next() returns NULL, never run-queue entries.
 */
struct thread* thread_create_dormant(const char* name, void (*entry)(void*),
                                     void* arg);

/*
 * thread_make_runnable - Mark a dormant thread RUNNABLE and enqueue it on this
 *                       CPU's run queue. Call only after the thread's initial
 *                       stack frame is fully set up.
 */
void thread_make_runnable(struct thread* t);

/*
 * thread_exit - Terminate the current thread. Marks it THREAD_DEAD, drops its
 *               PCB reference, and schedule()s away (never returns). The TCB
 *               and stack are reclaimed by the next schedule()'s reap of
 *               switch_prev. schedule() skips re-enqueue of a DEAD current
 *               so a dead thread is never re-picked.
 */
void thread_exit(void) __attribute__((noreturn));

/* --- Scheduling --- */

/*
 * schedule - Pick the next runnable thread off this CPU's run queue and
 *            context-switch to it (falling back to this CPU's idle thread when
 *            the queue is empty). A no-op early return if the current thread
 *            is still the only choice.
 */
void schedule(void);

/* thread_yield - Cooperatively give up the CPU. */
void thread_yield(void);

/*
 * sched_kick_idle - Send a reschedule IPI to up to @n idle cores (skipping
 *                   this CPU and offline slots), waking them out of WFI to
 *                   re-check their run queue. Cross-core wake primitive;
 *                   the SGI send arrives with the PSCI slice (next R1).
 */
void sched_kick_idle(unsigned n);

/* sched_get_current - Return the thread running on this CPU. */
struct thread* sched_get_current(void);

/*
 * sched_post_switch - Release this CPU's run_lock and enable interrupts.
 *                     Called only from thread_trampoline on a freshly started
 *                     thread (which enters via switch_to's 'ret', skipping
 *                     schedule()'s resume-side release, and resumes with
 *                     IRQs masked).
 */
void sched_post_switch(void);

/*
 * sched_start - Mark the scheduler started (lets timer ticks call schedule),
 *               enable interrupts, swap onto the boot thread's guarded kstack,
 *               run the first schedule(), then enter sched_idle_loop() — the
 *               boot thread IS the BSP idle thread. Never returns: the boot
 *               stack is abandoned at the SP swap, so there is no C frame to
 *               restore.
 */
void sched_start(void) __attribute__((noreturn));

/*
 * sched_idle_loop - The idle thread body: schedule, and WFI when there is
 *                   nothing runnable. Never returns.
 */
void sched_idle_loop(void) __attribute__((noreturn));

/* --- Timed-sleep support ---
 *
 * thread_sleep_until blocks the caller until a tick deadline;
 * sleep_check_wakeups (called from the BSP timer tick) wakes expired sleepers.
 * thread_arm_timer / thread_cancel_timer let sys_futex atomically park a thread
 * on BOTH a futex queue and the sleep queue (timed wait). Wakeups push onto the
 * waker CPU's run_queue under run_lock->sched_lock->sleep_lock. */
void sleep_init(void);
void thread_arm_timer(struct thread* t, uint64_t deadline_ticks);
void thread_cancel_timer(struct thread* t);
void thread_sleep_until(uint64_t wakeup_ticks);
void sleep_check_wakeups(uint64_t now);

/* Set true by sched_start() before the first schedule(); timer ticks before
 * this only reload+EOI, never schedule. */
extern bool g_sched_started;

#endif /* TOYOS_KERNEL_SCHED_H */
