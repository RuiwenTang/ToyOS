#include "fs/vfs.h"

#include <string.h>

#include "fs/mem/mem_fs.h"
#include "mmu/heap.h"

FS_NODE* g_root = NULL;

static FS_NODE* root_fs_open(FS_NODE* root, const char* name, uint32_t flags,
                             uint32_t mode) {
  FS_NODE* node = vfs_open(name, flags, mode);

  node->parent = root;
  if (root->children == NULL) {
    root->children = node;
  } else {
    node->child_list_next = root->children;
    root->children = node;
  }

  return node;
}

uint32_t root_fs_read(struct fs_node* root, uint32_t offset, uint32_t size,
                      uint8_t* buf) {
  return 0;
}

uint32_t root_fs_write(struct fs_node* root, uint32_t offset, uint32_t size,
                       uint8_t* buf) {
  return 0;
}

void root_fs_close(struct fs_node* root) { return; }

void root_fs_seek(struct fs_node* root, uint32_t offset) { return; }

void vfs_init() {
  g_root = (FS_NODE*)kmalloc(sizeof(FS_NODE));

  memset(g_root, 0, sizeof(FS_NODE));

  g_root->name[0] = PATH_SEPARATOR;
  g_root->name[1] = 0;

  g_root->f_read = root_fs_read;
  g_root->f_open = root_fs_open;
  g_root->f_write = root_fs_write;
  g_root->f_close = root_fs_close;
  g_root->f_seek = root_fs_seek;
}

FS_NODE* vfs_open(const char* name, uint32_t flags, uint32_t mode) {
  if (name[0] == PATH_SEPARATOR) {
    name++;
  }

  if (memcmp(name, "dev", 3) == 0) {
    // open std io file
    name += 3;
    return mem_fs_open(name, flags, mode);
  } else {
    return NULL;
  }
}
