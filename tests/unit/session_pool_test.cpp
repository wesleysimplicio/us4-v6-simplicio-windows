#include "us4/runtime/scheduler/session_pool.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(SessionPoolTest, AcquireReturnsIsolatedNamespace)
    {
        scheduler::SessionPool pool;
        pool.Configure(4U, 1024U * 1024U);

        const auto ns1 = pool.Acquire("session-1");
        const auto ns2 = pool.Acquire("session-2");
        ASSERT_TRUE(ns1.has_value());
        ASSERT_TRUE(ns2.has_value());
        EXPECT_NE(*ns1, *ns2);
    }

    TEST(SessionPoolTest, AcquireRejectsBeyondCapacity)
    {
        scheduler::SessionPool pool;
        pool.Configure(2U, 1024U);

        EXPECT_TRUE(pool.Acquire("a").has_value());
        EXPECT_TRUE(pool.Acquire("b").has_value());
        EXPECT_FALSE(pool.Acquire("c").has_value());
    }

    TEST(SessionPoolTest, TouchUpdatesGeneratedTokens)
    {
        scheduler::SessionPool pool;
        pool.Configure(4U, 1024U);
        (void) pool.Acquire("session-1");
        EXPECT_TRUE(pool.Touch("session-1", 32U, 2048U));
        const auto record = pool.Find("session-1");
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->generatedTokens, 32U);
        EXPECT_EQ(record->kvBytes, 2048U);
    }

    TEST(SessionPoolTest, ReleaseRemovesSession)
    {
        scheduler::SessionPool pool;
        pool.Configure(4U, 1024U);
        (void) pool.Acquire("session-1");
        EXPECT_TRUE(pool.Release("session-1"));
        EXPECT_FALSE(pool.Find("session-1").has_value());
    }

} // namespace us4::runtime::tests
