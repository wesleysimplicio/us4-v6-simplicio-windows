#include "us4/runtime/cache/sparsity_aware_cache.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(SparsityAwareCacheTest, UpsertAndHitIncrementCounters)
    {
        cache::SparsityAwareCache cache;
        cache.Configure(1024U, 0.5F);

        cache::SparsityCacheEntry entry{};
        entry.key = "layer-7";
        entry.activeChannels = 256U;
        entry.totalChannels = 1024U;
        entry.residentBytes = 4096U;
        cache.Upsert(entry);

        EXPECT_TRUE(cache.Contains("layer-7"));
        const auto hit = cache.TryGet("layer-7");
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->hitCount, 1U);

        const auto stats = cache.Stats();
        EXPECT_EQ(stats.entryCount, 1U);
        EXPECT_EQ(stats.hitCount, 1U);
        EXPECT_EQ(stats.residentBytes, 4096U);
        EXPECT_GT(stats.averageSparsity, 0.0F);
    }

    TEST(SparsityAwareCacheTest, EvictBelowThresholdRemovesDenseEntries)
    {
        cache::SparsityAwareCache cache;
        cache.Configure(1024U, 0.75F);

        cache::SparsityCacheEntry dense{};
        dense.key = "dense";
        dense.activeChannels = 900U;
        dense.totalChannels = 1024U;
        cache.Upsert(dense);

        cache::SparsityCacheEntry sparse{};
        sparse.key = "sparse";
        sparse.activeChannels = 100U;
        sparse.totalChannels = 1024U;
        cache.Upsert(sparse);

        const auto evicted = cache.EvictBelowThreshold();
        EXPECT_EQ(evicted, 1U);
        EXPECT_FALSE(cache.Contains("dense"));
        EXPECT_TRUE(cache.Contains("sparse"));
    }

    TEST(SparsityAwareCacheTest, MissOnUnknownKeyIncrementsCounter)
    {
        cache::SparsityAwareCache cache;
        cache.Configure(1024U, 0.5F);

        const auto miss = cache.TryGet("absent");
        EXPECT_FALSE(miss.has_value());
        EXPECT_EQ(cache.Stats().missCount, 1U);
    }

} // namespace us4::runtime::tests
