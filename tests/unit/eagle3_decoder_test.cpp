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

    TEST(Eagle3DecoderTest, DecodeTracksPartialAcceptanceWindows)
    {
        speculative::Eagle3Decoder decoder;
        decoder.Configure({.treeDepth = 4U, .treeWidth = 3U});

        const auto window = decoder.Decode(
            backends::TokenChunk{.tokens = {11, 12, 90}},
            backends::TokenChunk{.tokens = {11, 12, 13}});
        const auto stats = decoder.Stats();

        EXPECT_EQ(window.acceptedTokens, 2U);
        EXPECT_EQ(window.rejectedTokens, 1U);
        EXPECT_FLOAT_EQ(window.acceptanceRate, 2.0F / 3.0F);
        EXPECT_EQ(stats.draftCount, 1U);
        EXPECT_EQ(stats.acceptedTokens, 2U);
        EXPECT_EQ(stats.rejectedTokens, 1U);
        EXPECT_GT(stats.estimatedSpeedup, 1.0F);
    }

} // namespace us4::runtime::tests
