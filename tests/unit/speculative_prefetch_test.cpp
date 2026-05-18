#include "us4/runtime/moe/speculative_prefetch.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(SpeculativePrefetchTest, ObserveBuildsFrequencyTable)
    {
        moe::SpeculativePrefetch prefetch;
        prefetch.Configure(8U, 2U);
        prefetch.Observe(1U);
        prefetch.Observe(2U);
        prefetch.Observe(1U);
        prefetch.Observe(3U);

        const auto stats = prefetch.Stats();
        EXPECT_EQ(stats.observationCount, 4U);

        const auto predicted = prefetch.PredictNext();
        ASSERT_FALSE(predicted.empty());
        EXPECT_EQ(predicted.front(), 1U);
    }

    TEST(SpeculativePrefetchTest, HistoryWindowEvictsStaleObservations)
    {
        moe::SpeculativePrefetch prefetch;
        prefetch.Configure(2U, 3U);
        prefetch.Observe(1U);
        prefetch.Observe(2U);
        prefetch.Observe(3U);

        const auto predicted = prefetch.PredictNext();
        EXPECT_LE(predicted.size(), 2U);
    }

    TEST(SpeculativePrefetchTest, OutcomeUpdatesHitRatio)
    {
        moe::SpeculativePrefetch prefetch;
        prefetch.Configure(4U, 2U);
        prefetch.RecordPrefetchOutcome(0U, true);
        prefetch.RecordPrefetchOutcome(1U, true);
        prefetch.RecordPrefetchOutcome(2U, false);

        const auto stats = prefetch.Stats();
        EXPECT_NEAR(stats.hitRatio, 2.0F / 3.0F, 1e-5F);
    }

} // namespace us4::runtime::tests
