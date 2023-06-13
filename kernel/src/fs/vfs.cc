#include "fs/vfs.hpp"

#include <string.h>

#include "fs/ext4/ext4_fs.hpp"
#include "fs/mem/mem_fs.hpp"
#include "kprintf.h"
#include "proc/proc.h"
#include "util/list.hpp"

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
    if (strcmp(name, "/") == 0) {
      return nullptr;
    }

    return nullptr;
  }

  uint32_t Read(uint32_t size, uint8_t* buf) override { return 0; }

  uint32_t Write(uint32_t size, uint8_t* buf) override { return 0; }

  bool Seek(uint32_t offset) override { return false; }

  void Close() override {}

  void RegisterChild(Node* child) {
    util::List<Node>::Insert<&Node::prev_in_parent, &Node::next_in_parent>(
        child, m_children.tail, nullptr, &m_children.head, &m_children.tail);
  }

 private:
  util::List<Node> m_children;
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

  uint32_t flags = frame->ecx;

  (void)flags;  // ignore flags for now

  auto root_node = RootFSNode::GetRootNode();

  auto fs_node = root_node->Open(name, flags, 0);

  if (fs_node == nullptr) {
    // failed open file
    frame->eax = -1;
    return;
  }

  auto proc = reinterpret_cast<Proc*>(frame);

  proc_insert_file(proc, fs_node);

  frame->eax = reinterpret_cast<uint32_t>(fs_node);
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

  auto fs_node = reinterpret_cast<fs::Node*>(frame->ebx);

  fs_node->Close();
  // TODO delete fs_node

  frame->eax = 0;
}

}  // namespace fs