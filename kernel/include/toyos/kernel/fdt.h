/*
 * fdt.h — minimal flattened device tree parser
 *
 * This is the aarch64 replacement for the x86 ACPI/MADT path: the DTB that
 * U-Boot (booti) or QEMU's mini-loader passes in x0 is the kernel's only
 * hardware description. Deliberately small — walk, find node, read property.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_FDT_H
#define TOYOS_KERNEL_FDT_H

#include <toyos/kernel/types.h>

#define FDT_MAGIC 0xd00dfeed

/* Opaque node handle: byte offset of FDT_BEGIN_NODE within the blob. */
typedef int fdt_node_t;
#define FDT_NODE_INVALID (-1)

/* Validate the blob header (magic + version). Returns the total size, 0 on
 * failure. */
uint32_t fdt_valid(const void* blob);

/* Find a node by absolute path ("/soc/uart@9000000"). Returns FDT_NODE_INVALID
 * if absent. */
fdt_node_t fdt_find_node(const void* blob, const char* path);

/* Find a node anywhere in the tree whose `compatible` stringlist contains
 * compat. Hardware discovery goes through this, not paths — node names
 * embed unit addresses that vary per board ("intc@8000000" here,
 * "interrupt-controller@fd400000" on the RK3568). */
fdt_node_t fdt_find_compatible(const void* blob, const char* compat);

/* Read one big-endian cell (u32) or cell pair (u64) from property data. */
uint32_t fdt_cell32(const void* cells);
uint64_t fdt_cell64(const void* cells);

/* Look up a property in a node. Returns its length and sets *out to the
 * property value (big-endian cells for integers, raw bytes for strings);
 * returns -1 if absent. */
int fdt_get_prop(const void* blob, fdt_node_t node, const char* name,
                 const void** out);

/* Convenience: read one u32 cell of a property. Returns -1 if absent. */
int fdt_get_prop_u32(const void* blob, fdt_node_t node, const char* name,
                     uint32_t* out);

/* Recursively dump the tree to the console (R0 acceptance: DTB walk on
 * serial). */
void fdt_dump(const void* blob);

/* --- Child iteration (SMP: walking /cpus) ---
 * Node handles double as iteration cursors: child_first returns the first
 * direct child of @parent, child_next the sibling after @node. Both return
 * FDT_NODE_INVALID at the end. Properties of @parent are skipped (DT
 * ordering: props before children), and child_next skips @node's whole
 * subtree — only direct children are visited. */
fdt_node_t fdt_child_first(const void* blob, fdt_node_t parent);
fdt_node_t fdt_child_next(const void* blob, fdt_node_t node);

/* --- /memreserve/ block (R2.1: physical memory discovery) ---
 * The FDT's memory reservation block is an array of (address, size) u64
 * pairs, big-endian, terminated by an all-zero entry. Iterate with
 * idx = 0, 1, ... until it returns -1. Entries may be 0-length (skip). */
int fdt_mem_rsv(const void* blob, int idx, uint64_t* addr, uint64_t* size);

#endif /* TOYOS_KERNEL_FDT_H */
