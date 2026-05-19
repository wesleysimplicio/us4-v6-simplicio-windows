#include "runtime/core/runtime_context.h"
#include "us4/runtime/benchmarks/matrix_runner.h"
#include "us4/runtime/tuning/auto_tuner.h"
#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>

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

        std::size_t CountSubstring(const std::string& haystack, const std::string& needle)
        {
            std::size_t count = 0U;
            std::size_t offset = 0U;
            while ((offset = haystack.find(needle, offset)) != std::string::npos)
            {
                ++count;
                offset += needle.size();
            }
            return count;
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

        TEST(AutoTunerTest, BuildsLlamaHybridProbePlanForVulkanAndWinMlHosts)
        {
            ClearProfileStoreEnv();
            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.hasNpu = true;
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.npuVendor = backends::BackendVendor::kMicrosoft;
            capabilities.budget.deviceBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "llama-3.2-3b";
            request.mode = backends::RuntimeMode::kBalanced;
            request.preferredBackend = "windows-ml";
            request.allowNpu = true;
            request.enableSpeculative = true;

            tuning::AutoTuner tuner;
            const auto plan = tuner.BuildPlan(request, capabilities);

            EXPECT_TRUE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                    [](const tuning::TuningProbe& probe)
                                    { return probe.benchmarkName == "vulkan_llama_balanced"; }));
            EXPECT_TRUE(std::any_of(plan.probes.begin(), plan.probes.end(),
                                    [](const tuning::TuningProbe& probe)
                                    { return probe.benchmarkName == "windows_ml_llama_opt_in"; }));
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

        TEST(AutoTunerTest, MatrixRunnerBenchJsonCapturesUnpersistedMatrix)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-matrix-json";
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
            const auto report = runner.Benchmark(request, capabilities);
            const auto json = runtime::benchmarks::MatrixRunner::RenderJson(report);

            EXPECT_FALSE(report.persisted);
            EXPECT_NE(json.find("\"execution\": \"bench\""), std::string::npos);
            EXPECT_NE(json.find("\"selected_profile\": \"cpu-only\""), std::string::npos);
            EXPECT_NE(json.find("\"persisted\": false"), std::string::npos);
            EXPECT_NE(json.find("\"benchmark\":\"dense_baseline_qwen_cpu_only\""),
                      std::string::npos);

            tuning::ProfileStore store(storePath);
            EXPECT_EQ(store.LoadProfileId(capabilities), std::nullopt);
        }

        TEST(AutoTunerTest, MatrixRunnerFallsBackWhenPlanContainsUnregisteredAndUnsupportedProbes)
        {
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-matrix-fallback";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";

            backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.cpuName = "Fallback CPU";
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kBalanced;
            request.preferredBackend = "directml";
            request.allowNpu = true;

            tuning::TuningPlan plan{};
            plan.decisions = {
                {
                    .key = "requested-profile",
                    .value = "balanced",
                    .rationale = "User asked for a balanced default.",
                },
                {
                    .key = "recommended-profile",
                    .value = "micro",
                    .rationale = "Planner recommends micro for fallback coverage.",
                },
            };
            plan.probes = {
                {
                    .benchmarkName = "not_registered_yet",
                    .backend = "cpu",
                    .profileId = "balanced",
                    .promptTokens = 64U,
                    .generationTokens = 16U,
                    .regressionCritical = false,
                    .rationale = "Intentional missing catalog entry.",
                },
                {
                    .benchmarkName = "windows_ml_qwen_opt_in",
                    .backend = "windows-ml",
                    .profileId = "micro",
                    .promptTokens = 256U,
                    .generationTokens = 32U,
                    .regressionCritical = true,
                    .rationale = "Requires NPU support that this host does not expose.",
                },
            };

            runtime::benchmarks::MatrixRunner runner(storePath);
            const auto report = runner.ExecutePlan(plan, request, capabilities);

            EXPECT_EQ(report.selectedProfileId, "micro");
            EXPECT_EQ(report.selectedBackend, "directml");
            EXPECT_TRUE(report.persisted);
            ASSERT_EQ(report.samples.size(), 2U);
            EXPECT_FALSE(report.samples[0].supported);
            EXPECT_EQ(report.samples[0].rationale,
                      "Probe is not registered in the benchmark catalog yet.");
            EXPECT_FALSE(report.samples[1].supported);
            EXPECT_EQ(report.samples[1].rationale,
                      "Hardware capabilities do not satisfy the backend requirements for this "
                      "probe.");

            tuning::ProfileStore store(storePath);
            EXPECT_EQ(store.LoadProfileId(capabilities), std::optional<std::string>("micro"));
        }

        TEST(AutoTunerTest, MatrixRunnerPrefersRecommendedProfileWhenScoresTie)
        {
            constexpr std::uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-profile-store-matrix-tie";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";

            backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.cpuName = "Tie Break CPU";
            capabilities.budget.hostBytes = (1000ULL * kGiB) / 3ULL;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = backends::RuntimeMode::kBalanced;
            request.preferredBackend = "windows-ml";
            request.allowNpu = true;

            tuning::TuningPlan plan{};
            plan.decisions = {
                {
                    .key = "requested-profile",
                    .value = "cpu-only",
                    .rationale = "Existing cache asked for CPU-only.",
                },
                {
                    .key = "recommended-profile",
                    .value = "micro",
                    .rationale = "Planner recommends the micro profile for fallback.",
                },
            };
            plan.probes = {
                {
                    .benchmarkName = "dense_baseline_qwen_cpu_only",
                    .backend = "cpu",
                    .profileId = "cpu-only",
                    .promptTokens = 128U,
                    .generationTokens = 32U,
                    .regressionCritical = false,
                    .rationale = "CPU baseline candidate.",
                },
                {
                    .benchmarkName = "windows_ml_qwen_no_npu_fallback",
                    .backend = "windows-ml",
                    .profileId = "micro",
                    .promptTokens = 256U,
                    .generationTokens = 32U,
                    .regressionCritical = false,
                    .rationale = "Windows ML fallback candidate.",
                },
            };

            runtime::benchmarks::MatrixRunner runner(storePath);
            const auto report = runner.ExecutePlan(plan, request, capabilities);

            EXPECT_EQ(report.selectedProfileId, "micro");
            EXPECT_EQ(report.selectedBackend, "windows-ml");
            ASSERT_EQ(report.samples.size(), 2U);
            EXPECT_NEAR(report.samples[0].score, report.samples[1].score, 0.001);
        }

        TEST(AutoTunerTest, MatrixRunnerRenderJsonPreservesStructureAndEscapesText)
        {
            runtime::benchmarks::MatrixTuneReport report{};
            report.storePath = std::filesystem::temp_directory_path() / "us4-profile-store-json";
            report.requestedProfileId = "balanced";
            report.recommendedProfileId = "micro";
            report.selectedProfileId = "micro";
            report.selectedBackend = "windows-ml";
            report.selectedScore = 188.25;
            report.persisted = true;
            report.plan.decisions.push_back({
                .key = "recommended-profile",
                .value = "micro",
                .rationale = "Prefer \"micro\" for line\nwrapped output.",
            });
            report.samples.push_back({
                .benchmarkName = "windows_ml_qwen_opt_in",
                .backend = "windows-ml",
                .profileId = "micro",
                .score = 188.25,
                .supported = true,
                .regressionCritical = true,
                .rationale = "Escapes backslash \\ and\ttab characters.",
            });

            const auto json = runtime::benchmarks::MatrixRunner::RenderJson(report, "tune");

            EXPECT_NE(json.find("\"execution\": \"tune\""), std::string::npos);
            EXPECT_NE(json.find("\"persisted\": true"), std::string::npos);
            EXPECT_NE(json.find("\"recommended_profile\": \"micro\""), std::string::npos);
            EXPECT_NE(json.find("Prefer \\\"micro\\\" for line\\nwrapped output."),
                      std::string::npos);
            EXPECT_NE(json.find("Escapes backslash \\\\ and\\ttab characters."), std::string::npos);
            EXPECT_EQ(CountSubstring(json, "{\"key\":\""), 1U);
            EXPECT_EQ(CountSubstring(json, "{\"benchmark\":\""), 1U);
            EXPECT_TRUE(std::regex_search(json, std::regex(R"(\"samples\"\s*:\s*\[)")));
            EXPECT_TRUE(std::regex_search(json, std::regex(R"(\"decisions\"\s*:\s*\[)")));
        }
    } // namespace
} // namespace us4::runtime::tests
