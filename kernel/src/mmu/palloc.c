#include "mmu/palloc.h"

#include <stdint.h>
#include <string.h>

#ifndef TOY_DEBUG
#include "mmu/heap.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define kmalloc malloc
#define kfree free
#endif

Region* g_free_region;
Region* g_used_region;

#ifdef TOY_DEBUG
Region* free_list() { return g_free_region; }
Region* used_list() { return g_used_region; }
#endif

static void insert_into_used_list(Region* region);
static void insert_into_free_list(Region* region);
static void remove_from_free_list(Region* region);
static void remove_from_used_list(Region* region);
static void insert_into_list(Region** list, Region* node);
static void remove_from_list(Region** list, Region* node);
static void merge_free_list();

void palloc_init(uint32_t base, uint32_t length) {
  g_free_region = (Region*)kmalloc(sizeof(Region));

  g_free_region->base = base;
  g_free_region->length = length;
  g_free_region->next = NULL;
}

uint32_t palloc_allocate(uint32_t size) {
  Region* curr = g_free_region;
  // step 1 find first region fit the required size
  while (curr) {
    if (curr->length >= size) {
      break;
    }

    curr = curr->next;
  }

  if (curr->length < size) {
    // out of memory
    return 0;
  }

  uint32_t base = curr->base;

  remove_from_free_list(curr);

  curr->base += size;
  curr->length -= size;

  if (curr->length > 0) {
    insert_into_free_list(curr);
  }

  Region* used = (Region*)kmalloc(sizeof(Region));
  used->base = base;
  used->length = size;
  used->next = NULL;

  insert_into_used_list(used);

  return base;
}

void palloc_free(uint32_t base, uint32_t size) {
  Region* curr = g_used_region;

  while (curr) {
    if (curr->base == base && curr->length == size) {
      break;
    }

    curr = curr->next;
  }

  // double check but it can be removed
  if (curr->base == base && curr->length == size) {
    remove_from_used_list(curr);
    insert_into_free_list(curr);
    // always merge free list
    merge_free_list();
  }
}

// ------------ private function -----------------

void insert_into_used_list(Region* region) {
  insert_into_list(&g_used_region, region);
}

void insert_into_free_list(Region* region) {
  insert_into_list(&g_free_region, region);
}

void remove_from_used_list(Region* region) {
  remove_from_list(&g_used_region, region);
}

void remove_from_free_list(Region* region) {
  remove_from_list(&g_free_region, region);
}

void insert_into_list(Region** list, Region* region) {
  if (*list == NULL) {
    // fast path
    *list = region;
    return;
  }

  Region* head = *list;
  if (region->base < head->base) {
    // fast path
    region->next = head;
    *list = region;
    return;
  }

  Region* curr = head->next;
  while (curr && curr->base < region->base) {
    head = curr;
    curr = curr->next;
  }

  head->next = region;
  region->next = curr;
}

void remove_from_list(Region** list, Region* region) {
  if (*list == region) {
    // fast path
    *list = region->next;

    return;
  }

  Region* head = *list;
  Region* curr = head->next;

  while (curr && curr != region) {
    head = curr;
    curr = curr->next;
  }

  if (curr) {
    head->next = curr->next;
  }
}

void merge_free_list() {
  Region* curr = g_free_region;

  while (curr && curr->next) {
    if (curr->base + curr->length == curr->next->base) {
      Region* next = curr->next;
      curr->length += next->length;
      curr->next = next->next;
      kfree(next);
      continue;
    }

    curr = curr->next;
  }
}
