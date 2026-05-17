#include "runtime/cli/command_line.h"
#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/hardware_probe.h"
#include "runtime/core/model_registry.h"
#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "runtime/core/tensor.h"
#include "us4/profiles/profile_catalog.h"
#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/model_loader.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"
#include "us4/runtime/backends/cpu_avx/scalar_attention.h"
#include "us4/runtime/backends/cpu_avx/scalar_matmul.h"
#include "us4/runtime/benchmarks/benchmark_registry.h"
#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
            _putenv_s("US4_NPU_NAME", "");
            _putenv_s("US4_GPU_VENDOR", "");
            _putenv_s("US4_NPU_VENDOR", "");
            _putenv_s("US4_GPU_CLASS", "");
            _putenv_s("US4_HOST_GIB", "");
            _putenv_s("US4_DEVICE_GIB", "");
            _putenv_s("US4_STORAGE_GIB", "");
            _putenv_s("US4_POWER_SOURCE", "");
            _putenv_s("US4_BATTERY_PERCENT", "");
            _putenv_s("US4_BATTERY_SAVER", "");
            _putenv_s("US4_THERMAL_STATE", "");
            _putenv_s("US4_ETW_THROTTLED", "");
            _putenv_s("US4_PROFILE_STORE_PATH", "");
#else
            unsetenv("US4_HAS_CUDA");
            unsetenv("US4_HAS_DIRECTML");
            unsetenv("US4_HAS_VULKAN");
            unsetenv("US4_HAS_NPU");
            unsetenv("US4_HAS_AVX512");
            unsetenv("US4_HAS_AMX");
            unsetenv("US4_CPU_NAME");
            unsetenv("US4_GPU_NAME");
            unsetenv("US4_NPU_NAME");
            unsetenv("US4_GPU_VENDOR");
            unsetenv("US4_NPU_VENDOR");
            unsetenv("US4_GPU_CLASS");
            unsetenv("US4_HOST_GIB");
            unsetenv("US4_DEVICE_GIB");
            unsetenv("US4_STORAGE_GIB");
            unsetenv("US4_POWER_SOURCE");
            unsetenv("US4_BATTERY_PERCENT");
            unsetenv("US4_BATTERY_SAVER");
            unsetenv("US4_THERMAL_STATE");
            unsetenv("US4_ETW_THROTTLED");
            unsetenv("US4_PROFILE_STORE_PATH");
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

        TEST(HardwareProbeTest, BenchmarkRegistryExposesSprint11HybridBackendCases)
        {
            const auto vulkanCases =
                us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("vulkan");
            const auto windowsMlCases =
                us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("windows-ml");

            EXPECT_TRUE(std::any_of(vulkanCases.begin(), vulkanCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "vulkan_qwen_balanced" &&
                                               benchmark.modelId == "qwen-0.5b" &&
                                               benchmark.requiresGpu && benchmark.touchesCli &&
                                               benchmark.participatesInCorrectnessGate;
                                    }));
            EXPECT_TRUE(std::any_of(vulkanCases.begin(), vulkanCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "vulkan_llama_balanced" &&
                                               benchmark.adapterId == "dense-llama" &&
                                               benchmark.runtimeMode == "BALANCED" &&
                                               benchmark.participatesInCorrectnessGate;
                                    }));
            EXPECT_TRUE(std::any_of(windowsMlCases.begin(), windowsMlCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "windows_ml_qwen_opt_in" &&
                                               benchmark.profileId == "micro" &&
                                               !benchmark.requiresGpu &&
                                               benchmark.participatesInCorrectnessGate;
                                    }));
            EXPECT_TRUE(std::any_of(
                windowsMlCases.begin(), windowsMlCases.end(),
                [](const auto& benchmark)
                {
                    return benchmark.name == "windows_ml_qwen_thermal_throttle" &&
                           benchmark.scenario == "npu-dense-offload-thermal-throttle" &&
                           benchmark.touchesCli && benchmark.participatesInCorrectnessGate;
                }));
            EXPECT_TRUE(
                std::any_of(windowsMlCases.begin(), windowsMlCases.end(),
                            [](const auto& benchmark)
                            {
                                return benchmark.name == "windows_ml_qwen_no_npu_fallback" &&
                                       benchmark.scenario == "npu-opt-in-cpu-fallback-no-device" &&
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

        TEST(HardwareProbeTest, TensorTracksExpandedSprint02DataTypes)
        {
            const Tensor bf16Tensor({2U, 2U}, TensorDataType::kBFloat16);
            const Tensor int4Tensor({4U, 4U}, TensorDataType::kInt4);

            EXPECT_EQ(bf16Tensor.DataType(), TensorDataType::kBFloat16);
            EXPECT_EQ(int4Tensor.DataType(), TensorDataType::kInt4);
            EXPECT_EQ(int4Tensor.ElementCount(), 16U);
        }

        TEST(HardwareProbeTest, ScalarMatMulMatchesReferenceOutput)
        {
            Tensor left({2U, 3U});
            left.At({0U, 0U}) = 1.0F;
            left.At({0U, 1U}) = 2.0F;
            left.At({0U, 2U}) = 3.0F;
            left.At({1U, 0U}) = 4.0F;
            left.At({1U, 1U}) = 5.0F;
            left.At({1U, 2U}) = 6.0F;

            Tensor right({3U, 2U});
            right.At({0U, 0U}) = 7.0F;
            right.At({0U, 1U}) = 8.0F;
            right.At({1U, 0U}) = 9.0F;
            right.At({1U, 1U}) = 10.0F;
            right.At({2U, 0U}) = 11.0F;
            right.At({2U, 1U}) = 12.0F;

            const Tensor output = us4::runtime::backends::cpu_avx::ScalarMatMul(left, right);

            ASSERT_EQ(output.Shape(), std::vector<std::size_t>({2U, 2U}));
            EXPECT_NEAR(output.At({0U, 0U}), 58.0F, 1e-4F);
            EXPECT_NEAR(output.At({0U, 1U}), 64.0F, 1e-4F);
            EXPECT_NEAR(output.At({1U, 0U}), 139.0F, 1e-4F);
            EXPECT_NEAR(output.At({1U, 1U}), 154.0F, 1e-4F);
        }

        TEST(HardwareProbeTest, ScalarAttentionAppliesCausalMaskAndConcatsCachedKv)
        {
            Tensor query({2U, 2U});
            query.At({0U, 0U}) = 1.0F;
            query.At({0U, 1U}) = 0.0F;
            query.At({1U, 0U}) = 0.0F;
            query.At({1U, 1U}) = 1.0F;

            Tensor key({2U, 2U});
            key.At({0U, 0U}) = 1.0F;
            key.At({0U, 1U}) = 0.0F;
            key.At({1U, 0U}) = 0.0F;
            key.At({1U, 1U}) = 1.0F;

            Tensor value({2U, 2U});
            value.At({0U, 0U}) = 10.0F;
            value.At({0U, 1U}) = 1.0F;
            value.At({1U, 0U}) = 1.0F;
            value.At({1U, 1U}) = 10.0F;

            Tensor cachedKey({1U, 2U});
            cachedKey.At({0U, 0U}) = 1.0F;
            cachedKey.At({0U, 1U}) = 1.0F;

            Tensor cachedValue({1U, 2U});
            cachedValue.At({0U, 0U}) = 5.0F;
            cachedValue.At({0U, 1U}) = 5.0F;

            const auto output = us4::runtime::backends::cpu_avx::ScalarAttention(
                query, key, value,
                us4::runtime::backends::cpu_avx::AttentionOptions{
                    .scale = 1.0F,
                    .causalMask = true,
                    .cachedKey = &cachedKey,
                    .cachedValue = &cachedValue,
                });

            ASSERT_EQ(output.Shape(), std::vector<std::size_t>({2U, 2U}));
            EXPECT_GT(output.At({0U, 0U}), output.At({0U, 1U}));
            EXPECT_GT(output.At({1U, 1U}), output.At({1U, 0U}));
            EXPECT_GT(output.At({0U, 0U}), 5.0F);
            EXPECT_LT(output.At({1U, 0U}), 5.0F);
        }

        TEST(HardwareProbeTest, LoadModelAssetAcceptsSyntheticAndMinimalFormats)
        {
            const auto synthetic =
                us4::runtime::adapters::LoadModelAsset("builtin://qwen-0.5b", "qwen-0.5b");
            ASSERT_TRUE(synthetic.ok);
            EXPECT_EQ(synthetic.descriptor.format,
                      us4::runtime::adapters::ModelAssetFormat::kSynthetic);

            const auto root = std::filesystem::temp_directory_path();
            const auto ggufPath = root / "us4-test-model.gguf";
            const auto safetensorsPath = root / "us4-test-model.safetensors";

            {
                std::ofstream stream(ggufPath, std::ios::binary);
                stream.write("GGUF", 4);
                const char payload[] = {0x01, 0x02, 0x03, 0x04};
                stream.write(payload, static_cast<std::streamsize>(sizeof(payload)));
            }

            {
                std::ofstream stream(safetensorsPath, std::ios::binary);
                const std::uint64_t headerSize = 2;
                stream.write(reinterpret_cast<const char*>(&headerSize),
                             static_cast<std::streamsize>(sizeof(headerSize)));
                stream.write("{}", 2);
                const char payload[] = {0x11, 0x12, 0x13, 0x14};
                stream.write(payload, static_cast<std::streamsize>(sizeof(payload)));
            }

            const auto gguf =
                us4::runtime::adapters::LoadModelAsset(ggufPath.string(), "qwen-0.5b");
            const auto safetensors =
                us4::runtime::adapters::LoadModelAsset(safetensorsPath.string(), "gemma-2b");

            EXPECT_TRUE(gguf.ok);
            EXPECT_EQ(gguf.descriptor.format, us4::runtime::adapters::ModelAssetFormat::kGguf);
            EXPECT_TRUE(safetensors.ok);
            EXPECT_EQ(safetensors.descriptor.format,
                      us4::runtime::adapters::ModelAssetFormat::kSafetensors);

            std::filesystem::remove(ggufPath);
            std::filesystem::remove(safetensorsPath);
        }

        TEST(HardwareProbeTest, ScalarAdaptersRoundTripPromptTokenization)
        {
            const std::string prompt = "hello runtime";

            auto qwenAdapter = us4::runtime::adapters::CreateQwenScalarAdapter();
            auto gemmaAdapter = us4::runtime::adapters::CreateGemmaScalarAdapter();

            auto* qwenDense =
                dynamic_cast<us4::runtime::adapters::DenseAdapterBase*>(qwenAdapter.get());
            auto* gemmaDense =
                dynamic_cast<us4::runtime::adapters::DenseAdapterBase*>(gemmaAdapter.get());

            ASSERT_NE(qwenDense, nullptr);
            ASSERT_NE(gemmaDense, nullptr);

            const auto qwenTokens = qwenDense->TokenizePrompt(prompt);
            const auto gemmaTokens = gemmaDense->TokenizePrompt(prompt);

            EXPECT_EQ(qwenDense->DetokenizePromptTokens(qwenTokens), prompt);
            EXPECT_EQ(gemmaDense->DetokenizePromptTokens(gemmaTokens), prompt);
            EXPECT_NE(qwenTokens, gemmaTokens);
            EXPECT_EQ(qwenTokens.size(), prompt.size());
            EXPECT_EQ(gemmaTokens.size(), prompt.size());
        }

        TEST(HardwareProbeTest, ExecuteCpuScalarRunReturnsCompletedReport)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.maxGenerationTokens = 5U;

            const RuntimePlan plan = RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = ExecuteCpuScalarRun(plan, "hello from unit");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(runResult.report.modelPath, "builtin://qwen-0.5b");
            EXPECT_EQ(runResult.report.modelFormat,
                      us4::runtime::adapters::ModelAssetFormat::kSynthetic);
            EXPECT_EQ(runResult.report.generatedTokens.size(), 5U);
            EXPECT_FALSE(runResult.report.generatedText.empty());
            EXPECT_GT(runResult.report.scalarMatMulChecksum, 0.0);
            EXPECT_GT(runResult.report.scalarAttentionChecksum, 0.0);
            EXPECT_EQ(runResult.report.kvStats.segmentCount, 1U);
            EXPECT_EQ(runResult.report.prefixCacheWarmEntries, 1U);
            EXPECT_GE(runResult.report.telemetryEventCount, 3U);
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
            auto makeDescriptor = [&](us4::runtime::backends::BackendKind kind, std::string name,
                                      std::string displayName,
                                      us4::runtime::backends::DeviceClass deviceClass,
                                      us4::runtime::backends::BackendVendor vendor,
                                      us4::runtime::backends::PrecisionMode precision,
                                      std::uint32_t rank, std::uint32_t maxContextTokensHint)
            {
                us4::runtime::backends::BackendDescriptor descriptor{};
                descriptor.kind = kind;
                descriptor.name = std::move(name);
                descriptor.displayName = std::move(displayName);
                descriptor.deviceClass = deviceClass;
                descriptor.vendor = vendor;
                descriptor.availability = us4::runtime::backends::BackendAvailability::kAvailable;
                descriptor.defaultPrecision = precision;
                descriptor.selectionRank = rank;
                descriptor.maxContextTokensHint = maxContextTokensHint;
                descriptor.supportsPagedKv = true;
                descriptor.supportsMoE = true;
                descriptor.budget = summary.capabilities.budget;
                return descriptor;
            };
            summary.backends = {
                makeDescriptor(us4::runtime::backends::BackendKind::kDirectML, "directml",
                               "DirectML", us4::runtime::backends::DeviceClass::kIntegratedGpu,
                               us4::runtime::backends::BackendVendor::kIntel,
                               us4::runtime::backends::PrecisionMode::kFp16, 20U, 4096U),
                makeDescriptor(us4::runtime::backends::BackendKind::kCpu, "cpu-avx2", "CPU AVX2",
                               us4::runtime::backends::DeviceClass::kCpuOnly,
                               us4::runtime::backends::BackendVendor::kMicrosoft,
                               us4::runtime::backends::PrecisionMode::kFp32, 99U, 2048U),
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
            EXPECT_NE(
                result.stdoutText.find("--backend <auto|cpu|cuda|directml|vulkan|windows-ml|npu>"),
                std::string::npos);
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
            EXPECT_NE(result.stdoutText.find("us4-cli 0.1.28"), std::string::npos);
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

        TEST(HardwareProbeTest, CompletesCpuOnlyRunForScalarBaseline)
        {
            ClearProbeEnv();

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),      const_cast<char*>("run"),
                const_cast<char*>("--model"),      const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),     const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"),    const_cast<char*>("cpu"),
                const_cast<char*>("--max-tokens"), const_cast<char*>("5"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kSuccessExitCode);
            EXPECT_NE(result.stdoutText.find("backend: cpu-avx2"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("execution: cpu-scalar"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("generated_tokens:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("kv.segment_count: 1"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("prefix_cache.entries: 1"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("run_status: completed"), std::string::npos);
            EXPECT_TRUE(result.stderrText.empty());
        }

        TEST(HardwareProbeTest, ReturnsCpuOnlyRunAsJson)
        {
            ClearProbeEnv();

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kRun;
            command.modelId = "qwen-0.5b";
            command.prompt = "hello runtime";
            command.backend = "cpu";
            command.mode = "cpu-only";
            command.maxTokens = 5;
            command.format = "json";

            const auto result = cli::ExecuteCommand(command);

            EXPECT_EQ(result.exitCode, cli::kSuccessExitCode);
            EXPECT_TRUE(result.stderrText.empty());
            EXPECT_NE(result.stdoutText.find("\"execution\": \"run\""), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"plan_execution\": \"cpu-scalar\""),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"status\": \"completed\""), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"backend\": \"cpu-avx2\""), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"generated_tokens\": ["), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"report_text\": \""), std::string::npos);
        }

        TEST(HardwareProbeTest, RejectsRunWithInvalidBackendValue)
        {
            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("tensor-core"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kUsageExitCode);
            EXPECT_NE(result.stderrText.find("Unknown value for --backend"), std::string::npos);
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
            EXPECT_NE(result.stdoutText.find("execution: directml-dry-run"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("directml.graph_state: ready"), std::string::npos);
            EXPECT_NE(result.stderrText.find("not implemented yet"), std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsRunScaffoldJsonForValidatedIntent)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_DIRECTML", "1");
            _putenv_s("US4_GPU_NAME", "Intel Arc Test");
            _putenv_s("US4_DEVICE_GIB", "4");
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kRun;
            command.modelId = "qwen-0.5b";
            command.prompt = "hello runtime";
            command.mode = "auto";
            command.format = "json";

            const auto result = cli::ExecuteCommand(command);

            EXPECT_EQ(result.exitCode, cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("\"execution\": \"run\""), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"status\": \"dry-run\""), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"plan_execution\": \"directml-dry-run\""),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"backend\": \"directml\""), std::string::npos);
            EXPECT_NE(result.stdoutText.find("\"report_text\": \""), std::string::npos);
            EXPECT_NE(result.stderrText.find("not implemented yet"), std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsTuneSummaryAndPersistsProfile)
        {
            ClearProbeEnv();
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-cli-tune-profile-store";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", storePath.string().c_str());
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kTune;
            command.modelId = "qwen-0.5b";
            command.backend = "cpu";
            command.mode = "cpu-only";
            command.maxTokens = 8;

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kSuccessExitCode);
            EXPECT_TRUE(output.stderrText.empty());
            EXPECT_NE(output.stdoutText.find("execution: tune"), std::string::npos);
            EXPECT_NE(output.stdoutText.find("tune.selected_profile: cpu-only"), std::string::npos);
            EXPECT_NE(output.stdoutText.find("tune.persisted: yes"), std::string::npos);
            EXPECT_NE(output.stdoutText.find("tune_status: completed"), std::string::npos);

            us4::runtime::tuning::ProfileStore store(storePath);
            const auto capabilities = us4::runtime::backends::HardwareProbe::DetectHost();
            EXPECT_EQ(store.LoadProfileId(capabilities), std::optional<std::string>("cpu-only"));

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsProbeJsonWhenRequested)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_DIRECTML", "1");
            _putenv_s("US4_CPU_NAME", "Probe Json CPU");
            _putenv_s("US4_GPU_NAME", "Probe Json GPU");
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kProbe;
            command.format = "json";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kSuccessExitCode);
            EXPECT_TRUE(output.stderrText.empty());
            EXPECT_NE(output.stdoutText.find("\"execution\": \"probe\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"cpu\": \"Probe Json CPU\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"gpu\": \"Probe Json GPU\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"selected_backend\": \"directml\""),
                      std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsTuneJsonWhenRequested)
        {
            ClearProbeEnv();
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-cli-tune-json-store";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", storePath.string().c_str());
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kTune;
            command.modelId = "qwen-0.5b";
            command.backend = "cpu";
            command.mode = "cpu-only";
            command.format = "json";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kSuccessExitCode);
            EXPECT_TRUE(output.stderrText.empty());
            EXPECT_NE(output.stdoutText.find("\"execution\": \"tune\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"selected_profile\": \"cpu-only\""),
                      std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"persisted\": true"), std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsBenchJsonWithoutPersistingProfile)
        {
            ClearProbeEnv();
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-cli-bench-profile-store";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", storePath.string().c_str());
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kBench;
            command.modelId = "qwen-0.5b";
            command.backend = "cpu";
            command.mode = "cpu-only";
            command.format = "json";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kSuccessExitCode);
            EXPECT_TRUE(output.stderrText.empty());
            EXPECT_NE(output.stdoutText.find("\"execution\": \"bench\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"selected_profile\": \"cpu-only\""),
                      std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"persisted\": false"), std::string::npos);

            us4::runtime::tuning::ProfileStore store(storePath);
            const auto capabilities = us4::runtime::backends::HardwareProbe::DetectHost();
            EXPECT_EQ(store.LoadProfileId(capabilities), std::nullopt);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsBenchTextWithoutPersistingProfile)
        {
            ClearProbeEnv();
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-cli-bench-text-profile-store";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", storePath.string().c_str());
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kBench;
            command.modelId = "qwen-0.5b";
            command.backend = "cpu";
            command.mode = "cpu-only";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kSuccessExitCode);
            EXPECT_TRUE(output.stderrText.empty());
            EXPECT_NE(output.stdoutText.find("execution: bench"), std::string::npos);
            EXPECT_NE(output.stdoutText.find("bench.selected_profile: cpu-only"),
                      std::string::npos);
            EXPECT_NE(output.stdoutText.find("bench.persisted: no"), std::string::npos);
            EXPECT_NE(output.stdoutText.find("bench_status: completed"), std::string::npos);

            us4::runtime::tuning::ProfileStore store(storePath);
            const auto capabilities = us4::runtime::backends::HardwareProbe::DetectHost();
            EXPECT_EQ(store.LoadProfileId(capabilities), std::nullopt);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, RejectsBenchWithInvalidFormat)
        {
            ClearProbeEnv();

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kBench;
            command.modelId = "qwen-0.5b";
            command.backend = "cpu";
            command.mode = "cpu-only";
            command.format = "yaml";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kUsageExitCode);
            EXPECT_NE(output.stdoutText.find("us4-cli bench"), std::string::npos);
            EXPECT_EQ(output.stderrText, "Unknown value for --format.\n");

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, TunePersistsProfileAndSubsequentRunUsesProfileStore)
        {
            ClearProbeEnv();
            const auto tempRoot =
                std::filesystem::temp_directory_path() / "us4-cli-run-profile-store-hit";
            std::filesystem::create_directories(tempRoot);
            const auto storePath = tempRoot / "profiles.json";
#if defined(_WIN32)
            _putenv_s("US4_PROFILE_STORE_PATH", storePath.string().c_str());
            _putenv_s("US4_HAS_DIRECTML", "1");
            _putenv_s("US4_GPU_NAME", "Intel Arc Test");
            _putenv_s("US4_GPU_VENDOR", "intel");
            _putenv_s("US4_GPU_CLASS", "integrated");
            _putenv_s("US4_DEVICE_GIB", "8");
#endif

            cli::ParsedCommand tuneCommand{};
            tuneCommand.kind = cli::CommandKind::kTune;
            tuneCommand.modelId = "qwen-0.5b";
            tuneCommand.backend = "cpu";
            tuneCommand.mode = "cpu-only";
            tuneCommand.maxTokens = 8;

            const auto tuneOutput = cli::ExecuteCommand(tuneCommand);
            ASSERT_EQ(tuneOutput.exitCode, cli::kSuccessExitCode);

            cli::ParsedCommand runCommand{};
            runCommand.kind = cli::CommandKind::kRun;
            runCommand.modelId = "qwen-0.5b";
            runCommand.prompt = "reuse tuned profile";
            runCommand.backend = "auto";
            runCommand.mode = "auto";
            runCommand.maxTokens = 4;

            const auto runOutput = cli::ExecuteCommand(runCommand);

            EXPECT_EQ(runOutput.exitCode, cli::kSuccessExitCode);
            EXPECT_NE(runOutput.stdoutText.find("profile: cpu-only"), std::string::npos);
            EXPECT_NE(runOutput.stdoutText.find("profile-store-hit"), std::string::npos);
            EXPECT_NE(runOutput.stdoutText.find("run_status: completed"), std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsServeJsonScaffoldWhenRequested)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_VULKAN", "1");
            _putenv_s("US4_GPU_NAME", "Serve Json GPU");
            _putenv_s("US4_GPU_VENDOR", "amd");
            _putenv_s("US4_GPU_CLASS", "discrete");
            _putenv_s("US4_DEVICE_GIB", "8");
#endif

            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kServe;
            command.modelId = "qwen-0.5b";
            command.backend = "vulkan";
            command.format = "json";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kNotImplementedExitCode);
            EXPECT_NE(output.stdoutText.find("\"execution\": \"serve\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"status\": \"scaffold-only\""), std::string::npos);
            EXPECT_NE(output.stdoutText.find("\"backend\": \"vulkan\""), std::string::npos);
            EXPECT_NE(output.stderrText.find("Serve pipeline scaffolding is ready"),
                      std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, RejectsInvalidFormatValues)
        {
            cli::ParsedCommand command{};
            command.kind = cli::CommandKind::kBench;
            command.modelId = "qwen-0.5b";
            command.format = "xml";

            const auto output = cli::ExecuteCommand(command);

            EXPECT_EQ(output.exitCode, cli::kUsageExitCode);
            EXPECT_NE(output.stderrText.find("Unknown value for --format."), std::string::npos);
        }

        TEST(HardwareProbeTest, ParsesServeArguments)
        {
            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("serve"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--backend"), const_cast<char*>("windows-ml"),
                const_cast<char*>("--format"),  const_cast<char*>("json"),
                const_cast<char*>("--npu"),
            };

            const auto command = us4::cli::ParseArguments(static_cast<int>(argv.size()),
                                                          const_cast<char**>(argv.data()));

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kServe);
            EXPECT_EQ(command.modelId, "qwen-0.5b");
            EXPECT_EQ(command.backend, "windows-ml");
            EXPECT_EQ(command.format, "json");
            EXPECT_TRUE(command.allowNpu);
        }

        TEST(HardwareProbeTest, ReturnsCudaDryRunPlanForExplicitCudaBackend)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_CUDA", "1");
            _putenv_s("US4_GPU_NAME", "NVIDIA RTX 4090");
            _putenv_s("US4_GPU_VENDOR", "nvidia");
            _putenv_s("US4_DEVICE_GIB", "24");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("cuda"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("execution: cuda-dry-run"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("cuda.execution_flavor:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("cuda.prefill_chunk_tokens:"), std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsWindowsMlDryRunPlanWhenNpuIsRequested)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_NPU", "1");
            _putenv_s("US4_HAS_VULKAN", "1");
            _putenv_s("US4_NPU_NAME", "Ryzen AI");
            _putenv_s("US4_NPU_VENDOR", "microsoft");
            _putenv_s("US4_GPU_NAME", "Radeon RX Test");
            _putenv_s("US4_GPU_VENDOR", "amd");
            _putenv_s("US4_GPU_CLASS", "discrete");
            _putenv_s("US4_HOST_GIB", "32");
            _putenv_s("US4_DEVICE_GIB", "8");
            _putenv_s("US4_POWER_SOURCE", "ac");
            _putenv_s("US4_BATTERY_PERCENT", "100");
            _putenv_s("US4_BATTERY_SAVER", "0");
            _putenv_s("US4_THERMAL_STATE", "nominal");
            _putenv_s("US4_ETW_THROTTLED", "0");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("windows-ml"),
                const_cast<char*>("--npu"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("execution: windows-ml-dry-run"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.opt_in_satisfied: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.partition_count:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.adapter_state: compiled"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.npu_partitions:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.compile_target: npu"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.graph_reusable: yes"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.dispatch_table_size: 5"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.first_dispatch_target: npu"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_active: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_slice_count: 5"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_gpu_primary: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_npu_dense: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.power_policy: nominal"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_policy_degraded: no"),
                      std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReportsWindowsMlFallbackIssueWithoutNpuOptIn)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_NPU", "1");
            _putenv_s("US4_HAS_VULKAN", "1");
            _putenv_s("US4_NPU_NAME", "Ryzen AI");
            _putenv_s("US4_NPU_VENDOR", "microsoft");
            _putenv_s("US4_GPU_NAME", "Radeon RX Test");
            _putenv_s("US4_GPU_VENDOR", "amd");
            _putenv_s("US4_GPU_CLASS", "discrete");
            _putenv_s("US4_HOST_GIB", "32");
            _putenv_s("US4_DEVICE_GIB", "8");
            _putenv_s("US4_POWER_SOURCE", "ac");
            _putenv_s("US4_BATTERY_PERCENT", "100");
            _putenv_s("US4_BATTERY_SAVER", "0");
            _putenv_s("US4_THERMAL_STATE", "nominal");
            _putenv_s("US4_ETW_THROTTLED", "0");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("windows-ml"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("windows_ml.opt_in_satisfied: no"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.issue_codes: windows_ml.opt_in_required"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.cpu_fallback_partitions: 1"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.compile_target: cpu-fallback"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.fallback_reason: opt-in-required"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.last_dispatch_target: host-assist"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_active: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_cpu_fallback: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.power_policy: nominal"),
                      std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, DowngradesWindowsMlMixedDispatchWhenThermallyThrottled)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_NPU", "1");
            _putenv_s("US4_HAS_VULKAN", "1");
            _putenv_s("US4_NPU_NAME", "Ryzen AI");
            _putenv_s("US4_NPU_VENDOR", "microsoft");
            _putenv_s("US4_GPU_NAME", "Radeon RX Test");
            _putenv_s("US4_GPU_VENDOR", "amd");
            _putenv_s("US4_GPU_CLASS", "discrete");
            _putenv_s("US4_HOST_GIB", "32");
            _putenv_s("US4_DEVICE_GIB", "8");
            _putenv_s("US4_POWER_SOURCE", "battery");
            _putenv_s("US4_BATTERY_PERCENT", "18");
            _putenv_s("US4_BATTERY_SAVER", "1");
            _putenv_s("US4_THERMAL_STATE", "throttled");
            _putenv_s("US4_ETW_THROTTLED", "1");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("windows-ml"),
                const_cast<char*>("--npu"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("windows_ml.power_policy: thermal-throttle"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.synthetic_power_telemetry: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.power.issue_codes: "
                                             "windows_ml.power.thermal_throttle,"
                                             "windows_ml.power.etw_signal"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_npu_dense: no"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_policy_degraded: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_npu_demotions: 3"),
                      std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, FallsBackGracefullyWhenWindowsMlOptInHasNoNpuAvailable)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_NPU", "");
            _putenv_s("US4_HAS_VULKAN", "1");
            _putenv_s("US4_GPU_NAME", "Radeon RX Test");
            _putenv_s("US4_GPU_VENDOR", "amd");
            _putenv_s("US4_GPU_CLASS", "discrete");
            _putenv_s("US4_HOST_GIB", "32");
            _putenv_s("US4_DEVICE_GIB", "8");
            _putenv_s("US4_POWER_SOURCE", "ac");
            _putenv_s("US4_BATTERY_PERCENT", "100");
            _putenv_s("US4_BATTERY_SAVER", "0");
            _putenv_s("US4_THERMAL_STATE", "nominal");
            _putenv_s("US4_ETW_THROTTLED", "0");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("windows-ml"),
                const_cast<char*>("--npu"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("windows_ml.adapter_state: compiled"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.compile_target: cpu-fallback"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.fallback_reason: npu-unavailable"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.cpu_fallback_armed: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.npu_partitions: 0"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_npu_dense: no"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_cpu_fallback: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("windows_ml.mixed_dispatch_policy_degraded: no"),
                      std::string::npos);

            ClearProbeEnv();
        }

        TEST(HardwareProbeTest, ReturnsVulkanDryRunPlanForCrossVendorFallback)
        {
            ClearProbeEnv();
#if defined(_WIN32)
            _putenv_s("US4_HAS_VULKAN", "1");
            _putenv_s("US4_GPU_NAME", "Radeon RX Test");
            _putenv_s("US4_GPU_VENDOR", "amd");
            _putenv_s("US4_GPU_CLASS", "discrete");
            _putenv_s("US4_DEVICE_GIB", "12");
#endif

            const std::vector<char*> argv = {
                const_cast<char*>("us4-cli"),   const_cast<char*>("run"),
                const_cast<char*>("--model"),   const_cast<char*>("qwen-0.5b"),
                const_cast<char*>("--prompt"),  const_cast<char*>("hello runtime"),
                const_cast<char*>("--backend"), const_cast<char*>("vulkan"),
                const_cast<char*>("--npu"),
            };

            const us4::cli::ParsedCommand command = us4::cli::ParseArguments(
                static_cast<int>(argv.size()), const_cast<char**>(argv.data()));
            const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

            EXPECT_EQ(command.kind, us4::cli::CommandKind::kRun);
            EXPECT_EQ(result.exitCode, us4::cli::kNotImplementedExitCode);
            EXPECT_NE(result.stdoutText.find("execution: vulkan-dry-run"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.context_state: bound"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.step_count:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.descriptor_sets:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.kernel_manifest_loaded: yes"),
                      std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.kernel_count:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.required_kernel_count:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.timeline_semaphores:"), std::string::npos);
            EXPECT_NE(result.stdoutText.find("vulkan.issue_codes: vulkan.npu.bypass"),
                      std::string::npos);
            EXPECT_NE(result.stderrText.find("not implemented yet"), std::string::npos);

            ClearProbeEnv();
        }

    } // namespace
} // namespace us4::core
