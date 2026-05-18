#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/runtime_context.h"
#include "us4/runtime/moe/expert_pager.h"
#include "us4/runtime/moe/expert_router.h"
#include "us4/runtime/backends/runtime_types.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {

        TEST(MoeRuntimeTest, ExpertRouterBuildsTopKPlanAndEntropy)
        {
            moe::ExpertRouter router;
            const auto routes = router.BuildPlan(
                "deepseek route window",
                moe::RoutingPolicy{
                    .topK = 3U,
                    .preferDeterministic = true,
                    .allowCrossDeviceExperts = true,
                });
            const auto stats = router.Evaluate(routes);

            ASSERT_EQ(routes.size(), 3U);
            EXPECT_EQ(stats.hostRouteCount, 1U);
            EXPECT_EQ(stats.deviceRouteCount, 2U);
            EXPECT_GT(stats.entropy, 0.0F);
            EXPECT_GT(stats.loadBalanceLoss, 0.0F);
        }

        TEST(MoeRuntimeTest, ExpertPagerRebalancesHotToWarmAndCold)
        {
            moe::ExpertPager pager;
            pager.ConfigureBudget(moe::ExpertPagerBudget{
                .hotBytes = 1536U,
                .warmBytes = 1536U,
                .coldBytes = 4096U,
            });

            EXPECT_TRUE(pager.Touch(1U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(2U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(3U, 1536U, moe::ExpertResidency::kHot));

            const auto stats = pager.Stats();
            EXPECT_EQ(stats.hotExperts, 1U);
            EXPECT_EQ(stats.warmExperts, 1U);
            EXPECT_EQ(stats.coldExperts, 1U);
            EXPECT_GT(stats.evictionCount, 0U);
            EXPECT_GT(stats.coldOffloadCount, 0U);

            const auto coldEntry = pager.Find(1U);
            ASSERT_TRUE(coldEntry.has_value());
            EXPECT_EQ(coldEntry->residency, moe::ExpertResidency::kCold);
            EXPECT_TRUE(coldEntry->offloaded);
        }

        TEST(MoeRuntimeTest, ExpertPagerReloadsColdExpertsWithoutChangingPayloadChecksum)
        {
            moe::ExpertPager pager;
            pager.ConfigureBudget(moe::ExpertPagerBudget{
                .hotBytes = 1536U,
                .warmBytes = 1536U,
                .coldBytes = 4096U,
            });

            EXPECT_TRUE(pager.Touch(1U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(2U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(3U, 1536U, moe::ExpertResidency::kHot));

            const auto beforeReload = pager.Find(1U);
            ASSERT_TRUE(beforeReload.has_value());
            ASSERT_TRUE(beforeReload->offloaded);
            const auto expectedChecksum = beforeReload->payloadChecksum;
            const auto expectedResidentBytes = beforeReload->residentBytes;

            EXPECT_TRUE(pager.Reload(1U, moe::ExpertResidency::kHot));

            const auto afterReload = pager.Find(1U);
            ASSERT_TRUE(afterReload.has_value());
            EXPECT_FALSE(afterReload->offloaded);
            EXPECT_EQ(afterReload->payloadChecksum, expectedChecksum);
            EXPECT_EQ(afterReload->residentBytes, expectedResidentBytes);
            EXPECT_GT(afterReload->reloadCount, 0U);

            const auto stats = pager.Stats();
            EXPECT_GT(stats.reloadCount, 0U);
            EXPECT_GT(stats.coldOffloadCount, 0U);
        }

        TEST(MoeRuntimeTest, CpuScalarRunServesDeepSeekMoeScaffold)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "deepseek-r1-distill";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 4U;

            const us4::core::RuntimePlan plan = us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "deepseek moe check");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(plan.model.adapterId, "moe-deepseek");
            EXPECT_GT(runResult.report.moeRouteCount, 0U);
            EXPECT_GT(runResult.report.moeRouterEntropy, 0.0F);
            EXPECT_GT(runResult.report.moeLoadBalanceLoss, 0.0F);
            EXPECT_GT(runResult.report.telemetryEventCount, 3U);
        }

        TEST(MoeRuntimeTest, CpuScalarRunServesKimiMoeScaffold)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "kimi-k2";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 4U;

            const us4::core::RuntimePlan plan =
                us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "kimi moe check");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(plan.model.adapterId, "moe-kimi");
            EXPECT_GT(runResult.report.moeRouteCount, 0U);
            EXPECT_GT(runResult.report.moeRouterEntropy, 0.0F);
            EXPECT_GT(runResult.report.moeLoadBalanceLoss, 0.0F);
            EXPECT_GT(runResult.report.moePrefetchTelemetry.hitRatio, 0.0F);
            EXPECT_GT(runResult.report.telemetryEventCount, 3U);
        }

    } // namespace
} // namespace us4::runtime::tests
