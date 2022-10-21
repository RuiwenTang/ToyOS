#include "mmu/page_allocator.hpp"

namespace {
PageAllocator* g_instance = nullptr;

template <typename T>
T Align4k(T t) {
  t += 0xfff;
  t &= 0xfffff000;

  return t;
}
}  // namespace

void PageAllocator::Init(uint32_t base, uint32_t size) {
  g_instance = new PageAllocator;
  g_instance->m_ava_list = new PageNode(base, size);

  g_instance->m_ava_list->next = g_instance->m_ava_list;
  g_instance->m_ava_list->prev = g_instance->m_ava_list;
}

PageChunk PageAllocator::Allocate(uint32_t size) {
  size = Align4k(size);
  auto node = FindNode(size);

  if (node == nullptr) {
    // no memory available
    return PageChunk{};
  }

  PageChunk chunk{node->chunk.base, size};

  // save used info
  InsertIntoUsed(new PageNode(chunk.base, chunk.size));

  node->chunk.base = chunk.base + size;
  node->chunk.size -= size;

  if (node->chunk.size == 0) {
    RemoveAvailable(node);
  }

  return chunk;
}

void PageAllocator::Free(PageChunk const& chunk) {
  auto node = FindNode(chunk.base, chunk.size);
}
