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
                .hotBytes = 2048U,
                .warmBytes = 2048U,
                .coldBytes = 4096U,
            });

            EXPECT_TRUE(pager.Touch(1U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(2U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(3U, 1536U, moe::ExpertResidency::kWarm));

            const auto stats = pager.Stats();
            EXPECT_EQ(stats.hotExperts, 1U);
            EXPECT_EQ(stats.warmExperts, 1U);
            EXPECT_EQ(stats.coldExperts, 1U);
            EXPECT_GT(stats.evictionCount, 0U);
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
            EXPECT_GT(runResult.report.telemetryEventCount, 3U);
        }

    } // namespace
} // namespace us4::runtime::tests
