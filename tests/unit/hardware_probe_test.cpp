#include "runtime/cli/command_line.h"
#include "runtime/core/hardware_probe.h"
#include "runtime/core/model_registry.h"
#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/profiles/profile_catalog.h"
#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <algorithm>
#include <cstdlib>
#include <gtest/gtest.h>
#include <vector>

namespace us4::core
{
    namespace
    {

        void ClearProbeEnv()
        {
#if defined(_WIN32)
            _putenv_s("US4_HAS_CUDA", "");
            _putenv_s("US4_HAS_DIRECTML", "");
            _putenv_s("US4_HAS_VULKAN", "");
            _putenv_s("US4_HAS_NPU", "");
            _putenv_s("US4_HAS_AVX512", "");
            _putenv_s("US4_HAS_AMX", "");
            _putenv_s("US4_CPU_NAME", "");
            _putenv_s("US4_GPU_NAME", "");
            _putenv_s("US4_HOST_GIB", "");
            _putenv_s("US4_DEVICE_GIB", "");
            _putenv_s("US4_STORAGE_GIB", "");
#else
            unsetenv("US4_HAS_CUDA");
            unsetenv("US4_HAS_DIRECTML");
            unsetenv("US4_HAS_VULKAN");
            unsetenv("US4_HAS_NPU");
            unsetenv("US4_HAS_AVX512");
            unsetenv("US4_HAS_AMX");
            unsetenv("US4_CPU_NAME");
            unsetenv("US4_GPU_NAME");
            unsetenv("US4_HOST_GIB");
            unsetenv("US4_DEVICE_GIB");
            unsetenv("US4_STORAGE_GIB");
#endif
        }

        TEST(HardwareProbeTest, FallsBackToCpuWhenNoAcceleratorIsAvailable)
        {
            ClearProbeEnv();

            const ProbeSummary summary = ProbeHardware();

            EXPECT_EQ(summary.osName, "Windows x86-64");
            EXPECT_EQ(summary.selectedBackend, "cpu-avx2");
            EXPECT_EQ(summary.mode, "CPU_ONLY");
            ASSERT_FALSE(summary.backends.empty());
            EXPECT_EQ(summary.backends.back().name, "cpu-avx2");
            EXPECT_TRUE(summary.capabilities.hasAvx2);
            EXPECT_FALSE(summary.HasAccelerator());
            ASSERT_FALSE(summary.advisories.empty());
            EXPECT_EQ(summary.advisories.front().code, "fallback.cpu_only");
        }

        TEST(HardwareProbeTest, ParsesRuntimeModesFromCliValues)
        {
            EXPECT_FALSE(ParseRuntimeMode("auto").has_value());
            ASSERT_TRUE(ParseRuntimeMode("full").has_value());
            EXPECT_EQ(*ParseRuntimeMode("full"), us4::runtime::backends::RuntimeMode::kFull);
            ASSERT_TRUE(ParseRuntimeMode("cpu-only").has_value());
            EXPECT_EQ(*ParseRuntimeMode("cpu-only"), us4::runtime::backends::RuntimeMode::kCpuOnly);
        }

        TEST(HardwareProbeTest, SelectsCudaAndMarksLowVramAsDegraded)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_CUDA", "1");
            _putenv_s("US4_GPU_NAME", "RTX Test");
            _putenv_s("US4_DEVICE_GIB", "6");
#endif

            const ProbeSummary summary = ProbeHardware();

            EXPECT_EQ(summary.selectedBackend, "cuda");
            EXPECT_EQ(summary.mode, "DEGRADED");
            EXPECT_TRUE(summary.HasAccelerator());
            EXPECT_TRUE(summary.IsDegraded());
            ASSERT_GE(summary.advisories.size(), 1U);
            EXPECT_EQ(summary.advisories.front().code, "budget.low_vram");

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, RuntimeContextBuildsPlanForKnownModel)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasDirectMl = true;
            capabilities.hasAvx2 = true;
            capabilities.primaryAdapterName = "Integrated GPU";
            capabilities.primaryAdapterVendor = us4::runtime::backends::BackendVendor::kIntel;
            capabilities.primaryAdapterClass = us4::runtime::backends::DeviceClass::kIntegratedGpu;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.storageBytes = 256ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = us4::runtime::backends::RuntimeMode::kBalanced;

            const RuntimePlan plan = RuntimeContext::BuildPlan(request, capabilities);

