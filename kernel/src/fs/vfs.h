
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
typedef void (*open_type_t)(struct fs_node *, char *name, uint8_t read,
                            uint8_t write);
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

} FS_NODE;

FS_NODE *vfs_open(const char *name, uint32_t flags, uint32_t mode);

FS_NODE *vfs_close(FS_NODE *node);

FS_NODE *vfs_read(FS_NODE *node, uint32_t offset, uint32_t size, uint8_t *buff);

FS_NODE *vfs_write(FS_NODE *node, uint32_t offset, uint32_t size,
                   uint8_t *buff);

FS_NODE *vfs_seek(FS_NODE *node, uint32_t offset);

#endif  // TOY_KERNEL_VFS_H