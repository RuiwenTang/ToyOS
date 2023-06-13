
#ifndef TOY_KERNEL_VFS_H
#define TOY_KERNEL_VFS_H

#include <stdint.h>

#include "proc/stack_frame.h"

#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STRING "/"
#define PATH_UP ".."
#define PATH_DOT "."

namespace fs {

void Init();

class Node {
 public:
  Node(const char* name, uint32_t flags, uint32_t mode);

  virtual ~Node() = default;

  const char* GetName() const { return m_name; }

  uint32_t GetFlags() const { return m_flags; }

  uint32_t GetMode() const { return m_mode; }

  uint32_t GetSize() const { return m_size; }

  virtual Node* Open(const char* name, uint32_t flags, uint32_t mode) = 0;

  virtual void Close() = 0;

  virtual uint32_t Read(uint32_t size, uint8_t* buf) = 0;

  virtual uint32_t Write(uint32_t size, uint8_t* buf) = 0;

  virtual bool Seek(uint32_t offset) = 0;

  static Node* GetRootNode();

 protected:
  void SetSize(uint32_t size) { m_size = size; }

 private:
  char m_name[128];
  uint32_t m_flags;
  uint32_t m_mode;
  uint32_t m_size;

  // List in parent children
  Node* prev_in_parent = nullptr;
  Node* next_in_parent = nullptr;

  friend class RootFSNode;
};

void sys_call_open(StackFrame* frame);

void sys_call_close(StackFrame* frame);

}  // namespace fs

#endif  // TOY_KERNEL_VFS_H
