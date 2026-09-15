/*
 * test_heap.cpp — Host-side unit tests for the kernel heap allocator
 *
 * Uses Google Test framework. The heap implementation (kernel/mm/heap.c)
 * is compiled as C and linked here via extern "C". A test provider
 * backs page allocation with posix_memalign/free.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
/* Include kernel types first — on host, <stdint.h> sets __CLANG_STDINT_H
 * so the manual typedefs in types.h are skipped. */
#include <toyos/kernel/heap.h>
}

/* --- Test provider: backed by host posix_memalign --- */

static std::vector<void *> *g_allocated_pages = nullptr;

static void *test_alloc_pages(void *ctx, size_t count)
{
    (void)ctx;
    size_t total = count * 4096;
    void *mem = nullptr;
    if (posix_memalign(&mem, 4096, total) != 0)
        return nullptr;
    std::memset(mem, 0xCC, total); /* fill with debug pattern */
    if (g_allocated_pages)
        g_allocated_pages->push_back(mem);
    return mem;
}

static void test_free_pages(void *ctx, void *addr, size_t count)
{
    (void)ctx;
    (void)count;
    std::free(addr);
}

static const heap_provider_t test_heap_provider = {
    .alloc_pages = test_alloc_pages,
    .free_pages  = test_free_pages,
};

/* --- Test fixture --- */

class HeapTest : public ::testing::Test {
protected:
    std::vector<void *> allocated;

    void SetUp() override
    {
        g_allocated_pages = &allocated;
        heap_init(&test_heap_provider, nullptr, 0);
    }

    void TearDown() override
    {
        /* Free all pages allocated during the test */
        for (void *p : allocated)
            std::free(p);
        g_allocated_pages = nullptr;
    }
};

/* === Initialization tests === */

TEST_F(HeapTest, InitDoesNotAllocatePages)
{
    /* heap_init is lazy — no pages until first kmalloc */
    EXPECT_EQ(allocated.size(), 0u);
}

/* === Allocation tests === */

TEST_F(HeapTest, AllocBasic)
{
    void *p = kmalloc(64);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 16, 0u) << "allocation not 16-byte aligned";
}

TEST_F(HeapTest, AllocZeroReturnsNull)
{
    EXPECT_EQ(kmalloc(0), nullptr);
}

TEST_F(HeapTest, AllocLargeCrossPage)
{
    /* Request > 4096 bytes — must trigger at least 1 alloc_pages call
     * for multiple pages (heap_grow computes pages_needed = ceil((8192+32)/4096) = 3) */
    void *p = kmalloc(8192);
    ASSERT_NE(p, nullptr);
    EXPECT_GE(allocated.size(), 1u);
}

TEST_F(HeapTest, AllocMultipleDistinct)
{
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(32 + i * 16);
        ASSERT_NE(ptrs[i], nullptr);
    }

    /* All pointers must be distinct */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ptrs[i], ptrs[j])
                << "duplicate pointers at index " << i << " and " << j;
        }
    }
}

TEST_F(HeapTest, AllocAlignment)
{
    /* Test various sizes for 16-byte alignment */
    size_t sizes[] = {1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 100, 255, 256, 1000};
    for (size_t s : sizes) {
        void *p = kmalloc(s);
        ASSERT_NE(p, nullptr) << "kmalloc(" << s << ") returned NULL";
        EXPECT_EQ((uintptr_t)p % 16, 0u)
            << "kmalloc(" << s << ") not aligned: " << p;
    }
}

TEST_F(HeapTest, AllocAndWrite)
{
    /* Allocate and write to verify the memory is usable */
    char *p = (char *)kmalloc(128);
    ASSERT_NE(p, nullptr);
    std::memset(p, 'A', 128);
    for (int i = 0; i < 128; i++)
        EXPECT_EQ(p[i], 'A');
}

/* === Free tests === */

TEST_F(HeapTest, FreeNullIsNoop)
{
    kfree(nullptr); /* should not crash */
}

TEST_F(HeapTest, FreeBasic)
{
    void *p = kmalloc(64);
    ASSERT_NE(p, nullptr);
    size_t pages_before = allocated.size();

    kfree(p);

    /* No new pages should be allocated by kfree */
    EXPECT_EQ(allocated.size(), pages_before);
}

TEST_F(HeapTest, FreeAndReallocSameSize)
{
    void *p1 = kmalloc(64);
    ASSERT_NE(p1, nullptr);
    std::memset(p1, 0x42, 64);

    kfree(p1);

    /* Allocate same size — should reuse the freed block */
    void *p2 = kmalloc(64);
    ASSERT_NE(p2, nullptr);
    /* p2 may or may not equal p1, but no new pages should be needed */
}

