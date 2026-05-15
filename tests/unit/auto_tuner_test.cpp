#include "us4/runtime/tuning/auto_tuner.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        TEST(AutoTunerTest, BuildsCpuOnlyProbePlanForCpuHosts)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";

            tuning::AutoTuner tuner;
            const auto plan = tuner.BuildPlan(request, capabilities);

            ASSERT_FALSE(plan.decisions.empty());
            EXPECT_TRUE(std::any_of(
                plan.decisions.begin(), plan.decisions.end(),
                [](const tuning::TuningDecision& decision)
                { return decision.key == "recommended-profile" && decision.value == "cpu-only"; }));
            EXPECT_TRUE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                    [](const tuning::TuningProbe& probe)
                                    {
                                        return probe.benchmarkName ==
                                                   "dense_baseline_qwen_cpu_only" &&
                                               probe.backend == "cpu" && probe.regressionCritical;
                                    }));
        }

        TEST(AutoTunerTest, BuildsHybridProbePlanForVulkanAndWinMlHosts)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.hasNpu = true;
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.deviceBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kMicro;
            request.preferredBackend = "windows-ml";
            request.allowNpu = true;
            request.enableSpeculative = true;

            tuning::AutoTuner tuner;
            const auto plan = tuner.BuildPlan(request, capabilities);

            EXPECT_TRUE(std::any_of(
                plan.decisions.begin(), plan.decisions.end(),
                [](const tuning::TuningDecision& decision)
                { return decision.key == "recommended-profile" && decision.value == "micro"; }));
            EXPECT_TRUE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                    [](const tuning::TuningProbe& probe)
                                    { return probe.benchmarkName == "vulkan_qwen_balanced"; }));
            EXPECT_TRUE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                    [](const tuning::TuningProbe& probe)
                                    { return probe.benchmarkName == "windows_ml_qwen_opt_in"; }));
        }

        TEST(AutoTunerTest, PreservesNoNpuFallbackRegressionProbeForExplicitWinMlRequests)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kMicro;
            request.preferredBackend = "windows-ml";
            request.allowNpu = true;

            tuning::AutoTuner tuner;
            const auto plan = tuner.BuildPlan(request, capabilities);

            EXPECT_TRUE(std::any_of(plan.decisions.begin(), plan.decisions.end(),
                                    [](const tuning::TuningDecision& decision)
                                    {
                                        return decision.key == "npu-fallback-regression" &&
                                               decision.value == "enabled";
                                    }));
            EXPECT_TRUE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                    [](const tuning::TuningProbe& probe)
                                    {
                                        return probe.benchmarkName ==
                                                   "windows_ml_qwen_no_npu_fallback" &&
                                               probe.backend == "windows-ml" &&
                                               probe.regressionCritical;
                                    }));
            EXPECT_FALSE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                     [](const tuning::TuningProbe& probe)
                                     { return probe.benchmarkName == "windows_ml_qwen_opt_in"; }));
        }
    } // namespace
} // namespace us4::runtime::tests
