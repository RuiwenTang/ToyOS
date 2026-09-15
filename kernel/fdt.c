/*
 * fdt.c — minimal flattened device tree parser
 *
 * The DTB format (devicetree specification v0.3): a 40-byte big-endian
 * header, a memory reservation block, then a structure block that is a
 * linear token stream (BEGIN_NODE / END_NODE / PROP / NOP / END), with
 * property names deduplicated into a trailing strings block. Everything
 * the kernel needs at boot — the console path, memory banks, GIC, timer
 * frequencies — is a node lookup plus a cell read; ~200 lines covers it.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/fdt.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* Structure block tokens. */
#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_struct;
  uint32_t off_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_strings;
  uint32_t size_struct;
};

/* All multi-byte fields are big-endian (the DTB is host-independent). */
static uint32_t be32(const void* p) {
  const uint8_t* b = p;
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static uint32_t hdr(const void* blob, size_t field_ofs) {
  return be32((const uint8_t*)blob + field_ofs);
}

/* Advance past a null-terminated name, padded to 4 bytes. */
static const uint32_t* skip_name(const uint32_t* p) {
  const char* s = (const char*)p;
  size_t len = 0;

  while (s[len]) len++;
  return (const uint32_t*)(s + ((len + 1 + 3) & ~3));
}

uint32_t fdt_valid(const void* blob) {
  uint32_t magic, version;

  if (blob == NULL) return 0;
  magic = hdr(blob, offsetof(struct fdt_header, magic));
  if (magic != FDT_MAGIC) return 0;
  version = hdr(blob, offsetof(struct fdt_header, version));
  if (version < 16) return 0; /* v16+ has the size fields we rely on */
  return hdr(blob, offsetof(struct fdt_header, totalsize));
}

/* Skip past a FDT_PROP token (p points at the token): read len and
 * nameoff, return the payload start. The caller advances past the payload
 * by ((len + 3) & ~3) bytes. */
static const uint32_t* prop_data(const uint32_t* p, uint32_t* len,
                                 const void* blob, const char** name) {
  uint32_t nameoff;

  p++; /* the FDT_PROP token itself */
  *len = be32(p++);
  nameoff = be32(p++);
  *name = (const char*)blob +
          hdr(blob, offsetof(struct fdt_header, off_strings)) + nameoff;
  return p;
}

fdt_node_t fdt_find_node(const void* blob, const char* path) {
  const uint32_t *p, *base;

  if (fdt_valid(blob) == 0 || path[0] != '/') return FDT_NODE_INVALID;

  base = (const uint32_t*)((const uint8_t*)blob +
                           hdr(blob, offsetof(struct fdt_header, off_struct)));
  p = base + 1;     /* skip the root's BEGIN_NODE */
  p = skip_name(p); /* root name (empty) */

  if (path[1] == '\0') return 0; /* "/" — offset of root's BEGIN_NODE */

  const char* seg = path + 1;
  for (;;) {
    uint32_t tok = be32(p);

    if (tok == FDT_PROP) {
      uint32_t len;
      const char* name;

      p = prop_data(p, &len, blob, &name);
      p = (const uint32_t*)((const uint8_t*)p + ((len + 3) & ~3));
    } else if (tok == FDT_NOP) {
      p++;
    } else if (tok == FDT_BEGIN_NODE) {
      p++;
      const char* node_name = (const char*)p;
      size_t seg_len = 0;

      while (seg[seg_len] && seg[seg_len] != '/') seg_len++;
      size_t name_len = 0;
      while (node_name[name_len]) name_len++;

      if (seg_len == name_len) {
        size_t i = 0;
        while (i < seg_len && seg[i] == node_name[i]) i++;
        if (i == seg_len) {
          int node_off = (int)((const uint8_t*)p - 4 - (const uint8_t*)base);
          p = skip_name(p);
          seg += seg_len;
          if (*seg == '\0') return node_off;
          seg++; /* past '/', descend */
          continue;
        }
      }
      /* Not a match: skip the whole subtree. */
      p = skip_name(p);
      int depth = 1;
      while (depth > 0) {
        tok = be32(p);
        if (tok == FDT_BEGIN_NODE) {
          depth++;
          p++;
          p = skip_name(p);
        } else if (tok == FDT_END_NODE) {
          depth--;
          p++;
        } else if (tok == FDT_PROP) {
          uint32_t len;
          const char* name;

          p = prop_data(p, &len, blob, &name);
          p = (const uint32_t*)((const uint8_t*)p + ((len + 3) & ~3));
        } else {
          p++; /* NOP (or END, which ends everything) */
        }
      }
    } else {
      break; /* FDT_END / FDT_END_NODE: ran out */
    }
  }
  return FDT_NODE_INVALID;
}

int fdt_get_prop(const void* blob, fdt_node_t node, const char* name,
                 const void** out) {
  const uint32_t *p, *base;
  uint32_t len;
  const char* pname;

  if (fdt_valid(blob) == 0 || node < 0) return -1;

  base = (const uint32_t*)((const uint8_t*)blob +
                           hdr(blob, offsetof(struct fdt_header, off_struct)));
  p = (const uint32_t*)((const uint8_t*)base + node);
  p++; /* BEGIN_NODE */
  p = skip_name(p);

  for (;;) {
    uint32_t tok = be32(p);

    if (tok == FDT_PROP) {
      p = prop_data(p, &len, blob, &pname);
      const void* data = p;
      p = (const uint32_t*)((const uint8_t*)p + ((len + 3) & ~3));

      size_t i = 0;
      while (name[i] && name[i] == pname[i]) i++;
      if (name[i] == '\0' && pname[i] == '\0') {
        if (out) *out = data;
        return (int)len;
      }
    } else if (tok == FDT_NOP) {
      p++;
    } else {
      break; /* END_NODE or END: no such property */
    }
  }
  return -1;
}

int fdt_get_prop_u32(const void* blob, fdt_node_t node, const char* name,
                     uint32_t* out) {
  const void* data;
  int len = fdt_get_prop(blob, node, name, &data);

  if (len < 4) return -1;
  *out = be32(data);
  return 0;
}

uint32_t fdt_cell32(const void* cells) { return be32(cells); }

/* Two big-endian u32 cells → one u64. Volatile byte loads on purpose:
 * clang -O2 merges adjacent plain loads into a single 8-byte access,
 * which is value-correct but faults on 4-aligned data while the MMU is
 * off (everything is Device memory). Volatile keeps the accesses narrow. */
uint64_t fdt_cell64(const void* cells) {
  const volatile uint8_t* b = cells;
  uint32_t hi = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
                ((uint32_t)b[2] << 8) | (uint32_t)b[3];
  uint32_t lo = ((uint32_t)b[4] << 24) | ((uint32_t)b[5] << 16) |
                ((uint32_t)b[6] << 8) | (uint32_t)b[7];
  return ((uint64_t)hi << 32) | lo;
}

/* --- find by compatible --- */

static int prop_streq(const char* a, const char* b) {
  size_t i = 0;

  while (a[i] && a[i] == b[i]) i++;
  return a[i] == '\0' && b[i] == '\0';
}

/* compatible is a stringlist (NUL-separated); any element may match. */
static int stringlist_has(const char* data, uint32_t len, const char* s) {
  uint32_t i = 0;

  while (i < len) {
    if (prop_streq(data + i, s)) return 1;
    while (i < len && data[i] != '\0') i++;
    i++; /* past the NUL */
  }
  return 0;
}

fdt_node_t fdt_find_compatible(const void* blob, const char* compat) {
  const uint32_t *p, *base;

  if (fdt_valid(blob) == 0) return FDT_NODE_INVALID;

  base = (const uint32_t*)((const uint8_t*)blob +
                           hdr(blob, offsetof(struct fdt_header, off_struct)));
  p = base;

  for (;;) {
    uint32_t tok = be32(p);

    if (tok == FDT_BEGIN_NODE) {
      int node_off = (int)((const uint8_t*)p - (const uint8_t*)base);
      p++;
      p = skip_name(p);

      /* This node's properties end at its first child or its END_NODE;
       * the outer loop handles whichever it is. */
      for (;;) {
        uint32_t t = be32(p);

        if (t == FDT_PROP) {
          uint32_t len;
          const char* name;

          p = prop_data(p, &len, blob, &name);
          const char* data = (const char*)p;
          p = (const uint32_t*)((const uint8_t*)p + ((len + 3) & ~3));

          if (prop_streq(name, "compatible") &&
              stringlist_has(data, len, compat))
            return node_off;
        } else if (t == FDT_NOP) {
          p++;
        } else {
          break;
        }
      }
    } else if (tok == FDT_END_NODE || tok == FDT_NOP) {
      p++;
    } else {
      break; /* FDT_END */
    }
  }
  return FDT_NODE_INVALID;
}

/* --- dump (R0 acceptance: DTB walk on serial) --- */

static void print_indent(int depth) {
  for (int i = 0; i < depth; i++) serial_puts("    ");
}

static void print_prop_value(const void* data, uint32_t len) {
  /* A trailing NUL does not disqualify a string; an interior NUL does
   * (those are cells and print as <0x...>). Strip it only for the
   * printable scan — cell layout must use the full length or a reg
   * ending in 0x00 shrinks to a non-multiple of 4. */
  int printable = len > 0;
  uint32_t plen = len;

  if (plen > 0 && ((const uint8_t*)data)[plen - 1] == '\0') plen--;
  for (uint32_t i = 0; i < plen; i++) {
    uint8_t c = ((const uint8_t*)data)[i];
    if (c < 0x20 || c > 0x7e) {
      printable = 0;
      break;
    }
  }

  if (printable) {
    serial_puts("\"");
    for (uint32_t i = 0; i < plen; i++)
      serial_putchar((char)((const uint8_t*)data)[i]);
    serial_puts("\"");
    return;
  }
  if (len % 4 != 0) {
    serial_puts("<bytes>");
    return;
  }
  serial_puts("<");
  for (uint32_t i = 0; i < len; i += 4) {
    if (i) serial_putchar(' ');
    serial_puts("0x");
    uint32_t v = be32((const uint8_t*)data + i);
    static const char digits[] = "0123456789abcdef";
    for (int s = 28; s >= 0; s -= 4) {
      uint32_t nibble = (v >> s) & 0xf;
      if (nibble == 0 && s > 0 && (v >> s) == 0) continue;
      serial_putchar(digits[nibble]);
    }
  }
  serial_puts(">");
}

void fdt_dump(const void* blob) {
  const uint32_t *p, *base;
  int depth = 0;

  serial_puts("fdt: total size ");
  serial_print_dec(hdr(blob, offsetof(struct fdt_header, totalsize)));
  serial_puts(", version ");
  serial_print_dec(hdr(blob, offsetof(struct fdt_header, version)));
  serial_puts("\n");

  base = (const uint32_t*)((const uint8_t*)blob +
                           hdr(blob, offsetof(struct fdt_header, off_struct)));
  p = base;

  for (;;) {
    uint32_t tok = be32(p);

    switch (tok) {
      case FDT_BEGIN_NODE:
        p++;
        print_indent(depth);
        serial_puts((const char*)p);
        serial_puts(" {\n");
        p = skip_name(p);
        depth++;
        break;
      case FDT_END_NODE:
        p++;
        depth--;
        print_indent(depth);
        serial_puts("};\n");
        break;
      case FDT_PROP: {
        uint32_t len;
        const char* name;

        p = prop_data(p, &len, blob, &name);
        const void* data = p;
        p = (const uint32_t*)((const uint8_t*)p + ((len + 3) & ~3));

        print_indent(depth);
        serial_puts(name);
        serial_puts(" = ");
        print_prop_value(data, len);
        serial_puts(";\n");
        break;
      }
      case FDT_NOP:
        p++;
        break;
      default:
        return; /* FDT_END */
    }
  }
}
