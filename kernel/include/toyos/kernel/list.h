/*
 * list.h — Intrusive doubly-linked circular list for ToyOS64
 *
 * Inspired by the Linux/Minix kernel list pattern. Each list_node
 * is embedded inside a container struct; use container_of() to
 * recover the container from the node pointer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_LIST_H
#define TOYOS_KERNEL_LIST_H

#include <toyos/kernel/types.h>

struct list_node {
  struct list_node* next;
  struct list_node* prev;
};

/* --- Initialization --- */

static inline void list_init(struct list_node* head) {
  head->next = head;
  head->prev = head;
}

/* --- Query --- */

static inline bool list_empty(const struct list_node* head) {
  return head->next == head;
}

/* --- Insertion --- */

/*
 * list_insert_after - Insert 'node' immediately after 'pos'.
 *
 *   pos ↔ pos.next   becomes   pos ↔ node ↔ pos.next
 */
static inline void list_insert_after(struct list_node* pos,
                                     struct list_node* node) {
  node->next = pos->next;
  node->prev = pos;
  pos->next->prev = node;
  pos->next = node;
}

/*
 * list_push_back - Append 'node' at the tail of the list (before head).
 */
static inline void list_push_back(struct list_node* head,
                                  struct list_node* node) {
  list_insert_after(head->prev, node);
}

/*
 * list_push_front - Prepend 'node' at the head of the list (after head).
 * The node becomes the first element and will be returned by the next
 * pick_next / list_pop_front call.
 */
static inline void list_push_front(struct list_node* head,
                                   struct list_node* node) {
  list_insert_after(head, node);
}

/* --- Removal --- */

/*
 * list_remove - Unlink 'node' from whatever list it belongs to.
 * Does not zero the node's pointers.
 */
static inline void list_remove(struct list_node* node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->next = node;
  node->prev = node;
}

/*
 * list_pop_front - Remove and return the first node after 'head'.
 * Returns NULL if the list is empty.
 */
static inline struct list_node* list_pop_front(struct list_node* head) {
  if (list_empty(head)) return NULL;

  struct list_node* first = head->next;
  list_remove(first);
  return first;
}

#endif /* TOYOS_KERNEL_LIST_H */
