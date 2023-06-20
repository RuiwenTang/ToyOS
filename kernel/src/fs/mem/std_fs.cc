#include "fs/mem/std_fs.hpp"

#include <string.h>

#include "screen/screen.h"

namespace fs {

StdFSNode::StdFSNode() : Node("/dev/stdout", 0, 0), m_open_count(1) {}

StdFSNode::~StdFSNode() {}

Node* StdFSNode::Open(const char* name, uint32_t flags, uint32_t mode) {
  m_open_count++;
  return this;
}

uint32_t StdFSNode::Read(uint32_t size, uint8_t* buf) { return 0; }

uint32_t StdFSNode::Write(uint32_t size, uint8_t* buf) {
  screen_print((char*)buf, size, SCREEN_COLOR_GREEN);
  return size;
}

bool StdFSNode::Seek(uint32_t offset) { return false; }

void StdFSNode::Close() { m_open_count--; }

}  // namespace fs
