#include "runtime/core/tensor.h"
#include "us4/runtime/adapters/adapter_contracts.h"
#include "us4/runtime/backends/cpu_avx/kernel_profile.h"
#include "us4/runtime/backends/cpu_avx/scalar_matmul.h"
#include "us4/runtime/backends/directml/dml_device.h"
#include "us4/runtime/backends/directml/dml_graph.h"
#include "us4/runtime/backends/onednn/block_gemm_contract.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"
#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
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
    } // namespace
} // namespace us4::runtime::tests
