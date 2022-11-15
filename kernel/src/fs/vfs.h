
#ifndef TOY_KERNEL_VFS_H
#define TOY_KERNEL_VFS_H

#include <stdint.h>

#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STRING "/"
#define PATH_UP ".."
#define PATH_DOT "."

struct fs_node;

typedef uint32_t (*read_type_t)(struct fs_node *, uint32_t, uint32_t,
                                uint8_t *);
typedef uint32_t (*write_type_t)(struct fs_node *, uint32_t, uint32_t,
                                 uint8_t *);
typedef struct fs_node *(*open_type_t)(struct fs_node *, const char *name,
                                       uint32_t flag, uint32_t mode);
typedef void (*close_type_t)(struct fs_node *);

typedef void (*seek_type_t)(struct fs_node *, uint32_t);

typedef struct fs_node {
  char name[128];
  uint32_t flags;
  uint32_t length;
  uint32_t offset;

  read_type_t f_read;
  write_type_t f_write;
  open_type_t f_open;
  close_type_t f_close;
  seek_type_t f_seek;

  struct fs_node *parent;
  struct fs_node *children;
  struct fs_node *child_list_next;

} FS_NODE;

void vfs_init();

FS_NODE *vfs_open(const char *name, uint32_t flags, uint32_t mode);

FS_NODE *vfs_close(FS_NODE *node);

FS_NODE *vfs_read(FS_NODE *node, uint32_t offset, uint32_t size, uint8_t *buff);

FS_NODE *vfs_write(FS_NODE *node, uint32_t offset, uint32_t size,
                   uint8_t *buff);

FS_NODE *vfs_seek(FS_NODE *node, uint32_t offset);

#endif  // TOY_KERNEL_VFS_H