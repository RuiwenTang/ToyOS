/*
 * sched.c — Kernel thread scheduler
 *
 * Ported from ToyOS64 (x86_64) with only the arch interface exchanged:
 * per-CPU run queue + round-robin pick_next + context switch via
 * switch_to(). The boot thread wraps the kmain context (on entry.S's boot
 * stack) and is the BSP's idle thread. run_lock is held across switch_to
 * and released on the resume side; new threads release it via
 * sched_post_switch in their trampoline.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/fpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/arch/aarch64/sched_arch.h>
#include <toyos/kernel/atomic.h>
#include <toyos/kernel/heap.h>
#include <toyos/kernel/kstack.h>
#include <toyos/kernel/list.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/process.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>

/* Defined in arch/aarch64/switch.S */
extern void switch_to(uint64_t* current_sp, uint64_t* next_sp);
extern void thread_trampoline(void);

/* AP idle-thread entry — defined at the bottom of this file. */
void cpu_idle_loop(void* arg);

bool g_sched_started = false;
static volatile uint32_t next_tid;

#define THREAD_STACK_PAGES (THREAD_STACK_SIZE / PAGE_SIZE)

/* Top of a kstack allocation (first usable byte above the guard + stack pages).
 */
static inline void* kstack_top_of(void* stack_base) {
  return (char*)stack_base +
         (KSTACK_GUARD_PAGES + THREAD_STACK_PAGES) * PAGE_SIZE;
}

/*
 * stack_frame_init - Build the initial saved frame on a fresh thread's stack
 *                    so switch_to's pop sequence (x19..x28, x29, x30) lands
 *                    in thread_trampoline with x19=arg, x20=entry (the x86
 *                    original builds the same shape with r12/rbx).
 *
 * Memory layout at the returned kernel_rsp, low -> high (the exact mirror
 * of switch_to's six STP pairs):
 *   [0] x19 = arg          [6] x25 = 0
 *   [1] x20 = entry        [7] x26 = 0
 *   [2] x21 = 0            [8] x27 = 0
 *   [3] x22 = 0            [9] x28 = 0
 *   [4] x23 = 0           [10] x29 = 0   (terminates the frame chain)
 *   [5] x24 = 0           [11] x30 = thread_trampoline (the ret target)
 *
 * Returns the initial kernel_rsp (lowest address of the 12-qword frame).
 */
static uint64_t stack_frame_init(void* stack_base, void (*entry)(void*),
                                 void* arg) {
  uint64_t* sp = (uint64_t*)kstack_top_of(stack_base);

  sp -= 12;
  sp[0] = (uint64_t)(uintptr_t)arg;                /* x19 */
  sp[1] = (uint64_t)(uintptr_t)entry;              /* x20 */
  sp[2] = sp[3] = sp[4] = sp[5] = 0;               /* x21-x24 */
  sp[6] = sp[7] = sp[8] = sp[9] = 0;               /* x25-x28 */
  sp[10] = 0;                                      /* x29 */
  sp[11] = (uint64_t)(uintptr_t)thread_trampoline; /* x30: ret target */

  return (uint64_t)(uintptr_t)sp;
}

static struct thread* pick_next(void) {
  if (list_empty(&this_cpu()->run_queue)) return NULL;
  return container_of(this_cpu()->run_queue.next, struct thread, run_queue);
}

