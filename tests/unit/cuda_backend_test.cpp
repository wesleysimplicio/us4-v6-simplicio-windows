#include "us4/runtime/backends/cuda/cuda_backend.h"

#include <gtest/gtest.h>

namespace us4::runtime::backends::cuda
{
    namespace
    {

        TEST(CudaBackendTest, BuildsReferencePlanForDetectedCudaAdapter)
        {
            HardwareCapabilities capabilities{};
            capabilities.hasCuda = true;
            capabilities.hasUnifiedMemory = true;
            capabilities.primaryAdapterName = "NVIDIA RTX 4090";
            capabilities.primaryAdapterVendor = BackendVendor::kNvidia;
            capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
            capabilities.discreteGpuCount = 1U;
            capabilities.budget.deviceBytes = 24ULL * 1024ULL * 1024ULL * 1024ULL;
            capabilities.budget.hostBytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;

            SessionRequest request{};
            request.modelId = "qwen-0.5b";
            request.preferMaxThroughput = true;
            request.maxContextTokens = 8192U;
            request.maxGenerationTokens = 128U;

            const CudaExecutionPlan plan = CudaBackend::BuildExecutionPlan(request, capabilities);
            const CudaStepResult prefill = CudaBackend::PreparePrefill(plan, "hello from cuda");
            const CudaStepResult decode = CudaBackend::PrepareDecode(plan, prefill.chunk, 0U);

            EXPECT_EQ(plan.descriptor.kind, BackendKind::kCuda);
            EXPECT_EQ(plan.device.architecture, CudaArchitectureTier::kAda);
            EXPECT_GE(plan.recommendedBatchSize, 1U);
            EXPECT_FALSE(prefill.chunk.tokens.empty());
            EXPECT_FALSE(decode.chunk.tokens.empty());
        }

    } // namespace

} // namespace us4::runtime::backends::cuda
