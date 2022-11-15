#include "fs/mem/mem_fs.h"

#include <string.h>

#include "screen/screen.h"

FS_NODE* mem_fs_open(const char* name, uint32_t flags, uint32_t mode) {
  if (memcmp(name, "cout", 4) == 0) {
  }

  return 0;
}