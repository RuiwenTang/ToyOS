/*
 * test_atomic.cpp — Host-side unit tests for kernel/atomic.h
 *
 * atomic.h is header-only (static inline, hand-written lock-prefix
 * asm), so this links nothing but gtest. gtest pulls in <stdint.h>
 * first, so the manual typedefs in kernel types.h are skipped on host.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include <toyos/kernel/atomic.h>
}

TEST(AtomicInc, SingleThreadCounter)
{
    uint32_t v = 0;
    for (int i = 0; i < 1000; i++)
        atomic_inc(&v);
    EXPECT_EQ(v, 1000u);
}

TEST(AtomicDec, SingleThreadCounter)
{
    uint32_t v = 1000;
    for (int i = 0; i < 1000; i++)
        atomic_dec(&v);
    EXPECT_EQ(v, 0u);
}

TEST(AtomicAddReturn, ReturnsNewValue)
{
    uint32_t v = 10;
    EXPECT_EQ(atomic_add_return(&v, 5), 15u);
    EXPECT_EQ(v, 15u);
    EXPECT_EQ(atomic_add_return(&v, 0), 15u);
    EXPECT_EQ(atomic_add_return(&v, 100), 115u);
    EXPECT_EQ(v, 115u);
}

TEST(AtomicCas, StrongSuccessAndFailure)
{
    uint32_t v = 42;
    EXPECT_TRUE(atomic_cas(&v, 42, 7)); /* success */
    EXPECT_EQ(v, 7u);
    EXPECT_FALSE(atomic_cas(&v, 42, 8)); /* failure: *p != old */
    EXPECT_EQ(v, 7u);                    /* unchanged on failure */
    EXPECT_TRUE(atomic_cas(&v, 7, 99));
    EXPECT_EQ(v, 99u);
}

TEST(AtomicInc, MultiThreadTotal)
{
    const int N = 8;
    const int M = 100000;
    uint32_t v = 0;
    std::vector<std::thread> ts;
    for (int i = 0; i < N; i++)
        ts.emplace_back([&v]() {
            for (int j = 0; j < M; j++)
                atomic_inc(&v);
        });
    for (auto &t : ts)
        t.join();
    EXPECT_EQ(v, (uint32_t)(N * M));
}

TEST(AtomicCas, ConcurrentOnlyOneWins)
{
    /* N threads all CAS 0->1 on the same slot; exactly one wins. */
    const int N = 8;
    uint32_t v = 0;
    std::atomic<int> winners{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < N; i++)
        ts.emplace_back([&v, &winners]() {
            if (atomic_cas(&v, 0, 1))
                winners.fetch_add(1, std::memory_order_relaxed);
        });
    for (auto &t : ts)
        t.join();
    EXPECT_EQ(v, 1u);
    EXPECT_EQ(winners.load(), 1);
}