            EXPECT_EQ(plan.model.adapterId, "dense-qwen");
            EXPECT_EQ(plan.backend.name, "directml");
            EXPECT_EQ(plan.profile.id, "ultra-low");
            EXPECT_TRUE(plan.issues.empty());
        }

        TEST(HardwareProbeTest, ModelRegistryResolvesByFamilyHint)
        {
            const auto descriptor = ModelRegistry::Resolve("llama-4-mini");

            ASSERT_TRUE(descriptor.has_value());
            EXPECT_EQ(descriptor->family, ModelFamily::kLlama);
            EXPECT_EQ(descriptor->adapterId, "dense-llama");
        }

        TEST(HardwareProbeTest, RuntimeContextFlagsUnknownModels)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "mystery-model";

            const RuntimePlan plan = RuntimeContext::BuildPlan(request, capabilities);

            EXPECT_EQ(plan.model.id, "mystery-model");
            EXPECT_EQ(plan.model.adapterId, "null");
            ASSERT_FALSE(plan.issues.empty());
            EXPECT_EQ(plan.issues.front().code, "unknown-model");
        }

        TEST(HardwareProbeTest, BenchmarkRegistryExposesSprint02CpuScalarCases)
        {
            const auto cpuCases =
                us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("cpu");

            EXPECT_TRUE(std::any_of(cpuCases.begin(), cpuCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "scalar_matmul_reference" &&
                                               benchmark.profileId == "cpu-only" &&
                                               benchmark.runtimeMode == "CPU_ONLY" &&
                                               benchmark.promptTokens == 128U &&
                                               !benchmark.requiresGpu;
                                    }));
            EXPECT_TRUE(std::any_of(cpuCases.begin(), cpuCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "scalar_attention_reference" &&
                                               benchmark.generationTokens == 32U &&
                                               benchmark.participatesInCorrectnessGate;
                                    }));
            EXPECT_TRUE(std::any_of(cpuCases.begin(), cpuCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "dense_baseline_qwen_cpu_only" &&
                                               benchmark.adapterId == "dense-qwen" &&
                                               benchmark.modelId == "qwen-0.5b" &&
                                               benchmark.touchesCli;
                                    }));
            EXPECT_TRUE(std::any_of(cpuCases.begin(), cpuCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "dense_baseline_gemma_cpu_only" &&
                                               benchmark.adapterId == "dense-gemma" &&
                                               benchmark.modelId == "gemma-2b" &&
                                               benchmark.touchesCli;
                                    }));
        }

        TEST(HardwareProbeTest, BenchmarkRegistryKeepsCrossBackendCorrectnessVisibleToCpuQueries)
        {
            const auto cpuCases =
                us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("cpu");

            EXPECT_TRUE(std::any_of(cpuCases.begin(), cpuCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "correctness_diff" &&
                                               benchmark.backend == "cross-backend" &&
                                               benchmark.participatesInCorrectnessGate;
                                    }));
        }

        TEST(HardwareProbeTest, CpuOnlyProfileMatchesSprint02BaselineContract)
        {
            const auto profile = us4::profiles::ProfileCatalog::FindById("cpu-only");

            ASSERT_TRUE(profile.has_value());
            EXPECT_EQ(profile->preferredBackend, "cpu-avx2");
            EXPECT_EQ(profile->mode, us4::runtime::backends::RuntimeMode::kCpuOnly);
            EXPECT_EQ(profile->targetBatchSize, 1U);
            EXPECT_FALSE(profile->enableSpeculative);
            EXPECT_FALSE(profile->enableMoE);
        }

        TEST(HardwareProbeTest, ProfileCatalogRecommendsCpuOnlyWhenHostHasNoAccelerator)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            EXPECT_EQ(us4::profiles::ProfileCatalog::RecommendId(capabilities), "cpu-only");
        }

        TEST(HardwareProbeTest, FormatsHumanReadableProbeSummary)
        {
            ProbeSummary summary{};
            summary.osName = "Windows x86-64";
            summary.cpuName = "Unit Test CPU";
            summary.gpuName = "Unit Test GPU";
            summary.hasNpu = false;
            summary.selectedBackend = "directml";
            summary.mode = "ULTRA_LOW";
            summary.capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
            summary.capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
            summary.capabilities.budget.storageBytes = 256ULL * 1024ULL * 1024ULL * 1024ULL;
            summary.backends = {
                us4::runtime::backends::BackendDescriptor{
                    us4::runtime::backends::BackendKind::kDirectML,
                    "directml",
                    "DirectML",
                    us4::runtime::backends::BackendVendor::kIntel,
                    us4::runtime::backends::BackendAvailability::kAvailable,
                    us4::runtime::backends::DeviceClass::kIntegratedGpu,
                    us4::runtime::backends::PrecisionMode::kFp16,
                    20U,
                    4096U,
                    false,
                    true,
                    true,
                    false,
                    false,
                    false,
                    false,
                    summary.capabilities.budget,
                },
                us4::runtime::backends::BackendDescriptor{
                    us4::runtime::backends::BackendKind::kCpu,
                    "cpu-avx2",
                    "CPU AVX2",
                    us4::runtime::backends::BackendVendor::kMicrosoft,
                    us4::runtime::backends::BackendAvailability::kAvailable,
                    us4::runtime::backends::DeviceClass::kCpuOnly,
                    us4::runtime::backends::PrecisionMode::kFp32,
                    99U,
                    2048U,
                    false,
                    true,
                    true,
                    false,
                    false,
                    false,
                    false,
                    summary.capabilities.budget,
                },
            };
            summary.advisories = {
                ProbeAdvisory{
                    "warning",
                    "budget.low_shared_memory",
                    "The selected cross-vendor backend has a tight device budget.",
                },
            };

            const std::string rendered = FormatProbeSummary(summary);

            EXPECT_NE(rendered.find("US4 Windows Edition Probe"), std::string::npos);
            EXPECT_NE(rendered.find("backend: directml"), std::string::npos);
            EXPECT_NE(rendered.find("mode: ULTRA_LOW"), std::string::npos);
            EXPECT_NE(rendered.find("Unit Test GPU"), std::string::npos);
            EXPECT_NE(rendered.find("advisories:"), std::string::npos);
            EXPECT_NE(rendered.find("next: us4-cli run --model qwen-0.5b --prompt \"hello\""),
                      std::string::npos);
        }

        TEST(HardwareProbeTest, ParsesHelpCommandAndRendersExpandedUsage)
        {
            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),
                const_cast<char*>("help"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kHelp);
            EXPECT_EQ(result.exitCode, us4::cli::kSuccessExitCode);
            EXPECT_NE(result.stdoutText.find("us4-cli version"), std::string::npos);
            EXPECT_NE(result.stdoutText.find(
                          "--mode <auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>"),
                      std::string::npos);
        }

        TEST(HardwareProbeTest, RejectsRunWithoutPrompt)
        {
            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),
                const_cast<char*>("run"),
                const_cast<char*>("--model"),
                const_cast<char*>("qwen-0.5b"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kInvalid);
            EXPECT_EQ(result.exitCode, us4::cli::kUsageExitCode);
            EXPECT_NE(result.stderrText.find("requires --prompt"), std::string::npos);
        }

        TEST(HardwareProbeTest, RendersVersionCommand)
        {
            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),
                const_cast<char*>("version"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kVersion);
            EXPECT_EQ(result.exitCode, us4::cli::kSuccessExitCode);
            EXPECT_NE(result.stdoutText.find("us4-cli 0.1.0-alpha"), std::string::npos);
        }

        TEST(HardwareProbeTest, RejectsRunWithInvalidModeValue)
        {
            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),  const_cast<char*>("run"),
                const_cast<char*>("--model"),  const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"), const_cast<char*>("hello runtime"),
                const_cast<char*>("--mode"),   const_cast<char*>("turbo"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kUsageExitCode);
            EXPECT_NE(result.stderrText.find("Unknown value for --mode"), std::string::npos);
        }

        TEST(HardwareProbeTest, ReturnsRunScaffoldSummaryForValidatedIntent)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_DIRECTML", "1");
            _putenv_s("US4_GPU_NAME", "Intel Arc Test");
            _putenv_s("US4_DEVICE_GIB", "4");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),  const_cast<char*>("run"),
                const_cast<char*>("--model"),  const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"), const_cast<char*>("hello runtime"),
                const_cast<char*>("--mode"),   const_cast<char*>("auto"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("US4 Runtime Plan"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("backend: directml"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("execution: scaffold-only"), std::string::npos);
            EXPECT_NE(result.stderrText.find("not implemented yet"), std::string::npos);

            ClearProbeEnv();
        }

    } // namespace
} // namespace us4::core
