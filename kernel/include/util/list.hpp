#ifndef TOY_KERNEL_UTIL_LIST_HPP
#define TOY_KERNEL_UTIL_LIST_HPP

namespace util {

/**
 * A doubly linked list.
 */
template <typename T>
struct List {
  T* head;
  T* tail;

  List() : head(nullptr), tail(nullptr) {}
  List(T* head, T* tail) : head(head), tail(tail) {}

  /**
   * Inserts a node between prev and next pointers. Updates head and tail
   * pointers if necessary.
   *
   * @param node pointer to node to be inserted
   * @param prev pointer to previous node
   * @param next pointer to next node
   * @param head pointer to head pointer of linked list
   * @param tail pointer to tail pointer of linked list
   *
   * @throws None
   */
  template <T* T::*Prev, T* T::*Next>
  static void Insert(T* node, T* prev, T* next, T** head, T** tail) {
    node->*Prev = prev;
    node->*Next = next;

    if (prev) {
      prev->*Next = node;
    } else if (head) {
      *head = node;
    }

    if (next) {
      next->*Prev = node;
    } else if (tail) {
      *tail = node;
    }
  }

  /**
   * Remove a node from a doubly-linked list, updating the head and tail
   * pointers if necessary.
   *
   * @param node a pointer to the node to remove
   * @param head a pointer to the head pointer of the list
   * @param tail a pointer to the tail pointer of the list
   *
   * @throws None
   */
  template <T* T::*Prev, T* T::*Next>
  static void Remove(T* node, T** head, T** tail) {
    if (node->*Prev) {
      node->*Prev->*Next = node->*Next;
    } else if (head) {
      *head = node->*Next;
    }

    if (node->*Next) {
      node->*Next->*Prev = node->*Prev;
    } else if (tail) {
      *tail = node->*Prev;
    }

    node->*Prev = node->*Next = nullptr;
  }
};

}  // namespace util

#endif  // TOY_KERNEL_UTIL_LIST_HPP