void sched_init(void) {
  serial_puts("[Sched] Initializing scheduler...\n");

  thread_subsys_init(); /* all_threads list + global sched_lock */
  atomic_store(&next_tid,
               0); /* per-CPU run queues already initialised by percpu_init */

  /*
   * Boot thread: wraps the currently executing kmain context. It uses
   * entry.S's boot stack (stack_base = NULL) and doubles as the BSP's
   * idle thread (cpu_locals[0].idle). kernel_rsp = 0 is safe: the boot
   * thread is only ever switched OUT (in sched_start's first schedule()),
   * and switch_to writes SP before any thread is switched back IN to it.
   */
  struct thread* boot = kcalloc(1, sizeof(*boot));
  if (!boot) {
    serial_puts("[Sched] FATAL: cannot allocate boot TCB\n");
    cpu_halt();
  }

  boot->tid = atomic_add_return(&next_tid, 1) - 1; /* = 0 */
  boot->state = THREAD_RUNNING;
  boot->on_cpu = 0;
  boot->stack_base =
      kstack_alloc(THREAD_STACK_PAGES); /* guarded kstack (boot
                                         * runs on the entry.S boot stack until
                                         * sched_start swaps SP onto this) */
  if (!boot->stack_base) {
    serial_puts("[Sched] FATAL: cannot allocate boot kstack\n");
    cpu_halt();
  }
  boot->kernel_rsp =
      0; /* set on first switch_to (after sched_start swaps SP) */
  boot->entry = NULL;
  boot->arg = NULL;
  list_init(&boot->run_queue);
  list_init(&boot->process_node);
  list_init(&boot->all_threads_node);
  list_init(&boot->wait_node);
  refcount_init(&boot->refcount, 1); /* base ref (boot==idle: never dropped) */
  boot->base_dropped = 0;
  boot->magic = THREAD_MAGIC;
  thread_register(boot);

  const char* bname = "boot";
  for (int i = 0; i < THREAD_NAME_MAX - 1 && bname[i]; i++)
    boot->name[i] = bname[i];

  this_cpu()->current = boot;
  cpu_locals[0].idle = boot;

  serial_puts("[Sched] Boot thread TCB created (tid=0)\n");

  /*
   * Per-CPU idle threads for the APs. Each AP's idle runs cpu_idle_loop()
   * below (armed by the PSCI bring-up, next R1 slice): it enables that
   * core's timer + IRQs on the idle stack, then enters sched_idle_loop.
   * Idle is a per-CPU fallback (picked when the run queue is empty) and
   * never enters a queue. ncpus is set by the bring-up before sched_init,
   * so this covers every online AP.
   */
  for (unsigned i = 1; i < ncpus; i++) {
    char iname[THREAD_NAME_MAX];
    const char* base = "idle";
    int p = 0;
    while (p < THREAD_NAME_MAX - 2 && base[p]) {
      iname[p] = base[p];
      p++;
    }
    iname[p++] = '0' + (char)i;
    iname[p] = '\0';

    struct thread* idle = thread_create_dormant(iname, cpu_idle_loop, NULL);
    if (!idle) continue;
    idle->state = THREAD_RUNNING;
    idle->on_cpu = -1;
    cpu_locals[i].idle = idle;
  }
}

static void thread_log_created(const struct thread* t, uint32_t cpu) {
  serial_printf("[Sched] created thread '%s' tid=%u cpu=%u stack=%x\n", t->name,
                (uint64_t)t->tid, (uint64_t)cpu,
                (uint64_t)(uintptr_t)t->stack_base);
}

/*
 * thread_alloc - Allocate + initialise a thread TCB (heap) and its guarded
 *                kstack, build the initial switch frame, assign a TID + name.
 *                Not queued. Returns NULL on allocation failure.
 */
static struct thread* thread_alloc(const char* name, void (*entry)(void*),
                                   void* arg) {
  struct thread* t = kcalloc(1, sizeof(*t));
  if (!t) {
    serial_puts("[Sched] FATAL: cannot allocate TCB\n");
    return NULL;
  }

  t->stack_base = kstack_alloc(THREAD_STACK_PAGES);
  if (!t->stack_base) return NULL;
  t->kernel_rsp = stack_frame_init(t->stack_base, entry, arg);
  t->entry = entry;
  t->arg = arg;
  t->state = THREAD_RUNNABLE;
  t->on_cpu = -1;
  list_init(&t->run_queue);
  list_init(&t->process_node);
  list_init(&t->all_threads_node);
  list_init(&t->wait_node);
  refcount_init(&t->refcount,
                1); /* base ref: "managed"; dropped once via base_dropped */
  t->base_dropped = 0;
  t->magic = THREAD_MAGIC; /* live-TCB invariant (UAF guard) */
  t->tid = atomic_add_return(&next_tid, 1) - 1;
  thread_register(t); /* add to all_threads under sched_lock */

  if (name) {
    for (int i = 0; i < THREAD_NAME_MAX - 1 && name[i]; i++)
      t->name[i] = name[i];
  }

  return t;
}

struct thread* thread_create(const char* name, void (*entry)(void*),
                             void* arg) {
  struct thread* t = thread_alloc(name, entry, arg);
  if (!t) return NULL;

  spin_lock_irqsave(&this_cpu()->run_lock);
  list_push_back(&this_cpu()->run_queue, &t->run_queue);
  spin_unlock_irqrestore(&this_cpu()->run_lock);

  thread_log_created(t, this_cpu()->index);
  return t;
}

struct thread* thread_create_on(uint32_t cpu, const char* name,
                                void (*entry)(void*), void* arg) {
  struct thread* t = thread_alloc(name, entry, arg);
  if (!t) return NULL;

  if (cpu >= ncpus)
    cpu = this_cpu()->index; /* defensive: fall back to this CPU */

  spin_lock_irqsave(&cpu_locals[cpu].run_lock);
  list_push_back(&cpu_locals[cpu].run_queue, &t->run_queue);
  spin_unlock_irqrestore(&cpu_locals[cpu].run_lock);

  thread_log_created(t, cpu);
  return t;
}

