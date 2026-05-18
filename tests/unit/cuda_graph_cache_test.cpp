#include "us4/runtime/backends/cuda/cuda_graph_cache.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(CudaGraphCacheTest, LookupMissesBeforeRecord)
    {
        backends::cuda::CudaGraphCache cache;
        cache.Configure(8U);
        backends::cuda::GraphSignature sig{};
        sig.adapterId = "qwen-0.5b";
        sig.stage = "decode";
        sig.batchSize = 1U;
        sig.sequenceLength = 32U;
        sig.kvLength = 32U;
        EXPECT_FALSE(cache.Lookup(sig));
        EXPECT_EQ(cache.Stats().missCount, 1U);
    }

    TEST(CudaGraphCacheTest, RecordEnablesHitOnSecondLookup)
    {
        backends::cuda::CudaGraphCache cache;
        cache.Configure(4U);
        backends::cuda::GraphSignature sig{};
        sig.adapterId = "qwen-0.5b";
        sig.stage = "decode";
        sig.batchSize = 2U;
        sig.sequenceLength = 64U;
        cache.Record(sig);
        EXPECT_TRUE(cache.Lookup(sig));
        const auto stats = cache.Stats();
        EXPECT_EQ(stats.hitCount, 1U);
        EXPECT_EQ(stats.reuseCount, 1U);
        EXPECT_GT(stats.hitRatio, 0.0F);
    }

    TEST(CudaGraphCacheTest, EvictsOldestWhenAtCapacity)
    {
        backends::cuda::CudaGraphCache cache;
        cache.Configure(2U);
        for (std::size_t i = 0; i < 3; ++i)
        {
            backends::cuda::GraphSignature sig{};
            sig.adapterId = "qwen-0.5b";
            sig.stage = "decode";
            sig.batchSize = i + 1U;
            cache.Record(sig);
        }
        EXPECT_EQ(cache.Stats().entryCount, 2U);
        EXPECT_EQ(cache.Stats().evictionCount, 1U);
    }

} // namespace us4::runtime::tests
