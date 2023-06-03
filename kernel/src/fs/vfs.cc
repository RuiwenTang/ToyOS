#include "fs/vfs.hpp"

#include <string.h>

#include "fs/mem/mem_fs.hpp"
#include "util/list.hpp"

namespace fs {

void Init() {}

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

 private:
  util::List<Node> m_children;
};

}  // namespace fs