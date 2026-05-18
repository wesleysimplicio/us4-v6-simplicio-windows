#include "us4/runtime/scheduler/continuous_batcher.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(ContinuousBatcherTest, EnqueueAndStepRespectMaxBatchSize)
    {
        scheduler::ContinuousBatcher batcher;
        batcher.Configure(2U, 10U);

        EXPECT_TRUE(batcher.Enqueue({.sessionId = "s1", .requestId = "r1", .promptTokens = 16U}));
        EXPECT_TRUE(batcher.Enqueue({.sessionId = "s1", .requestId = "r2", .promptTokens = 16U}));
        EXPECT_TRUE(batcher.Enqueue({.sessionId = "s2", .requestId = "r3", .promptTokens = 16U}));

        const auto step1 = batcher.Step();
        EXPECT_EQ(step1.scheduledCount, 2U);
        EXPECT_EQ(batcher.QueueDepth(), 1U);

        const auto step2 = batcher.Step();
        EXPECT_EQ(step2.scheduledCount, 1U);
        EXPECT_EQ(batcher.QueueDepth(), 0U);
    }

    TEST(ContinuousBatcherTest, MaxQueueDepthRejects)
    {
        scheduler::ContinuousBatcher batcher;
        batcher.Configure(4U, 2U);

        EXPECT_TRUE(batcher.Enqueue({.requestId = "r1"}));
        EXPECT_TRUE(batcher.Enqueue({.requestId = "r2"}));
        EXPECT_FALSE(batcher.Enqueue({.requestId = "r3"}));
    }

    TEST(ContinuousBatcherTest, HigherPriorityRunsFirst)
    {
        scheduler::ContinuousBatcher batcher;
        batcher.Configure(1U, 10U);

        EXPECT_TRUE(batcher.Enqueue({.requestId = "low", .priority = 0U}));
        EXPECT_TRUE(batcher.Enqueue({.requestId = "high", .priority = 5U}));

        const auto step = batcher.Step();
        ASSERT_EQ(step.requestIds.size(), 1U);
        EXPECT_EQ(step.requestIds.front(), "high");
    }

    TEST(ContinuousBatcherTest, CompleteAdvancesStats)
    {
        scheduler::ContinuousBatcher batcher;
        batcher.Configure(2U, 4U);
        EXPECT_TRUE(batcher.Enqueue({.requestId = "r1"}));
        (void) batcher.Step();
        batcher.Complete("r1");
        EXPECT_EQ(batcher.Stats().completeCount, 1U);
    }

} // namespace us4::runtime::tests
