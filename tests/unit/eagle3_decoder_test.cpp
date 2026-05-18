#include "us4/runtime/speculative/eagle3_decoder.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(Eagle3DecoderTest, ConfigureKeepsCountersZero)
    {
        speculative::Eagle3Decoder decoder;
        decoder.Configure({.treeDepth = 5U, .treeWidth = 4U, .acceptanceThreshold = 0.7F});
        const auto stats = decoder.Stats();
        EXPECT_EQ(stats.draftCount, 0U);
        EXPECT_FLOAT_EQ(stats.estimatedSpeedup, 1.0F);
    }

    TEST(Eagle3DecoderTest, ResetClearsCounters)
    {
        speculative::Eagle3Decoder decoder;
        decoder.Configure({.treeDepth = 3U, .treeWidth = 2U});
        decoder.Reset();
        const auto stats = decoder.Stats();
        EXPECT_EQ(stats.acceptedTokens, 0U);
        EXPECT_EQ(stats.rejectedTokens, 0U);
        EXPECT_EQ(stats.maxTreeWidthObserved, 0U);
    }

} // namespace us4::runtime::tests
