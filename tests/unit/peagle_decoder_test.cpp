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

} // namespace us4::runtime::tests
