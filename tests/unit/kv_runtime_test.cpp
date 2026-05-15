#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/runtime_context.h"
#include "us4/runtime/cache/prefix_cache.h"
#include "us4/runtime/kv/kv_pager.h"
#include "us4/runtime/kv/summarizer.h"
#include "us4/runtime/backends/runtime_types.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {

        TEST(KvRuntimeTest, PrefixCacheBuildsStableKeysAndWarmsEntries)
        {
            cache::PrefixCache prefixCache;
            const std::string key = cache::PrefixCache::BuildKey("hello runtime", 13U);

            prefixCache.Upsert(cache::PrefixCacheEntry{
                .key = key,
                .sequenceId = "seq-1",
                .promptTokens = 13U,
                .kvBytes = 4096U,
            });

            ASSERT_TRUE(prefixCache.Contains(key));
            const auto hit = prefixCache.TryGet(key);
            ASSERT_TRUE(hit.has_value());
            EXPECT_EQ(hit->sequenceId, "seq-1");
            EXPECT_EQ(hit->hitCount, 1U);
            EXPECT_EQ(prefixCache.WarmEntryCount(), 1U);
            EXPECT_EQ(prefixCache.TotalKvBytes(), 4096U);
        }

        TEST(KvRuntimeTest, KvPagerRebalancesAcrossHostStorageAndSummary)
        {
            kv::KvPager pager;
            pager.ConfigureBudget(kv::KvPagerBudget{
                .deviceBytes = 0U,
                .hostBytes = 128U,
                .storageBytes = 96U,
                .summaryBytes = 32U,
            });

            EXPECT_TRUE(pager.Append("seq-a", 16U, 80U, kv::KvTier::kHost, true));
            EXPECT_TRUE(pager.Append("seq-b", 16U, 80U, kv::KvTier::kHost));
            EXPECT_TRUE(pager.Append("seq-c", 16U, 80U, kv::KvTier::kHost));
            EXPECT_TRUE(pager.Touch("seq-a").has_value());
            EXPECT_TRUE(pager.Touch("seq-a").has_value());
            EXPECT_TRUE(pager.Touch("seq-b").has_value());

            const kv::KvPagerStats stats = pager.Stats();

            EXPECT_EQ(stats.segmentCount, 3U);
            EXPECT_EQ(stats.pinnedSegmentCount, 1U);
            EXPECT_GT(stats.storageBytes + stats.summaryBytes, 0U);
            EXPECT_GT(stats.evictionCount, 0U);
            EXPECT_GE(stats.summarizeCount, 1U);
        }

        TEST(KvRuntimeTest, SummarizerKeepsHeadAndTailWindows)
        {
            const std::vector<std::int32_t> tokens = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            const auto summary =
                kv::Summarizer::Compact(tokens, kv::SummaryWindow{.headTokens = 3U, .tailTokens = 2U});

            EXPECT_EQ(summary.retainedTokens,
                      std::vector<std::int32_t>({1, 2, 3, 9, 10}));
            EXPECT_EQ(summary.droppedTokens, 5U);
            EXPECT_EQ(summary.estimatedBytes, summary.retainedTokens.size() * sizeof(std::int32_t));
        }

        TEST(KvRuntimeTest, CpuScalarRunRecordsKvAndCacheTelemetry)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 6U;

            const us4::core::RuntimePlan plan = us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "kv runtime check");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(runResult.report.kvStats.segmentCount, 1U);
            EXPECT_GE(runResult.report.kvStats.hostBytes, plan.residency.kvBytesPerToken);
            EXPECT_EQ(runResult.report.prefixCacheEntries, 1U);
            EXPECT_EQ(runResult.report.prefixCacheWarmEntries, 1U);
            EXPECT_GE(runResult.report.telemetryEventCount, 3U);
        }

    } // namespace
} // namespace us4::runtime::tests
