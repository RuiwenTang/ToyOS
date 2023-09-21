#include "fs/vfs.hpp"

#include <string.h>

#include "fs/ext4/ext4_fs.hpp"
#include "fs/mem/std_fs.hpp"
#include "kprintf.h"
#include "proc/proc.h"
#include "util/list.hpp"
#include "util/string_view.hpp"

namespace fs {

Node::Node(const char* name, uint32_t flags, uint32_t mode) {
  strcpy(m_name, name);
  m_flags = flags;
  m_mode = mode;
  m_size = 0;
}

class RootFSNode : public Node {
 public:
  RootFSNode() : Node(nullptr, 0, 0) {}
  ~RootFSNode() override = default;

  Node* Open(const char* name, uint32_t flags, uint32_t mode) override {
    Node* node = m_children.head;

    do {
      if (strcmp(node->GetName(), name) == 0) {
        return node;
      }
      node = node->next_in_parent;
    } while (node != m_children.tail);

    // try open new fs node

    // "/" is ext4 fs
    if (name[0] == '/') {
      // try open with default ext4 fs node
      return GetDefault()->Open(name, flags, mode);
    }

    return nullptr;
  }

  uint32_t Read(uint32_t size, uint8_t* buf) override { return 0; }

  uint32_t Write(uint32_t size, uint8_t* buf) override { return 0; }

  bool Seek(uint32_t offset, uint32_t origin) override { return false; }

  void Close() override {}

  void RegisterChild(Node* child) {
    util::List<Node>::Insert<&Node::prev_in_parent, &Node::next_in_parent>(
        child, m_children.tail, nullptr, &m_children.head, &m_children.tail);

    if (strcmp(child->m_name, "/") == 0) {
      m_default = child;
    }
  }

  Node* GetDefault() { return m_default; }

 private:
  util::List<Node> m_children;
  Node* m_default;
};

RootFSNode* g_root_fs = nullptr;

Node* Node::GetRootNode() { return g_root_fs; }

void Init() {
  g_root_fs = new RootFSNode();

  // mout ext4 in "/"
  auto node = Ext4FSNode::Mount("/");

  if (node != nullptr) {
    // found ext4 fs node
    kprintf("found ext4 fs node, fs size = %d MB\n",
            node->GetSize() / (1024 * 1024));
    g_root_fs->RegisterChild(node);
  } else {
    // not found ext4 fs node
    kprintf("not found ext4 fs node\n");
  }

  // stdout file
  auto std_node = new StdFSNode();
  g_root_fs->RegisterChild(std_node);
}

/**
 * ebx : char* for file name
 * ecx : flags
 */
void sys_call_open(StackFrame* frame) {
  if (frame->ebx == 0) {
    // no file name
    frame->eax = -1;
    return;
  }

  auto name = reinterpret_cast<const char*>(frame->ebx);

  if (strlen(name) == 0) {
    // empty file name
    frame->eax = -1;
    return;
  }

  auto proc = reinterpret_cast<Proc*>(frame);

  util::StringView path = name;

  // this is a relative
  if (path[0] != '/') {
    path = proc_get_pwd(proc);

    if (path[path.size() - 1] != '/') {
      path += "/";
    }

    path += name;
  }

  uint32_t flags = frame->ecx;

  auto root_node = RootFSNode::GetRootNode();

  auto fs_node = root_node->Open(path.c_str(), flags, 0);

  if (fs_node == nullptr) {
    // failed open file
    frame->eax = -1;
    return;
  }

  auto fd = proc_insert_file(proc, fs_node);

  frame->eax = fd;
}

/**
 * ebx -> file ptr in kernel space
 *
 */
void sys_call_close(StackFrame* frame) {
  if (frame->ebx == 0) {
    frame->eax = -1;
    return;
  }

  if (frame->ebx < 4) {
    // FIXME: this is a stdio file descriptor
    frame->eax = 0;
    return;
  }

  auto proc = reinterpret_cast<Proc*>(frame);

  auto fs_node = proc_get_file_by_id(proc, frame->ebx);

  if (fs_node == nullptr) {
    frame->eax = -1;
    return;
  }

  fs_node->Close();

  proc_remove_file(proc, fs_node);

  frame->eax = 0;
}

/**
 * ebx -> file id
 * ecx -> buffer pointer
 * edx -> buffer len
 */
void sys_call_write(StackFrame* frame) {
  if (frame->ebx < 4) {
    frame->eax = -1;
    return;
  }

  if (frame->ecx <= 0x80000000) {
    frame->eax = -1;
    return;
  }

  auto proc = reinterpret_cast<Proc*>(frame);

  auto fs_node = proc_get_file_by_id(proc, frame->ebx);

  if (fs_node == nullptr) {
    frame->eax = -2;
    return;
  }

  auto buf = reinterpret_cast<uint8_t*>(frame->ecx);
  auto len = frame->edx;

  frame->eax = fs_node->Write(len, buf);
}

/**
 * ebx -> file id
 * ecx -> buffer pointer
 * edx -> buffer len
 */
void sys_call_read(StackFrame* frame) {
  if (frame->ebx < 4) {
    frame->eax = -1;
    return;
  }

  if (frame->ecx <= 0x80000000) {
    frame->eax = -1;
    return;
  }

  auto proc = reinterpret_cast<Proc*>(frame);

  auto fs_node = proc_get_file_by_id(proc, frame->ebx);

  if (fs_node == nullptr) {
    frame->eax = -2;
    return;
  }

  auto buf = reinterpret_cast<uint8_t*>(frame->ecx);
  auto len = frame->edx;

  frame->eax = fs_node->Read(len, buf);
}

/**
 * ebx -> file id
 * ecx -> seek offset
 * edx -> seek origin
 */
void sys_call_seek(StackFrame* frame) {
  if (frame->ebx < 4) {
    frame->eax = -1;
    return;
  }

  auto proc = reinterpret_cast<Proc*>(frame);

  auto fs_node = proc_get_file_by_id(proc, frame->ebx);

  if (fs_node == nullptr) {
    frame->eax = -2;
    return;
  }

  auto offset = frame->ecx;
  auto origin = frame->edx;

  auto success = fs_node->Seek(offset, origin);

  if (success) {
    frame->eax = 0;
  } else {
    frame->eax = -1;
  }
}

}  // namespace fs
