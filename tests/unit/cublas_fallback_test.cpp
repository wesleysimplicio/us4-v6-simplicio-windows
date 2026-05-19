#include "us4/runtime/backends/cuda/cublas_fallback.h"
#include "us4/runtime/backends/cuda/cuda_backend.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;

        us4::runtime::backends::HardwareCapabilities MakeCudaCapabilities()
        {
            using namespace us4::runtime::backends;

            HardwareCapabilities capabilities;
            capabilities.hasCuda = true;
            capabilities.primaryAdapterVendor = BackendVendor::kNvidia;
            capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
            capabilities.primaryAdapterName = "GeForce RTX 4090";
            capabilities.discreteGpuCount = 1U;
            capabilities.hasUnifiedMemory = true;
            capabilities.budget.hostBytes = 64ULL * kGiB;
            capabilities.budget.deviceBytes = 24ULL * kGiB;
            capabilities.budget.storageBytes = 2ULL * 1024ULL * kGiB;
            return capabilities;
        }

        us4::runtime::backends::SessionRequest MakeCudaRequest()
        {
            using namespace us4::runtime::backends;

            SessionRequest request;
            request.modelId = "qwen-0.5b";
            request.mode = RuntimeMode::kBalanced;
            request.maxContextTokens = 4096U;
            request.maxGenerationTokens = 64U;
            request.preferMaxThroughput = true;
            request.seed = 77U;
            return request;
        }

        TEST(CublasFallbackTest, SelectsCustomKernelForAlignedFp16Shapes)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            const CudaMatmulShape shape{
                .rows = 512U,
                .columns = 512U,
                .innerDimension = 256U,
                .batchCount = 1U,
                .transposeWeights = false,
                .preferDeterministic = false,
                .useFp16 = true,
                .useBf16 = false,
                .allowCustomKernel = true,
            };

            const auto dispatch = CublasFallbackWrapper::SelectMatmulDispatch(plan, shape);
            EXPECT_EQ(dispatch.primary, CudaMatmulPrimitive::kCustomKernel);
            EXPECT_TRUE(dispatch.customKernelEligible);
            EXPECT_TRUE(dispatch.graphFriendly);
            EXPECT_GT(dispatch.estimatedSpeedupVsFallback, 1.0);
        }

        TEST(CublasFallbackTest, FallsBackToCublasLtForBatchedIrregularShapes)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            const CudaMatmulShape shape{
                .rows = 1536U,
                .columns = 1792U,
                .innerDimension = 960U,
                .batchCount = 8U,
                .transposeWeights = true,
                .preferDeterministic = false,
                .useFp16 = true,
                .useBf16 = false,
                .allowCustomKernel = true,
            };

            const auto dispatch = CublasFallbackWrapper::SelectMatmulDispatch(plan, shape);
            EXPECT_EQ(dispatch.primary, CudaMatmulPrimitive::kCublasLt);
            EXPECT_EQ(dispatch.fallback, CudaMatmulPrimitive::kCublas);
            EXPECT_FALSE(dispatch.customKernelEligible);
            ASSERT_FALSE(dispatch.issues.empty());
            EXPECT_EQ(dispatch.issues.front().code, "cuda.matmul.library_fallback");
        }

        TEST(CublasFallbackTest, DeterministicShapesPreferCublasPolicy)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            const CudaMatmulShape shape{
                .rows = 512U,
                .columns = 256U,
                .innerDimension = 256U,
                .batchCount = 1U,
                .transposeWeights = false,
                .preferDeterministic = true,
                .useFp16 = true,
                .useBf16 = false,
                .allowCustomKernel = true,
            };

            const auto dispatch = CublasFallbackWrapper::SelectMatmulDispatch(plan, shape);
            EXPECT_EQ(dispatch.primary, CudaMatmulPrimitive::kCublas);
            EXPECT_EQ(dispatch.fallback, CudaMatmulPrimitive::kCublasLt);
            EXPECT_FALSE(dispatch.customKernelEligible);
            EXPECT_EQ(dispatch.policyReason, "small_or_deterministic_shape_prefers_cublas");
        }
    } // namespace
} // namespace us4::runtime::tests
