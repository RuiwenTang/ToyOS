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

}  // namespace fs