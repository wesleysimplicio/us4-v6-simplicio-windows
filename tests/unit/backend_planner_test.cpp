#include "runtime/core/tensor.h"
#include "us4/runtime/adapters/adapter_contracts.h"
#include "us4/runtime/backends/backend_selector.h"
#include "us4/runtime/backends/cpu_avx/kernel_profile.h"
#include "us4/runtime/backends/cpu_avx/scalar_matmul.h"
#include "us4/runtime/backends/directml/dml_device.h"
#include "us4/runtime/backends/directml/dml_graph.h"
#include "us4/runtime/backends/onednn/block_gemm_contract.h"
#include "us4/runtime/backends/onednn/onednn_backend.h"
#include "us4/runtime/backends/vulkan/vulkan_context.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"
#include "us4/runtime/backends/vulkan/vulkan_kernel_library.h"
#include "us4/runtime/backends/windows_ml/layer_offloader.h"
#include "us4/runtime/backends/windows_ml/mixed_dispatch_planner.h"
#include "us4/runtime/backends/windows_ml/power_thermal_monitor.h"
#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"
#include "us4/runtime/backends/windows_ml/winml_adapter.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        void ClearPowerTelemetryEnv()
        {
#if defined(_WIN32)
            _putenv_s("US4_POWER_SOURCE", "");
            _putenv_s("US4_BATTERY_PERCENT", "");
            _putenv_s("US4_BATTERY_SAVER", "");
            _putenv_s("US4_THERMAL_STATE", "");
            _putenv_s("US4_ETW_THROTTLED", "");
#else
            unsetenv("US4_POWER_SOURCE");
            unsetenv("US4_BATTERY_PERCENT");
            unsetenv("US4_BATTERY_SAVER");
            unsetenv("US4_THERMAL_STATE");
            unsetenv("US4_ETW_THROTTLED");
#endif
        }

        adapters::RuntimeBinding MakeBinding(const backends::BackendDescriptor& backend,
                                             const std::string& modelId,
                                             const backends::RuntimeMode mode,
                                             const bool allowNpu = false)
        {
            return adapters::RuntimeBinding{
                .backend = backend,
                .profileId = "balanced",
                .mode = mode,
                .modelId = modelId,
                .allowNpu = allowNpu,
            };
        }

        TEST(BackendPlannerTest, DirectMlGraphCompilesAndDispatchesReferenceSession)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasDirectMl = true;
            capabilities.primaryAdapterName = "Intel Arc";
            capabilities.primaryAdapterVendor = backends::BackendVendor::kIntel;
            capabilities.primaryAdapterClass = backends::DeviceClass::kIntegratedGpu;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::directml::DmlDevice device({
                .preferIntegratedGpu = true,
                .preferLowLatency = true,
            });
            ASSERT_TRUE(device.Initialize(capabilities));
            ASSERT_TRUE(device.PrepareGraphSession("qwen-0.5b",
                                                   backends::directml::DmlExecutionPhase::kPrefill,
                                                   4ULL * 1024ULL * 1024ULL));

            backends::directml::DmlGraph graph(&device);
            graph.RecordNode({
                .name = "prefill.matmul",
                .kind = backends::directml::DmlOperatorKind::kMatMul,
                .dataType = backends::directml::DmlTensorDataType::kFp16,
                .inputCount = 2,
                .outputCount = 1,
                .usesPersistentWeights = true,
                .allowChunking = true,
                .temporaryBytes = 512ULL * 1024ULL,
                .persistentBytes = 2ULL * 1024ULL * 1024ULL,
            });
            graph.RecordNode({
                .name = "prefill.attention",
                .kind = backends::directml::DmlOperatorKind::kAttention,
                .dataType = backends::directml::DmlTensorDataType::kFp16,
                .inputCount = 3,
                .outputCount = 1,
                .usesPersistentWeights = false,
                .allowChunking = true,
                .temporaryBytes = 256ULL * 1024ULL,
                .persistentBytes = 512ULL * 1024ULL,
            });

            const auto compile = graph.Compile({
                .precision = backends::PrecisionMode::kFp16,
                .enableOperatorFusion = true,
                .enablePersistentDescriptors = true,
                .enableChunkedCompilation = true,
                .maxTemporaryBytes = 4ULL * 1024ULL * 1024ULL,
                .maxPersistentBytes = 4ULL * 1024ULL * 1024ULL,
            });
            ASSERT_TRUE(compile.compiled);

            const auto dispatch = graph.Dispatch({
                .phase = backends::directml::DmlExecutionPhase::kPrefill,
                .tokenCount = 128U,
                .batchSize = 1U,
                .sequenceLength = 128U,
                .allowAsyncCompletion = true,
            });
            EXPECT_TRUE(dispatch.submitted);
            EXPECT_EQ(graph.State(), backends::directml::DmlGraphState::kReady);
            EXPECT_GT(graph.Stats().lastFenceValue, 0U);
        }

        TEST(BackendPlannerTest, VulkanPlanRespectsNanoFallbackAndKvSpill)
        {
            backends::BackendDescriptor backend{};
            backend.kind = backends::BackendKind::kVulkan;
            backend.name = "vulkan";
            backend.displayName = "Vulkan Compute";
            backend.deviceClass = backends::DeviceClass::kIntegratedGpu;
            backend.vendor = backends::BackendVendor::kIntel;
            backend.defaultPrecision = backends::PrecisionMode::kFp16;
            backend.supportsPagedKv = true;
            backend.supportsSpeculative = true;
            backend.supportsUnifiedMemory = true;
            backend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "llama-3.2-3b";
            request.maxContextTokens = 8192U;
            request.enableSpeculative = true;
            request.requireDeterministic = true;

            const auto plan = backends::vulkan::BuildVulkanExecutionPlan({
                .binding = MakeBinding(backend, "llama-3.2-3b", backends::RuntimeMode::kNano),
                .request = request,
                .adapterId = "dense-llama",
                .targetBatchSize = 2U,
                .modelSupportsMoE = false,
                .modelSupportsSpeculative = true,
            });

            EXPECT_TRUE(plan.fallbackToCpuAttention);
            EXPECT_TRUE(plan.enableHostVisibleKvSpill);
            EXPECT_EQ(plan.decodeMicroBatchSize, 1U);
            ASSERT_FALSE(plan.steps.empty());
            EXPECT_EQ(plan.steps.back().stage, backends::vulkan::VulkanKernelStage::kKvPageOut);
        }

        TEST(BackendPlannerTest, VulkanContextInitializesAndBindsExecutionPlan)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasVulkan = true;
            capabilities.primaryAdapterName = "Radeon RX Test";
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.deviceBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::BackendDescriptor backend{};
            backend.kind = backends::BackendKind::kVulkan;
            backend.name = "vulkan";
            backend.displayName = "Vulkan Compute";
            backend.deviceClass = backends::DeviceClass::kDiscreteGpu;
            backend.vendor = backends::BackendVendor::kAmd;
            backend.defaultPrecision = backends::PrecisionMode::kFp16;
            backend.supportsPagedKv = true;
            backend.supportsSpeculative = true;
            backend.supportsUnifiedMemory = false;
            backend.maxContextTokensHint = 8192U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "vulkan";
            request.maxContextTokens = 4096U;

            const auto plan = backends::vulkan::BuildVulkanExecutionPlan({
                .binding = MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kBalanced),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsMoE = false,
                .modelSupportsSpeculative = true,
            });

            backends::vulkan::VulkanContext context({
                .preferIntegratedGpu = false,
                .enableValidationLayer = true,
                .allowDescriptorBuffer = true,
                .preferAsyncTransfers = true,
            });
            ASSERT_TRUE(context.Initialize(capabilities));
            ASSERT_TRUE(context.BindExecutionPlan("qwen-0.5b", plan));
            EXPECT_EQ(context.State(), backends::vulkan::VulkanContextState::kBound);
            EXPECT_TRUE(context.KernelLibrary().Loaded());
            EXPECT_EQ(context.Stats().loadedKernelCount, context.KernelLibrary().LoadedCount());
            EXPECT_GE(context.Stats().loadedKernelCount, 3U);
            EXPECT_EQ(context.Stats().requiredKernelCount, 3U);
            EXPECT_EQ(context.Queue().queueClass,
                      backends::vulkan::VulkanQueueClass::kDedicatedCompute);
            EXPECT_GE(context.DescriptorArena().setCount, 1U);
            EXPECT_GT(context.Stats().pipelineStageCount, 0U);
            EXPECT_NE(context.DescribeSession().find("backend=vulkan"), std::string::npos);
        }

        TEST(BackendPlannerTest, VulkanKernelLibraryLoadsManifestShaders)
        {
            const auto library = backends::vulkan::VulkanKernelLibrary::LoadDefault();
            ASSERT_TRUE(library.Loaded());
            EXPECT_GE(library.Size(), 3U);
            EXPECT_GE(library.LoadedCount(), 3U);
            EXPECT_TRUE(library.Issues().empty());

            const auto* matmul =
                library.FindProgram(backends::vulkan::VulkanKernelProgram::kMatMul);
            const auto* softmax =
                library.FindProgram(backends::vulkan::VulkanKernelProgram::kSoftmax);
            const auto* rmsnorm =
                library.FindProgram(backends::vulkan::VulkanKernelProgram::kRmsNorm);

            ASSERT_NE(matmul, nullptr);
            ASSERT_NE(softmax, nullptr);
            ASSERT_NE(rmsnorm, nullptr);
            EXPECT_NE(matmul->source.find("#version 460"), std::string::npos);
            EXPECT_NE(softmax->source.find("layout(local_size_x = 64"), std::string::npos);
            EXPECT_NE(rmsnorm->source.find("inversesqrt"), std::string::npos);
        }

        TEST(BackendPlannerTest, WindowsMlPlanRequiresExplicitOptIn)
        {
            backends::BackendDescriptor backend{};
            backend.kind = backends::BackendKind::kWindowsMl;
            backend.name = "windows-ml";
            backend.displayName = "Windows ML";
            backend.deviceClass = backends::DeviceClass::kNpu;
            backend.vendor = backends::BackendVendor::kQualcomm;
            backend.defaultPrecision = backends::PrecisionMode::kInt8;
            backend.supportsNpuOffload = true;
            backend.requiresOptIn = true;
            backend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            const auto fallbackPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kUltraLow, false),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = true,
            });
            EXPECT_FALSE(fallbackPlan.optInSatisfied);
            EXPECT_FALSE(fallbackPlan.partitions.empty());
            EXPECT_EQ(fallbackPlan.partitions.back().kind,
                      backends::windows_ml::WindowsMlPartitionKind::kCpuFallback);

            const auto optInPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kUltraLow, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });
            EXPECT_TRUE(optInPlan.optInSatisfied);
            EXPECT_TRUE(optInPlan.offloadPrefill);
            EXPECT_TRUE(optInPlan.offloadDecode);
        }

        TEST(BackendPlannerTest, WinMlAdapterCompilesOptInAndFallbackPlans)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasNpu = true;
            capabilities.npuName = "Ryzen AI Test";
            capabilities.npuVendor = backends::BackendVendor::kMicrosoft;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::BackendDescriptor backend{};
            backend.kind = backends::BackendKind::kWindowsMl;
            backend.name = "windows-ml";
            backend.displayName = "Windows ML";
            backend.deviceClass = backends::DeviceClass::kNpu;
            backend.vendor = backends::BackendVendor::kMicrosoft;
            backend.defaultPrecision = backends::PrecisionMode::kInt8;
            backend.supportsNpuOffload = true;
            backend.requiresOptIn = true;
            backend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            backends::windows_ml::WinMlAdapter adapter({
                .allowCpuFallback = true,
                .preferStaticShapes = true,
                .enableTelemetry = true,
            });
            ASSERT_TRUE(adapter.Initialize(capabilities));

            const auto fallbackPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kUltraLow, false),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = true,
            });
            ASSERT_TRUE(adapter.CompileGraph("qwen-0.5b", fallbackPlan));
            EXPECT_EQ(adapter.State(), backends::windows_ml::WinMlAdapterState::kCompiled);
            EXPECT_EQ(adapter.Stats().cpuFallbackPartitionCount, 1U);
            ASSERT_TRUE(adapter.SessionArtifact().has_value());
            EXPECT_EQ(adapter.SessionArtifact()->compileTarget,
                      backends::windows_ml::WinMlCompileTarget::kCpuFallback);
            EXPECT_EQ(adapter.SessionArtifact()->fallbackReason, "opt-in-required");

            const auto optInPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kUltraLow, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });
            ASSERT_TRUE(adapter.CompileGraph("qwen-0.5b", optInPlan));
            EXPECT_GT(adapter.Stats().npuPartitionCount, 0U);
            ASSERT_TRUE(adapter.SessionArtifact().has_value());
            EXPECT_EQ(adapter.SessionArtifact()->compileTarget,
                      backends::windows_ml::WinMlCompileTarget::kNpu);
            EXPECT_TRUE(adapter.SessionArtifact()->fallbackReason.empty());
            EXPECT_NE(adapter.DescribeSession().find("backend=windows-ml"), std::string::npos);
        }

        TEST(BackendPlannerTest, WinMlAdapterFallsBackWhenNpuIsMissingButCpuFallbackIsAllowed)
        {
            backends::HardwareCapabilities capabilities{};
            capabilities.hasNpu = false;
            capabilities.hasVulkan = true;
            capabilities.primaryAdapterName = "Radeon RX Test";
            capabilities.primaryAdapterVendor = backends::BackendVendor::kAmd;
            capabilities.primaryAdapterClass = backends::DeviceClass::kDiscreteGpu;
            capabilities.budget.deviceBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            backends::BackendDescriptor backend{};
            backend.kind = backends::BackendKind::kWindowsMl;
            backend.name = "windows-ml";
            backend.displayName = "Windows ML";
            backend.deviceClass = backends::DeviceClass::kNpu;
            backend.vendor = backends::BackendVendor::kMicrosoft;
            backend.defaultPrecision = backends::PrecisionMode::kInt8;
            backend.supportsNpuOffload = true;
            backend.requiresOptIn = true;
            backend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            backends::windows_ml::WinMlAdapter adapter({
                .allowCpuFallback = true,
                .preferStaticShapes = true,
                .enableTelemetry = true,
            });
            ASSERT_TRUE(adapter.Initialize(capabilities));

            const auto optInPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding = MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kMicro, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });

            ASSERT_TRUE(adapter.CompileGraph("qwen-0.5b", optInPlan));
            ASSERT_TRUE(adapter.SessionArtifact().has_value());
            EXPECT_EQ(adapter.State(), backends::windows_ml::WinMlAdapterState::kCompiled);
            EXPECT_EQ(adapter.SessionArtifact()->compileTarget,
                      backends::windows_ml::WinMlCompileTarget::kCpuFallback);
            EXPECT_EQ(adapter.SessionArtifact()->fallbackReason, "npu-unavailable");
            EXPECT_EQ(adapter.Stats().npuPartitionCount, 0U);
            EXPECT_EQ(adapter.Stats().hostPartitionCount,
                      static_cast<std::uint32_t>(optInPlan.partitions.size()));
            EXPECT_GE(adapter.Stats().cpuFallbackPartitionCount, 1U);
        }

        TEST(BackendPlannerTest, LayerOffloaderBuildsMixedDispatchTable)
        {
            backends::BackendDescriptor backend{};
            backend.kind = backends::BackendKind::kWindowsMl;
            backend.name = "windows-ml";
            backend.displayName = "Windows ML";
            backend.deviceClass = backends::DeviceClass::kNpu;
            backend.vendor = backends::BackendVendor::kMicrosoft;
            backend.defaultPrecision = backends::PrecisionMode::kInt8;
            backend.supportsNpuOffload = true;
            backend.requiresOptIn = true;
            backend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            const auto plan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding = MakeBinding(backend, "qwen-0.5b", backends::RuntimeMode::kMicro, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });

            const std::vector<backends::windows_ml::LayerDescriptor> layers = {
                {
                    .name = "embed",
                    .kind = backends::windows_ml::LayerKind::kEmbedding,
                },
                {
                    .name = "prefill.ffn",
                    .kind = backends::windows_ml::LayerKind::kDensePrefill,
                },
                {
                    .name = "decode.ffn",
                    .kind = backends::windows_ml::LayerKind::kDenseDecode,
                    .latencySensitive = true,
                },
                {
                    .name = "attention",
                    .kind = backends::windows_ml::LayerKind::kAttention,
                },
                {
                    .name = "kv-compress",
                    .kind = backends::windows_ml::LayerKind::kKvCompression,
                },
            };

            const auto dispatchTable =
                backends::windows_ml::LayerOffloader::BuildDispatchTable(plan, layers);
            ASSERT_EQ(dispatchTable.size(), layers.size());
            EXPECT_EQ(dispatchTable[0].target, backends::windows_ml::LayerExecutionTarget::kNpu);
            EXPECT_EQ(dispatchTable[1].partitionKind,
                      backends::windows_ml::WindowsMlPartitionKind::kPrefillEncoder);
            EXPECT_EQ(dispatchTable[2].partitionKind,
                      backends::windows_ml::WindowsMlPartitionKind::kDecodeProjection);
            EXPECT_EQ(dispatchTable[3].target, backends::windows_ml::LayerExecutionTarget::kCpu);
            EXPECT_EQ(dispatchTable[4].target,
                      backends::windows_ml::LayerExecutionTarget::kHostAssist);
        }

        TEST(BackendPlannerTest, MixedDispatchPlannerCombinesGpuAndNpuSlices)
        {
            backends::BackendDescriptor gpuBackend{};
            gpuBackend.kind = backends::BackendKind::kVulkan;
            gpuBackend.name = "vulkan";
            gpuBackend.displayName = "Vulkan Compute";
            gpuBackend.deviceClass = backends::DeviceClass::kDiscreteGpu;
            gpuBackend.vendor = backends::BackendVendor::kAmd;
            gpuBackend.defaultPrecision = backends::PrecisionMode::kFp16;
            gpuBackend.supportsPagedKv = true;
            gpuBackend.supportsSpeculative = true;
            gpuBackend.maxContextTokensHint = 8192U;

            backends::BackendDescriptor npuBackend{};
            npuBackend.kind = backends::BackendKind::kWindowsMl;
            npuBackend.name = "windows-ml";
            npuBackend.displayName = "Windows ML";
            npuBackend.deviceClass = backends::DeviceClass::kNpu;
            npuBackend.vendor = backends::BackendVendor::kMicrosoft;
            npuBackend.defaultPrecision = backends::PrecisionMode::kInt8;
            npuBackend.supportsNpuOffload = true;
            npuBackend.requiresOptIn = true;
            npuBackend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            const auto gpuPlan = backends::vulkan::BuildVulkanExecutionPlan({
                .binding = MakeBinding(gpuBackend, "qwen-0.5b", backends::RuntimeMode::kBalanced),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsMoE = false,
                .modelSupportsSpeculative = true,
            });
            const auto npuPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(npuBackend, "qwen-0.5b", backends::RuntimeMode::kMicro, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });

            const std::vector<backends::windows_ml::LayerDescriptor> layers = {
                {.name = "prefill.ffn", .kind = backends::windows_ml::LayerKind::kDensePrefill},
                {.name = "decode.ffn", .kind = backends::windows_ml::LayerKind::kDenseDecode},
                {.name = "attention", .kind = backends::windows_ml::LayerKind::kAttention},
                {.name = "kv-compress", .kind = backends::windows_ml::LayerKind::kKvCompression},
            };

            const auto mixedPlan =
                backends::windows_ml::MixedDispatchPlanner::Build(gpuPlan, npuPlan, layers);
            EXPECT_TRUE(mixedPlan.gpuPrimaryActive);
            EXPECT_TRUE(mixedPlan.npuDenseActive);
            EXPECT_TRUE(mixedPlan.hostAssistRequired);
            ASSERT_EQ(mixedPlan.slices.size(), layers.size());
            EXPECT_EQ(mixedPlan.slices[0].target,
                      backends::windows_ml::MixedDispatchTarget::kNpuDense);
            EXPECT_EQ(mixedPlan.slices[2].target,
                      backends::windows_ml::MixedDispatchTarget::kCpuFallback);
            EXPECT_EQ(mixedPlan.slices[3].target,
                      backends::windows_ml::MixedDispatchTarget::kHostAssist);
        }

        TEST(BackendPlannerTest, MixedDispatchPlannerSuppressesNpuDenseWhenCompiledFallbackIsArmed)
        {
            backends::BackendDescriptor gpuBackend{};
            gpuBackend.kind = backends::BackendKind::kVulkan;
            gpuBackend.name = "vulkan";
            gpuBackend.displayName = "Vulkan Compute";
            gpuBackend.deviceClass = backends::DeviceClass::kDiscreteGpu;
            gpuBackend.vendor = backends::BackendVendor::kAmd;
            gpuBackend.defaultPrecision = backends::PrecisionMode::kFp16;
            gpuBackend.supportsPagedKv = true;
            gpuBackend.supportsSpeculative = true;
            gpuBackend.maxContextTokensHint = 8192U;

            backends::BackendDescriptor npuBackend{};
            npuBackend.kind = backends::BackendKind::kWindowsMl;
            npuBackend.name = "windows-ml";
            npuBackend.displayName = "Windows ML";
            npuBackend.deviceClass = backends::DeviceClass::kNpu;
            npuBackend.vendor = backends::BackendVendor::kMicrosoft;
            npuBackend.defaultPrecision = backends::PrecisionMode::kInt8;
            npuBackend.supportsNpuOffload = true;
            npuBackend.requiresOptIn = true;
            npuBackend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            const auto gpuPlan = backends::vulkan::BuildVulkanExecutionPlan({
                .binding = MakeBinding(gpuBackend, "qwen-0.5b", backends::RuntimeMode::kBalanced),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsMoE = false,
                .modelSupportsSpeculative = true,
            });
            const auto npuPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(npuBackend, "qwen-0.5b", backends::RuntimeMode::kMicro, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });

            const std::vector<backends::windows_ml::LayerDescriptor> layers = {
                {.name = "embed", .kind = backends::windows_ml::LayerKind::kEmbedding},
                {.name = "prefill.ffn", .kind = backends::windows_ml::LayerKind::kDensePrefill},
                {.name = "decode.ffn",
                 .kind = backends::windows_ml::LayerKind::kDenseDecode,
                 .latencySensitive = true},
                {.name = "attention", .kind = backends::windows_ml::LayerKind::kAttention},
            };

            const backends::windows_ml::WinMlSessionArtifact artifact{
                .modelId = "qwen-0.5b",
                .compileTarget = backends::windows_ml::WinMlCompileTarget::kCpuFallback,
                .batchHint = 2U,
                .contextHint = 4096U,
                .npuPartitionCount = 0U,
                .hostPartitionCount = 4U,
                .cpuFallbackPartitionCount = 1U,
                .cpuFallbackArmed = true,
                .hostAssistRequired = true,
                .reusableGraph = true,
                .requiresStaticShapes = true,
                .fallbackReason = "npu-unavailable",
                .cacheKey = "qwen-0.5b|cpu-fallback|b2|c4096",
            };

            const auto mixedPlan = backends::windows_ml::MixedDispatchPlanner::Build(
                gpuPlan, npuPlan, layers, {}, &artifact);
            EXPECT_TRUE(mixedPlan.gpuPrimaryActive);
            EXPECT_FALSE(mixedPlan.npuDenseActive);
            EXPECT_TRUE(mixedPlan.cpuFallbackPresent);
            EXPECT_FALSE(mixedPlan.degradedByPolicy);
            ASSERT_EQ(mixedPlan.slices.size(), layers.size());
            EXPECT_EQ(mixedPlan.slices[0].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
            EXPECT_EQ(mixedPlan.slices[1].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
            EXPECT_EQ(mixedPlan.slices[2].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
        }

        TEST(BackendPlannerTest, PowerThermalMonitorBuildsSyntheticBatterySnapshot)
        {
            ClearPowerTelemetryEnv();
#if defined(_WIN32)
            _putenv_s("US4_POWER_SOURCE", "battery");
            _putenv_s("US4_BATTERY_PERCENT", "19");
            _putenv_s("US4_BATTERY_SAVER", "1");
            _putenv_s("US4_THERMAL_STATE", "warm");
            _putenv_s("US4_ETW_THROTTLED", "0");
#endif

            const auto snapshot = backends::windows_ml::PowerThermalMonitor::Sample();
            EXPECT_EQ(snapshot.powerSource, backends::windows_ml::PowerSource::kBattery);
            EXPECT_EQ(snapshot.batteryPercent, 19U);
            EXPECT_TRUE(snapshot.batterySaverActive);
            EXPECT_EQ(snapshot.thermalState, backends::windows_ml::ThermalState::kWarm);
            EXPECT_TRUE(snapshot.usingSyntheticTelemetry);
            EXPECT_EQ(backends::windows_ml::PowerThermalMonitor::SelectPolicy(snapshot),
                      backends::windows_ml::PowerDispatchPolicy::kPreferEfficiency);

            ClearPowerTelemetryEnv();
        }

        TEST(BackendPlannerTest, MixedDispatchPlannerDemotesDecodeInEfficiencyMode)
        {
            backends::BackendDescriptor gpuBackend{};
            gpuBackend.kind = backends::BackendKind::kVulkan;
            gpuBackend.name = "vulkan";
            gpuBackend.displayName = "Vulkan Compute";
            gpuBackend.deviceClass = backends::DeviceClass::kDiscreteGpu;
            gpuBackend.vendor = backends::BackendVendor::kAmd;
            gpuBackend.defaultPrecision = backends::PrecisionMode::kFp16;
            gpuBackend.supportsPagedKv = true;
            gpuBackend.supportsSpeculative = true;
            gpuBackend.maxContextTokensHint = 8192U;

            backends::BackendDescriptor npuBackend{};
            npuBackend.kind = backends::BackendKind::kWindowsMl;
            npuBackend.name = "windows-ml";
            npuBackend.displayName = "Windows ML";
            npuBackend.deviceClass = backends::DeviceClass::kNpu;
            npuBackend.vendor = backends::BackendVendor::kMicrosoft;
            npuBackend.defaultPrecision = backends::PrecisionMode::kInt8;
            npuBackend.supportsNpuOffload = true;
            npuBackend.requiresOptIn = true;
            npuBackend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            const auto gpuPlan = backends::vulkan::BuildVulkanExecutionPlan({
                .binding = MakeBinding(gpuBackend, "qwen-0.5b", backends::RuntimeMode::kBalanced),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsMoE = false,
                .modelSupportsSpeculative = true,
            });
            const auto npuPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(npuBackend, "qwen-0.5b", backends::RuntimeMode::kMicro, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });

            const std::vector<backends::windows_ml::LayerDescriptor> layers = {
                {.name = "prefill.ffn", .kind = backends::windows_ml::LayerKind::kDensePrefill},
                {.name = "decode.ffn",
                 .kind = backends::windows_ml::LayerKind::kDenseDecode,
                 .latencySensitive = true},
            };

            const backends::windows_ml::PowerThermalSnapshot snapshot{
                .powerSource = backends::windows_ml::PowerSource::kBattery,
                .thermalState = backends::windows_ml::ThermalState::kWarm,
                .batteryPercent = 20U,
                .batterySaverActive = true,
            };

            const auto mixedPlan = backends::windows_ml::MixedDispatchPlanner::Build(
                gpuPlan, npuPlan, layers, snapshot);
            EXPECT_EQ(mixedPlan.policy,
                      backends::windows_ml::PowerDispatchPolicy::kPreferEfficiency);
            EXPECT_TRUE(mixedPlan.degradedByPolicy);
            EXPECT_TRUE(mixedPlan.npuDenseActive);
            EXPECT_EQ(mixedPlan.npuDemotionCount, 1U);
            ASSERT_EQ(mixedPlan.slices.size(), layers.size());
            EXPECT_EQ(mixedPlan.slices[0].target,
                      backends::windows_ml::MixedDispatchTarget::kNpuDense);
            EXPECT_EQ(mixedPlan.slices[1].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
        }

        TEST(BackendPlannerTest, MixedDispatchPlannerDemotesAllNpuDenseSlicesWhenThrottled)
        {
            backends::BackendDescriptor gpuBackend{};
            gpuBackend.kind = backends::BackendKind::kVulkan;
            gpuBackend.name = "vulkan";
            gpuBackend.displayName = "Vulkan Compute";
            gpuBackend.deviceClass = backends::DeviceClass::kDiscreteGpu;
            gpuBackend.vendor = backends::BackendVendor::kAmd;
            gpuBackend.defaultPrecision = backends::PrecisionMode::kFp16;
            gpuBackend.supportsPagedKv = true;
            gpuBackend.supportsSpeculative = true;
            gpuBackend.maxContextTokensHint = 8192U;

            backends::BackendDescriptor npuBackend{};
            npuBackend.kind = backends::BackendKind::kWindowsMl;
            npuBackend.name = "windows-ml";
            npuBackend.displayName = "Windows ML";
            npuBackend.deviceClass = backends::DeviceClass::kNpu;
            npuBackend.vendor = backends::BackendVendor::kMicrosoft;
            npuBackend.defaultPrecision = backends::PrecisionMode::kInt8;
            npuBackend.supportsNpuOffload = true;
            npuBackend.requiresOptIn = true;
            npuBackend.maxContextTokensHint = 4096U;

            backends::SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferredBackend = "windows-ml";
            request.maxContextTokens = 4096U;
            request.preferLowLatency = true;

            const auto gpuPlan = backends::vulkan::BuildVulkanExecutionPlan({
                .binding = MakeBinding(gpuBackend, "qwen-0.5b", backends::RuntimeMode::kBalanced),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsMoE = false,
                .modelSupportsSpeculative = true,
            });
            const auto npuPlan = backends::windows_ml::BuildWindowsMlExecutionPlan({
                .binding =
                    MakeBinding(npuBackend, "qwen-0.5b", backends::RuntimeMode::kMicro, true),
                .request = request,
                .adapterId = "dense-qwen",
                .targetBatchSize = 2U,
                .modelSupportsSpeculative = false,
            });

            const std::vector<backends::windows_ml::LayerDescriptor> layers = {
                {.name = "embed", .kind = backends::windows_ml::LayerKind::kEmbedding},
                {.name = "prefill.ffn", .kind = backends::windows_ml::LayerKind::kDensePrefill},
                {.name = "decode.ffn",
                 .kind = backends::windows_ml::LayerKind::kDenseDecode,
                 .latencySensitive = true},
                {.name = "attention", .kind = backends::windows_ml::LayerKind::kAttention},
            };

            const backends::windows_ml::PowerThermalSnapshot snapshot{
                .powerSource = backends::windows_ml::PowerSource::kAc,
                .thermalState = backends::windows_ml::ThermalState::kThrottled,
                .batteryPercent = 100U,
                .batterySaverActive = false,
                .etwThrottleActive = true,
            };

            const auto mixedPlan = backends::windows_ml::MixedDispatchPlanner::Build(
                gpuPlan, npuPlan, layers, snapshot);
            EXPECT_EQ(mixedPlan.policy,
                      backends::windows_ml::PowerDispatchPolicy::kThermalThrottle);
            EXPECT_TRUE(mixedPlan.degradedByPolicy);
            EXPECT_FALSE(mixedPlan.npuDenseActive);
            EXPECT_EQ(mixedPlan.npuDemotionCount, 3U);
            ASSERT_EQ(mixedPlan.slices.size(), layers.size());
            EXPECT_EQ(mixedPlan.slices[0].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
            EXPECT_EQ(mixedPlan.slices[1].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
            EXPECT_EQ(mixedPlan.slices[2].target,
                      backends::windows_ml::MixedDispatchTarget::kGpuPrimary);
            EXPECT_EQ(mixedPlan.slices[3].target,
                      backends::windows_ml::MixedDispatchTarget::kCpuFallback);
        }

        TEST(BackendPlannerTest, CpuKernelAndOneDnnPlansStayConsistent)
        {
            const auto profile = backends::cpu_avx::BuildKernelProfile({
                .avx2 = true,
                .avx512f = true,
                .avx512Vnni = true,
                .amxInt8 = true,
                .amxBf16 = true,
                .f16c = true,
            });
            EXPECT_EQ(profile.level, backends::cpu_avx::CpuInstructionSetLevel::kAmx);
            EXPECT_TRUE(profile.prefersOneDnnBlockGemm);

            const auto oneDnnPlan = backends::onednn::BuildOneDnnMatMulPlan(
                {
                    .rows = 128U,
                    .inner = 256U,
                    .cols = 192U,
                    .leftType = backends::onednn::OneDnnDataType::kFloat32,
                    .rightType = backends::onednn::OneDnnDataType::kInt8,
                    .outputType = backends::onednn::OneDnnDataType::kFloat32,
                    .prepackWeights = true,
                },
                {
                    .allowAmx = true,
                    .allowAvx512 = true,
                    .threadCountHint = 8U,
                });
            EXPECT_TRUE(oneDnnPlan.valid);
            EXPECT_TRUE(oneDnnPlan.useAmxTiles);

            us4::core::Tensor left({32U, 64U});
            us4::core::Tensor right({64U, 48U});
            left.Fill(0.5F);
            right.Fill(0.25F);
            const auto matmulPlan = backends::cpu_avx::ExplainScalarMatMulPlan(left, right);
            EXPECT_TRUE(matmulPlan.packRightHandSide);
            EXPECT_GE(matmulPlan.tile.depthBlock, 16U);
        }

        TEST(BackendPlannerTest, OneDnnBackendMatchesScalarReferenceMatMul)
        {
            us4::core::Tensor left({32U, 64U});
            us4::core::Tensor right({64U, 48U});
            for (std::size_t index = 0; index < left.ElementCount(); ++index)
            {
                left[index] = 0.125F + static_cast<float>((index % 17U)) * 0.03125F;
            }
            for (std::size_t index = 0; index < right.ElementCount(); ++index)
            {
                right[index] = -0.25F + static_cast<float>((index % 13U)) * 0.0625F;
            }

            const auto scalarProfile = backends::cpu_avx::BuildKernelProfile({
                .avx2 = false,
                .avx512f = false,
                .amxInt8 = false,
                .amxBf16 = false,
                .hardwareThreadCount = 1U,
            });
            const auto avx512Profile = backends::cpu_avx::BuildKernelProfile({
                .avx2 = true,
                .avx512f = true,
                .avx512Vnni = true,
                .l2BytesPerCore = 2048U * 1024U,
                .hardwareThreadCount = 8U,
            });

            const auto scalarPlan = backends::cpu_avx::MakeReferenceMatMulPlan(
                left.Dim(0), left.Dim(1), right.Dim(1), &scalarProfile);
            const auto scalarOutput = backends::cpu_avx::BlockedMatMul(left, right, scalarPlan);

            backends::onednn::OneDnnBackend backend({
                .allowAmx = true,
                .allowAvx512 = true,
                .preferPersistentCache = true,
                .l2BytesPerCore = 2048U * 1024U,
                .threadCountHint = 8U,
            });
            const auto result = backend.ExecuteMatMul(left, right, &avx512Profile);

            EXPECT_TRUE(result.plan.valid);
            EXPECT_TRUE(result.blockedPlan.useOneDnnFriendlyBlocking);
            EXPECT_NE(result.blockedPlan.kernelTag.find("onednn-"), std::string::npos);
            EXPECT_EQ(result.output.Shape(), scalarOutput.Shape());
            EXPECT_EQ(result.output.ElementCount(), scalarOutput.ElementCount());
            for (std::size_t index = 0; index < result.output.ElementCount(); ++index)
            {
                EXPECT_NEAR(result.output[index], scalarOutput[index], 1e-3F);
            }
        }

        TEST(BackendPlannerTest, CpuFallbackTracksDetectedInstructionSetLevel)
        {
            backends::HardwareCapabilities avx2Capabilities{};
            avx2Capabilities.hasAvx2 = true;
            avx2Capabilities.cpuName = "AMD Ryzen Test";
            avx2Capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            const auto avx2Backend = backends::BackendSelector::SelectCpuFallback(avx2Capabilities);
            EXPECT_EQ(avx2Backend.name, "cpu-avx2");
            EXPECT_EQ(avx2Backend.displayName, "CPU (AVX2)");
            EXPECT_EQ(avx2Backend.vendor, backends::BackendVendor::kAmd);
            EXPECT_EQ(avx2Backend.defaultPrecision, backends::PrecisionMode::kFp32);

            backends::HardwareCapabilities avx512Capabilities = avx2Capabilities;
            avx512Capabilities.hasAvx512 = true;
            avx512Capabilities.cpuName = "Intel Xeon Test";

            const auto avx512Backend =
                backends::BackendSelector::SelectCpuFallback(avx512Capabilities);
            EXPECT_EQ(avx512Backend.name, "cpu-avx512");
            EXPECT_EQ(avx512Backend.displayName, "CPU (AVX-512)");
            EXPECT_EQ(avx512Backend.vendor, backends::BackendVendor::kIntel);
            EXPECT_EQ(avx512Backend.defaultPrecision, backends::PrecisionMode::kFp32);

            backends::HardwareCapabilities amxCapabilities = avx512Capabilities;
            amxCapabilities.hasAmx = true;

            const auto amxBackend = backends::BackendSelector::SelectCpuFallback(amxCapabilities);
            EXPECT_EQ(amxBackend.name, "cpu-amx");
            EXPECT_EQ(amxBackend.displayName, "CPU (AMX)");
            EXPECT_EQ(amxBackend.vendor, backends::BackendVendor::kIntel);
            EXPECT_EQ(amxBackend.defaultPrecision, backends::PrecisionMode::kBf16);
        }
    } // namespace
} // namespace us4::runtime::tests
