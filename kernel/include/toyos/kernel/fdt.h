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

#endif /* TOYOS_KERNEL_FDT_H */
