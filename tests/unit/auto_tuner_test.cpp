#include "runtime/core/runtime_context.h"
#include "us4/runtime/benchmarks/matrix_runner.h"
#include "us4/runtime/tuning/auto_tuner.h"
#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        void ClearProfileStoreEnv()
        {
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", "");
#else
            unsetenv("US4_PROFILE_STORE_PATH");
#endif
        }

        TEST(AutoTunerTest, BuildsCpuOnlyProbePlanForCpuHosts)
        {
            ClearProfileStoreEnv();
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
            EXPECT_TRUE(std::any_of(
                plan.decisions.begin(), plan.decisions.end(),
                [](const tuning::TuningDecision& decision)
                { return decision.key == "profile-store" && decision.value == "miss"; }));
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
            ClearProfileStoreEnv();
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
            ClearProfileStoreEnv();
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

        TEST(AutoTunerTest, ProfileStoreRoundTripsEntriesByHardwareFingerprint)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-roundtrip";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";

            backends::HardwareCapabilities capabilities{};
            capabilities.cpuName = "Test CPU";
            capabilities.primaryAdapterName = "Radeon RX Test";
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;

            tuning::ProfileStore store(storePath);
            ASSERT_TRUE(store.SaveProfileId(capabilities, "micro"));

            const auto entries = store.LoadEntries();
            ASSERT_EQ(entries.size(), 1U);
            EXPECT_EQ(entries.front().profileId, "micro");
            EXPECT_EQ(store.LoadProfileId(capabilities), std::optional<std::string>("micro"));
        }

        TEST(AutoTunerTest, ProfileStoreFingerprintChangesWithHardwareBudget)
        {
            backends::HardwareCapabilities first{};
            first.cpuName = "Test CPU";
            first.primaryAdapterName = "Arc Test";
            first.primaryAdapterVendor = backends::BackendVendor::kIntel;
            first.primaryAdapterClass = backends::DeviceClass::kIntegratedGpu;
            first.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
            first.budget.deviceBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;

            auto second = first;
            second.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;

            EXPECT_NE(tuning::ProfileStore::BuildHardwareFingerprint(first),
                      tuning::ProfileStore::BuildHardwareFingerprint(second));
        }

        TEST(AutoTunerTest, AutoTunerUsesStoredProfileWhenCacheExists)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-autotuner";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";

            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.cpuName = "Test CPU";
            capabilities.primaryAdapterName = "Radeon RX Test";
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;

            tuning::ProfileStore store(storePath);
            ASSERT_TRUE(store.SaveProfileId(capabilities, "nano"));

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kBalanced;

            tuning::AutoTuner tuner(storePath);
            const auto plan = tuner.BuildPlan(request, capabilities);

            EXPECT_TRUE(std::any_of(
                plan.decisions.begin(), plan.decisions.end(),
                [](const tuning::TuningDecision& decision)
                { return decision.key == "recommended-profile" && decision.value == "nano"; }));
            EXPECT_TRUE(std::any_of(
                plan.decisions.begin(), plan.decisions.end(),
                [](const tuning::TuningDecision& decision)
                { return decision.key == "profile-store" && decision.value == "hit"; }));
        }

        TEST(AutoTunerTest, RuntimeContextUsesStoredProfileBeforeCatalogRecommendation)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-runtime-context";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", storePath.string().c_str());
#endif

            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.cpuName = "Test CPU";
            capabilities.primaryAdapterName = "Radeon RX Test";
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;

            tuning::ProfileStore store(storePath);
            ASSERT_TRUE(store.SaveProfileId(capabilities, "nano"));

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kBalanced;

            const auto plan = core::RuntimeContext::BuildPlan(request, capabilities);

            EXPECT_EQ(plan.profile.id, "nano");
            EXPECT_EQ(plan.mode, backends::RuntimeMode::kNano);
            EXPECT_TRUE(std::any_of(plan.issues.begin(), plan.issues.end(),
                                    [](const backends::RuntimeIssue& issue)
                                    { return issue.code == "profile-store-hit"; }));

            ClearProfileStoreEnv();
        }

        TEST(AutoTunerTest, MatrixRunnerPersistsCpuOnlyWinnerForCpuHosts)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-matrix-cpu";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";

            backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.cpuName = "Test CPU";
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";

            runtime::benchmarks::MatrixRunner runner(storePath);
            const auto report = runner.Tune(request, capabilities);

            EXPECT_EQ(report.selectedProfileId, "cpu-only");
            EXPECT_EQ(report.selectedBackend, "cpu");
            EXPECT_TRUE(report.persisted);
            ASSERT_FALSE(report.samples.empty());
            EXPECT_EQ(report.samples.front().benchmarkName, "dense_baseline_qwen_cpu_only");

            tuning::ProfileStore store(storePath);
            EXPECT_EQ(store.LoadProfileId(capabilities), std::optional<std::string>("cpu-only"));
        }

        TEST(AutoTunerTest, MatrixRunnerPrefersWinMlMicroWhenNpuIsAvailable)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-matrix-winml";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";

            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.hasNpu = true;
            capabilities.npuCount = 1;
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.deviceBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kBalanced;
            request.preferredBackend = "windows-ml";
            request.allowNpu = true;

            runtime::benchmarks::MatrixRunner runner(storePath);
            const auto report = runner.Tune(request, capabilities);

            EXPECT_EQ(report.selectedProfileId, "micro");
            EXPECT_EQ(report.selectedBackend, "windows-ml");
            EXPECT_TRUE(report.persisted);
            EXPECT_TRUE(std::any_of(
                report.samples.begin(), report.samples.end(),
                [](const runtime::benchmarks::MatrixSample& sample)
                { return sample.benchmarkName == "windows_ml_qwen_opt_in" && sample.supported; }));
        }
    } // namespace
} // namespace us4::runtime::tests