TEST_F(HeapTest, FreeAndReallocLarger)
{
    void *p1 = kmalloc(32);
    ASSERT_NE(p1, nullptr);
    size_t pages_after_first = allocated.size();

    kfree(p1);

    void *p2 = kmalloc(128);
    ASSERT_NE(p2, nullptr);

    /* May or may not need new pages depending on splitting */
}

/* === Splitting tests === */

TEST_F(HeapTest, SplitOnAlloc)
{
    /* Allocate a large block (triggers a page), then a small one.
     * The large page should be split. */
    void *large = kmalloc(64);
    ASSERT_NE(large, nullptr);
    size_t pages_after_large = allocated.size();

    /* Free it — the whole page-sized block becomes one free block */
    kfree(large);

    /* Now allocate a small block — it should split from the free block */
    void *small = kmalloc(32);
    ASSERT_NE(small, nullptr);

    /* No new pages should be needed — the small block fits in the existing free space */
    EXPECT_EQ(allocated.size(), pages_after_large);
}

TEST_F(HeapTest, SplitRemainderIsFree)
{
    /* Allocate to fill a page, then free. Allocate small, then allocate another small.
     * Both should fit in the same page. */
    void *p1 = kmalloc(64);
    ASSERT_NE(p1, nullptr);
    kfree(p1);

    void *a = kmalloc(64);
    void *b = kmalloc(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
}

/* === Coalescing tests === */

TEST_F(HeapTest, ForwardCoalesce)
{
    /* Allocate three blocks, free first two, verify they merge */
    void *a = kmalloc(64);
    void *b = kmalloc(64);
    void *c = kmalloc(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    size_t pages_before = allocated.size();

    kfree(a);
    kfree(b); /* b should coalesce with a (forward) */

    /* Now allocate something larger than 64 but <= 64 + metadata + 64 */
    /* The coalesced block should be able to satisfy this without new pages */
    void *big = kmalloc(128);
    ASSERT_NE(big, nullptr);

    /* Should not have needed new pages */
    EXPECT_EQ(allocated.size(), pages_before);

    kfree(c);
    kfree(big);
}

TEST_F(HeapTest, BackwardCoalesce)
{
    void *a = kmalloc(64);
    void *b = kmalloc(64);
    void *c = kmalloc(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    size_t pages_before = allocated.size();

    kfree(b);
    kfree(a); /* a should coalesce with b (backward) */

    void *big = kmalloc(128);
    ASSERT_NE(big, nullptr);
    EXPECT_EQ(allocated.size(), pages_before);

    kfree(c);
    kfree(big);
}

TEST_F(HeapTest, FullCoalesce)
{
    void *a = kmalloc(64);
    void *b = kmalloc(64);
    void *c = kmalloc(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    size_t pages_before = allocated.size();

    kfree(a);
    kfree(c);
    kfree(b); /* b coalesces with both a and c */

    void *big = kmalloc(200);
    ASSERT_NE(big, nullptr);
    EXPECT_EQ(allocated.size(), pages_before);

    kfree(big);
}

TEST_F(HeapTest, NoCoalesceAcrossAllocatedBlock)
{
    void *a = kmalloc(64);
    void *b = kmalloc(64);
    void *c = kmalloc(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    kfree(a);
    kfree(c); /* a and c should NOT merge — b is between them */

    /* b is still allocated, so a and c are separate free blocks */
    /* Allocating something that only fits in a+c together should need new pages */
    /* (This is a soft test — we verify the allocator doesn't crash) */
    kfree(b);
}

/* === kcalloc tests === */

TEST_F(HeapTest, CallocBasic)
{
    void *p = kcalloc(4, 64);
    ASSERT_NE(p, nullptr);

    /* Verify all bytes are zero */
    unsigned char *bytes = (unsigned char *)p;
    for (size_t i = 0; i < 256; i++)
        EXPECT_EQ(bytes[i], 0) << "non-zero byte at index " << i;
}

TEST_F(HeapTest, CallocOverflow)
{
    EXPECT_EQ(kcalloc(SIZE_MAX, 2), nullptr);
    EXPECT_EQ(kcalloc(2, SIZE_MAX), nullptr);
}

TEST_F(HeapTest, CallocZero)
{
    EXPECT_EQ(kcalloc(0, 64), nullptr);
    EXPECT_EQ(kcalloc(4, 0), nullptr);
}

/* === krealloc tests === */

TEST_F(HeapTest, ReallocNull)
{
    void *p = krealloc(nullptr, 64);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 16, 0u);
}

TEST_F(HeapTest, ReallocZero)
{
    void *p = kmalloc(64);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(krealloc(p, 0), nullptr);
}

TEST_F(HeapTest, ReallocGrow)
{
    char *p = (char *)kmalloc(64);
    ASSERT_NE(p, nullptr);
    std::memset(p, 'X', 64);

    char *p2 = (char *)krealloc(p, 128);
    ASSERT_NE(p2, nullptr);

    /* First 64 bytes should be preserved */
    for (int i = 0; i < 64; i++)
        EXPECT_EQ(p2[i], 'X') << "data lost at index " << i;
}

TEST_F(HeapTest, ReallocShrink)
{
    char *p = (char *)kmalloc(128);
    ASSERT_NE(p, nullptr);
    std::memset(p, 'Y', 128);

    char *p2 = (char *)krealloc(p, 32);
    ASSERT_NE(p2, nullptr);

    /* Pointer should be the same when shrinking */
    EXPECT_EQ(p2, p);

    /* First 32 bytes preserved */
    for (int i = 0; i < 32; i++)
        EXPECT_EQ(p2[i], 'Y');
}

TEST_F(HeapTest, ReallocSameSize)
{
    void *p = kmalloc(64);
    ASSERT_NE(p, nullptr);
    void *p2 = krealloc(p, 64);
    EXPECT_EQ(p2, p); /* same pointer */
}

/* === kfree_sized tests === */

TEST_F(HeapTest, FreeSizedBasic)
{
    void *p = kmalloc(64);
    ASSERT_NE(p, nullptr);
    kfree_sized(p, 64); /* should behave like kfree */
    /* No crash = success */
}

/* === Stress test === */

TEST_F(HeapTest, RandomAllocFree)
{
    constexpr int ITERATIONS = 500;
    constexpr size_t MAX_SIZE = 1024;
    constexpr int MAX_OUTSTANDING = 50;

    void *ptrs[MAX_OUTSTANDING] = {};
    size_t sizes[MAX_OUTSTANDING] = {};
    int count = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        if (count > 0 && (rand() % 3 == 0)) {
            /* Free a random allocation */
            int idx = rand() % count;
            kfree(ptrs[idx]);
            ptrs[idx] = ptrs[count - 1];
            sizes[idx] = sizes[count - 1];
            ptrs[count - 1] = nullptr;
            count--;
        } else if (count < MAX_OUTSTANDING) {
            /* Allocate */
            size_t sz = (size_t)(rand() % MAX_SIZE) + 1;
            void *p = kmalloc(sz);
            ASSERT_NE(p, nullptr) << "kmalloc(" << sz << ") failed at iteration " << i;
            EXPECT_EQ((uintptr_t)p % 16, 0u);

            /* Write a pattern */
            std::memset(p, 0xAB, sz);

            ptrs[count] = p;
            sizes[count] = sz;
            count++;
        }
    }

    /* Free remaining allocations */
    for (int i = 0; i < count; i++)
        kfree(ptrs[i]);
}

/* === Multiple alloc/free cycles === */

TEST_F(HeapTest, RepeatedCycles)
{
    for (int cycle = 0; cycle < 10; cycle++) {
        void *ptrs[20];
        for (int i = 0; i < 20; i++) {
            ptrs[i] = kmalloc(32 + i * 8);
            ASSERT_NE(ptrs[i], nullptr);
        }
        for (int i = 0; i < 20; i++)
            kfree(ptrs[i]);
    }
}

/* === Edge cases === */

TEST_F(HeapTest, VerySmallAllocation)
{
    void *p = kmalloc(1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 16, 0u);
    /* Write to the single byte */
    *(char *)p = 42;
    EXPECT_EQ(*(char *)p, 42);
    kfree(p);
}

TEST_F(HeapTest, ExactPageSizeAllocation)
{
    /* Request exactly PAGE_SIZE - METADATA_SIZE bytes.
     * This should fit in a single page. */
    void *p = kmalloc(4096 - 32);
    ASSERT_NE(p, nullptr);
    kfree(p);
}

TEST_F(HeapTest, ManySmallAllocations)
{
    constexpr int COUNT = 100;
    void *ptrs[COUNT];

    for (int i = 0; i < COUNT; i++) {
        ptrs[i] = kmalloc(16);
        ASSERT_NE(ptrs[i], nullptr) << "allocation " << i << " failed";
    }

    /* Free every other one */
    for (int i = 0; i < COUNT; i += 2) {
        kfree(ptrs[i]);
        ptrs[i] = nullptr;
    }

    /* Allocate again in the freed slots */
    for (int i = 0; i < COUNT; i += 2) {
        ptrs[i] = kmalloc(16);
        ASSERT_NE(ptrs[i], nullptr);
    }

    /* Clean up */
    for (int i = 0; i < COUNT; i++)
        kfree(ptrs[i]);
}
