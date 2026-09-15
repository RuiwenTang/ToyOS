/*
 * process.h — Process control block (R1 staged subset)
 *
 * The scheduler port references the PCB only through the `process` pointer
 * in struct thread and process_put() at thread release; both are inert in
 * R1 (kernel threads have process == NULL, and nothing creates one until
 * the R2 paging + ring-3 port lands with the full upstream copy of this
 * header: struct process, aspace_*, thread_create_user*). Keeping the
 * names now means thread.c needs no edits then.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_PROCESS_H
#define TOYOS_KERNEL_PROCESS_H

#include <toyos/kernel/types.h>

struct process;

/*
 * process_put - Drop a PCB reference. Inert until the real PCB arrives
 * (R2): the only caller (thread_release) runs it under `if (t->process)`,
 * and R1 kernel threads always have process == NULL.
 */
static inline void process_put(struct process* p) { (void)p; }

#endif /* TOYOS_KERNEL_PROCESS_H */