struct thread* thread_create_dormant(const char* name, void (*entry)(void*),
                                     void* arg) {
  struct thread* t = thread_alloc(name, entry, arg);
  if (!t) return NULL;

  serial_puts("[Sched] created dormant thread '");
  serial_puts(t->name);
  serial_puts("' tid=");
  serial_print_dec(t->tid);
  serial_puts("' (idle fallback, not queued)\n");
  return t;
}

void thread_make_runnable(struct thread* t) {
  /* Enqueue a dormant thread on THIS CPU's run queue. Caller must have fully
   * set up the initial frame first (user threads patch x19/x20 + TCB fields
   * before this) to avoid another core picking a half-initialised thread. */
  spin_lock_irqsave(&this_cpu()->run_lock);
  t->state = THREAD_RUNNABLE;
  list_push_back(&this_cpu()->run_queue, &t->run_queue);
  spin_unlock_irqrestore(&this_cpu()->run_lock);
}

void thread_exit(void) {
  struct thread* curr = this_cpu()->current;
  uint64_t flags = irq_save();
  spin_lock_irqsave(&this_cpu()->run_lock);

  curr->state = THREAD_DEAD;
  list_remove(&curr->run_queue); /* never re-picked by pick_next */
  spin_unlock_irqrestore(&this_cpu()->run_lock);

  /* Drop the base reference once (base_dropped guards a double-drop if both
   * thread_exit and a later destroy see this TCB). The current-ref stays until
   * the next schedule() reaps switch_prev, so the TCB survives until we're
   * actually switched out — no kfree/process_put here; thread_release does it.
   */
  if (__atomic_exchange_n(&curr->base_dropped, 1, __ATOMIC_SEQ_CST) == 0)
    refcount_put(&curr->refcount);

  irq_restore(flags);

  /* schedule() sees curr->state == THREAD_DEAD, skips re-enqueue, switches
   * away; the next schedule() on this core reaps switch_prev (== curr) via
   * thread_put -> thread_release -> process_put + kfree. Never returns. */
  schedule();
  cpu_halt();
}

void schedule(void) {
  struct thread* curr = this_cpu()->current;
  if (!curr) return;

  /*
   * Double-layer IRQ save: run_lock.int_flags is a single per-CPU slot that
   * intervening schedule() calls overwrite, so it cannot carry this thread's
   * entry DAIF across switch_to. The local `flags` lives on this stack frame
   * and survives the switch; irq_restore(flags) on the resume side is the
   * authoritative DAIF restore (without it, a thread yielded out with IRQs
   * enabled then switched back in from a masked timer ISR would resume
   * stuck with IRQs masked).
   */
  uint64_t flags = irq_save();

  /* Reap the thread this core switched away from on its previous schedule().
   * OUTSIDE run_lock: thread_put -> thread_release -> process_put nests
   * deeper locks upstream, and we don't nest run_lock under itself. IRQs
   * are off (a tick mid-reap would recurse schedule()). Idle is exempt. */
  struct thread* reaped = this_cpu()->switch_prev;
  this_cpu()->switch_prev = NULL;
  if (reaped && reaped != this_cpu()->idle) thread_put(reaped);

  spin_lock_irqsave(&this_cpu()->run_lock);

  struct thread* next = pick_next();
  if (!next) next = this_cpu()->idle; /* empty queue → idle fallback */

  if (next == curr) {
    spin_unlock_irqrestore(&this_cpu()->run_lock);
    irq_restore(flags);
    return;
  }

  /* The outgoing thread (idle excepted) goes to the back of the queue —
   * UNLESS it is DEAD (thread_exit: stays dequeued for reap) or BLOCKED
   * (sleep/IPC/wait parked itself off the run queue; re-enqueuing here
   * would undo the block and the waiter runs unsafely). */
  if (curr != this_cpu()->idle && curr->state != THREAD_DEAD &&
      curr->state != THREAD_BLOCKED) {
    curr->state = THREAD_RUNNABLE;
    curr->on_cpu = -1;
    list_remove(&curr->run_queue);
    list_push_back(&this_cpu()->run_queue, &curr->run_queue);
  }

  /* The incoming thread takes this CPU. */
  list_remove(&next->run_queue);
  next->state = THREAD_RUNNING;
  next->on_cpu = (int)this_cpu()->index;
  this_cpu()->current = next;

  /* Refcount: pin next with a current-ref (it becomes this core's current)
   * and remember curr as switch_prev so the next schedule() reaps it once
   * it's off the core. Idle is exempt (never freed). */
  if (next != this_cpu()->idle) refcount_get(&next->refcount);
  if (curr != this_cpu()->idle) this_cpu()->switch_prev = curr;

  /* Address space / user TLS / ring-3 entry state — the CR3 + FS_BASE +
   * tss_set_rsp0 block upstream. R1: all identity (see sched_arch.h);
   * the hook is where R2 fills in TTBR0_EL1 + ASID and TPIDR_EL0. */
  arch_sched_switch_state(curr, next);

  /* FPSIMD/NEON: eager save/restore slot (R1 stub — see fpu.h). */
  fpu_context_switch(curr, next);

  /*
   * Switch. run_lock is held ACROSS switch_to and released on the resume
   * side (below) once we land back on curr's stack. New threads release it
   * via sched_post_switch in thread_trampoline instead (they enter through
   * switch_to's 'ret', skipping this resume side).
   */
  switch_to(&curr->kernel_rsp, &next->kernel_rsp);

  /* ---- resume side: back on curr's stack ---- */
  spin_unlock_irqrestore(&this_cpu()->run_lock);
  irq_restore(flags);
}

