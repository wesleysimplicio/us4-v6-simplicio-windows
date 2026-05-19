#include "us4/runtime/backends/cuda/cublas_fallback.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(CublasFallbackTest, BatchedRequestsPreferCublasLt)
    {
        const backends::cuda::CublasDispatchRequest req{.rowsLeft = 128U,
                                                          .colsLeft = 128U,
                                                          .colsRight = 128U,
                                                          .batchSize = 8U};
        const auto decision = backends::cuda::SelectCublasPath(req);
        EXPECT_EQ(decision.path, backends::cuda::CublasPath::kCublasLt);
    }

    TEST(CublasFallbackTest, EmptyShapeFallsBackToHost)
    {
        const backends::cuda::CublasDispatchRequest req{};
        const auto decision = backends::cuda::SelectCublasPath(req);
        EXPECT_EQ(decision.path, backends::cuda::CublasPath::kHostFallback);
    }

    TEST(CublasFallbackTest, SmallShapesPreferLegacyCublas)
    {
        const backends::cuda::CublasDispatchRequest req{.rowsLeft = 16U,
                                                          .colsLeft = 16U,
                                                          .colsRight = 16U};
        const auto decision = backends::cuda::SelectCublasPath(req);
        EXPECT_EQ(decision.path, backends::cuda::CublasPath::kCublasLegacy);
    }

    TEST(CublasFallbackTest, LargeFp16PreferCublasLt)
    {
        const backends::cuda::CublasDispatchRequest req{.rowsLeft = 512U,
                                                          .colsLeft = 512U,
                                                          .colsRight = 512U,
                                                          .useFp16Accumulate = true};
        const auto decision = backends::cuda::SelectCublasPath(req);
        EXPECT_EQ(decision.path, backends::cuda::CublasPath::kCublasLt);
    }

    TEST(CublasFallbackTest, ToStringCoversAllPaths)
    {
        EXPECT_EQ(backends::cuda::ToString(backends::cuda::CublasPath::kCustomKernel),
                  "custom_kernel");
        EXPECT_EQ(backends::cuda::ToString(backends::cuda::CublasPath::kCublasLt), "cublasLt");
        EXPECT_EQ(backends::cuda::ToString(backends::cuda::CublasPath::kCublasLegacy),
                  "cublas_legacy");
        EXPECT_EQ(backends::cuda::ToString(backends::cuda::CublasPath::kHostFallback),
                  "host_fallback");
    }

} // namespace us4::runtime::tests
