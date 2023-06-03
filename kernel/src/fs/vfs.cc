#include "fs/vfs.hpp"

#include <string.h>

#include "fs/ext4/ext4_fs.hpp"
#include "fs/mem/mem_fs.hpp"
#include "kprintf.h"
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

  uint32_t Read(uint32_t offset, uint32_t size, uint8_t* buf) override {
    return 0;
  }

  uint32_t Write(uint32_t offset, uint32_t size, uint8_t* buf) override {
    return 0;
  }

  bool Seek(uint32_t offset) override { return false; }

  void Close() override {}

  void RegisterChild(Node* child) {
    util::List<Node>::Insert<&Node::prev_in_parent, &Node::next_in_parent>(
        child, m_children.head, nullptr, &m_children.head, &m_children.tail);
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

}  // namespace fs