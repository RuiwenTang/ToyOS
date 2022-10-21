#ifndef KERNEL_MMU_PAGE_ALLOCATOR_H
#define KERNEL_MMU_PAGE_ALLOCATOR_H

#include <stdint.h>

struct PageChunk {
  uint32_t base = 0;
  uint32_t size = 0;
};

struct PageNode {
  PageChunk chunk = {};
  PageNode* prev = nullptr;
  PageNode* next = nullptr;

  PageNode(uint32_t base, uint32_t size)
      : chunk{base, size}, prev(nullptr), next(nullptr) {}

  ~PageNode() = default;
};

class PageAllocator {
 public:
  PageAllocator() = default;
  ~PageAllocator() { CleanUp(); }

  static void Init(uint32_t base, uint32_t size);

  PageChunk Allocate(uint32_t size);

  void Free(PageChunk const& chunk);

  static PageAllocator* Instance();

 private:
  void CleanUp();

  void MergeAvail();

  /**
  * Find node by size in available list
  *
  */
  PageNode* FindNode(uint32_t size);
  /**
  * Find node by base and size in used list
  *
  */
  PageNode* FindNode(uint32_t base, uint32_t size);

  void RemoveAvailable(PageNode* node);
  void InsertIntoUsed(PageNode* node);

 private:
  // available list
  PageNode* m_ava_list;
  // in used list
  PageNode* m_used_list;
};

#endif  // KERNEL_MMU_PAGE_ALLOCATOR_H
