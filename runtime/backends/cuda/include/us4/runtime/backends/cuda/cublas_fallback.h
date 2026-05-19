#pragma once

#include "us4/runtime/backends/cuda/cuda_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace us4::runtime::backends::cuda
{

    enum class CudaMatmulPrimitive
    {
        kCustomKernel,
        kCublas,
        kCublasLt,
    };

    struct CudaMatmulShape
    {
        std::size_t rows = 0U;
        std::size_t columns = 0U;
        std::size_t innerDimension = 0U;
        std::uint32_t batchCount = 1U;
        bool transposeWeights = false;
        bool preferDeterministic = false;
        bool useFp16 = false;
        bool useBf16 = false;
        bool allowCustomKernel = true;
    };

    struct CudaMatmulDispatch
    {
        CudaMatmulPrimitive primary = CudaMatmulPrimitive::kCublas;
        CudaMatmulPrimitive fallback = CudaMatmulPrimitive::kCublas;
        bool customKernelEligible = false;
        bool graphFriendly = false;
        double estimatedLatencyUs = 0.0;
        double estimatedFallbackLatencyUs = 0.0;
        double estimatedSpeedupVsFallback = 1.0;
        std::string policyReason;
        std::vector<RuntimeIssue> issues;
    };

    [[nodiscard]] const char* ToString(CudaMatmulPrimitive value);

    class CublasFallbackWrapper
    {
      public:
        [[nodiscard]] static CudaMatmulDispatch SelectMatmulDispatch(const CudaExecutionPlan& plan,
                                                                     const CudaMatmulShape& shape);
    };

} // namespace us4::runtime::backends::cuda