struct thread* sched_get_current(void) { return this_cpu()->current; }

void thread_yield(void) { schedule(); }

void sched_kick_idle(unsigned n) {
  unsigned kicked = 0;
  for (unsigned i = 0; i < ncpus && kicked < n; i++) {
    if (i == this_cpu()->index) continue;     /* never self-IPI */
    if (cpu_locals[i].idle == NULL) continue; /* slot not online yet */
    struct thread* cur =
        __atomic_load_n(&cpu_locals[i].current, __ATOMIC_ACQUIRE);
    if (cur == cpu_locals[i].idle) { /* only wake cores actually idle */
      /* TargetList is bit-per-Aff0-cpu — both targets are single-cluster
       * (gicv3_send_sgi fills Aff1..3 = 0); multi-cluster boards extend
       * the send, not this loop. */
      gicv3_send_sgi(GICV3_SGI_RESCHED, 1u << i, false);
      kicked++;
    }
  }
}

/*
 * cpu_idle_loop - Entry of every AP's idle thread (armed by the PSCI
 *                 bring-up, next R1 slice; upstream defines it in smp.c
 *                 next to ap_entry). The timer arming that upstream does
 *                 here (per-core LAPIC, only after the switch onto the
 *                 idle stack established a savable kernel_rsp) becomes
 *                 per-core CNTV arming when that slice lands.
 */
void cpu_idle_loop(void* arg) {
  (void)arg;
  irq_enable();
  sched_idle_loop();
}

void sched_post_switch(void) {
  /*
   * Release the run_lock acquired by the schedule() that switched here (a
   * fresh thread enters via switch_to's 'ret', skipping schedule()'s
   * resume-side release), then unconditionally enable interrupts — new
   * threads resume with IRQs masked (schedule() runs irq-disabled), so
   * without this the timer could never preempt them.
   */
  spin_unlock_irqrestore(&this_cpu()->run_lock);
  irq_enable();
}

void sched_start(void) {
  serial_puts("[Sched] Starting scheduler...\n");

  /* Let timer ticks call schedule(). Release-store pairs with the
   * acquire-load the APs will use once PSCI bring-up lands. */
  __atomic_store_n(&g_sched_started, true, __ATOMIC_RELEASE);

  irq_enable(); /* the timer begins ticking */

  /* Switch off the entry.S boot stack onto the boot thread's guarded
   * kstack, so boot/idle has a guarded stack like every other thread. The
   * boot stack is abandoned — we never return through this C frame.
   * Same discipline as upstream's RSP swap; kstack tops are 16-aligned. */
  struct thread* boot = this_cpu()->current;
  uint64_t new_sp = (uint64_t)(uintptr_t)boot->stack_base +
                    (KSTACK_GUARD_PAGES + THREAD_STACK_PAGES) * PAGE_SIZE;
  __asm__ volatile("mov sp, %0" ::"r"(new_sp) : "memory");

  schedule(); /* boot → first worker (now running on the kstack) */

  /* Boot IS the idle thread: once every worker has blocked/yielded, schedule
   * switches back here and we enter the idle loop directly. Do NOT return —
   * the stack was just cut (SP moved to a fresh kstack), so there is no
   * caller frame to pop into. sched_idle_loop is noreturn, so the compiler
   * emits no epilogue after this call. */
  sched_idle_loop();
}

void sched_idle_loop(void) {
  for (;;) {
    schedule(); /* empty queue → next == curr (idle), early return */

    irq_disable();
    /* Re-check under mask: a tick may have queued work since schedule(). */
    if (!list_empty(&this_cpu()->run_queue)) {
      irq_enable();
      continue;
    }
    /* Unmask-then-WFI, the aarch64 shape of upstream's sti;hlt. Note
     * WFI also wakes on a masked-but-pending IRQ, so the re-check above
     * stays necessary for correctness, not just the wakeup window. */
    irq_enable();
    cpu_wait_for_interrupt();
  }
}
