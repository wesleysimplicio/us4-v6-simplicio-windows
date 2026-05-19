#include "us4/runtime/backends/cpu_avx/amx_dispatcher.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(AmxDispatcherTest, ToStringCoversAllVariants)
    {
        EXPECT_EQ(backends::cpu_avx::ToString(backends::cpu_avx::AmxPreferredIsa::kScalar), "scalar");
        EXPECT_EQ(backends::cpu_avx::ToString(backends::cpu_avx::AmxPreferredIsa::kAvx2), "avx2");
        EXPECT_EQ(backends::cpu_avx::ToString(backends::cpu_avx::AmxPreferredIsa::kAvx512), "avx512");
        EXPECT_EQ(backends::cpu_avx::ToString(backends::cpu_avx::AmxPreferredIsa::kAmx), "amx");
    }

    TEST(AmxDispatcherTest, FallbackPlanPicksAvx512WhenAmxAbsent)
    {
        const auto cap = backends::cpu_avx::DetectAmxCapability();
        const auto plan = backends::cpu_avx::PlanAmxOrAvx512Fallback();
        if (cap.tileLoad && cap.dpbf16ps)
        {
            EXPECT_EQ(plan.selectedIsa, backends::cpu_avx::AmxPreferredIsa::kAmx);
        }
        else
        {
            EXPECT_EQ(plan.selectedIsa, backends::cpu_avx::AmxPreferredIsa::kAvx512);
        }
        EXPECT_GT(plan.tileBytes, 0U);
        EXPECT_FALSE(plan.rationale.empty());
    }

} // namespace us4::runtime::tests
