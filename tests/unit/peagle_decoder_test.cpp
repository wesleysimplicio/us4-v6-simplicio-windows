#include "us4/runtime/speculative/peagle_decoder.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(PEagleDecoderTest, ConfigureDoesNotMutateStats)
    {
        speculative::PEagleDecoder decoder;
        decoder.Configure({.draftDepth = 4U, .acceptanceThreshold = 0.6F, .useLayerPruning = true});
        EXPECT_EQ(decoder.Stats().draftCount, 0U);
    }

    TEST(PEagleDecoderTest, ResetClearsCounters)
    {
        speculative::PEagleDecoder decoder;
        decoder.Configure({.draftDepth = 2U, .acceptanceThreshold = 0.5F});
        decoder.Reset();
        const auto stats = decoder.Stats();
        EXPECT_EQ(stats.acceptedTokens, 0U);
        EXPECT_EQ(stats.rejectedTokens, 0U);
    }

    TEST(PEagleDecoderTest, DecodeTracksPartialAcceptanceWindows)
    {
        speculative::PEagleDecoder decoder;
        decoder.Configure({.draftDepth = 4U, .acceptanceThreshold = 0.6F});

        const auto window = decoder.Decode(
            backends::TokenChunk{.tokens = {21, 22, 70}},
            backends::TokenChunk{.tokens = {21, 22, 23}});
        const auto stats = decoder.Stats();

        EXPECT_EQ(window.acceptedTokens, 2U);
        EXPECT_EQ(window.rejectedTokens, 1U);
        EXPECT_FLOAT_EQ(window.acceptanceRate, 2.0F / 3.0F);
        EXPECT_EQ(stats.draftCount, 1U);
        EXPECT_EQ(stats.acceptedTokens, 2U);
        EXPECT_EQ(stats.rejectedTokens, 1U);
        EXPECT_FLOAT_EQ(stats.averageAcceptanceRate, 2.0F / 3.0F);
    }

} // namespace us4::runtime::tests
