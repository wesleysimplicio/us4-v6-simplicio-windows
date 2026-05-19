#include "us4/runtime/backends/cuda/cublas_fallback.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace us4::runtime::backends::cuda
{

    namespace
    {
        [[nodiscard]] double EstimatePrimitiveLatencyUs(const CudaMatmulShape& shape,
                                                        const CudaMatmulPrimitive primitive)
        {
            const double workload =
                static_cast<double>(shape.rows) * static_cast<double>(shape.columns) *
                static_cast<double>(shape.innerDimension) * std::max(1U, shape.batchCount);
            const double precisionFactor = shape.useBf16 ? 0.92 : (shape.useFp16 ? 0.88 : 1.15);

            double divisor = 0.0;
            switch (primitive)
            {
            case CudaMatmulPrimitive::kCustomKernel:
                divisor = shape.batchCount > 1U ? 39000000.0 : 36000000.0;
                break;
            case CudaMatmulPrimitive::kCublas:
                divisor = shape.batchCount > 1U ? 26000000.0 : 22000000.0;
                break;
            case CudaMatmulPrimitive::kCublasLt:
                divisor = shape.batchCount > 1U ? 34000000.0 : 29500000.0;
                break;
            }

            const double launchPenaltyUs = primitive == CudaMatmulPrimitive::kCustomKernel ? 8.0
                                           : primitive == CudaMatmulPrimitive::kCublasLt   ? 15.0
                                                                                           : 18.0;
            return std::max(launchPenaltyUs,
                            (workload / divisor) * precisionFactor + launchPenaltyUs);
        }

        [[nodiscard]] bool IsCustomKernelEligible(const CudaExecutionPlan& plan,
                                                  const CudaMatmulShape& shape)
        {
            if (!shape.allowCustomKernel || shape.preferDeterministic)
            {
                return false;
            }
            if (!(shape.useFp16 || shape.useBf16))
            {
                return false;
            }
            if (shape.transposeWeights)
            {
                return false;
            }
            if (shape.rows == 0U || shape.columns == 0U || shape.innerDimension == 0U)
            {
                return false;
            }

            const bool tileAligned = (shape.rows % 64U == 0U) && (shape.columns % 64U == 0U) &&
                                     (shape.innerDimension % 32U == 0U);
            return tileAligned && plan.device.computeCapabilityMajor >= 8U;
        }

        [[nodiscard]] bool ShouldPreferCublasLt(const CudaMatmulShape& shape)
        {
            return shape.batchCount > 1U || shape.transposeWeights ||
                   (shape.rows >= 2048U && shape.columns >= 2048U);
        }
    } // namespace

    const char* ToString(const CudaMatmulPrimitive value)
    {
        switch (value)
        {
        case CudaMatmulPrimitive::kCustomKernel:
            return "custom-kernel";
        case CudaMatmulPrimitive::kCublas:
            return "cublas";
        case CudaMatmulPrimitive::kCublasLt:
            return "cublaslt";
        }
        return "cublas";
    }

    CudaMatmulDispatch CublasFallbackWrapper::SelectMatmulDispatch(const CudaExecutionPlan& plan,
                                                                   const CudaMatmulShape& shape)
    {
        CudaMatmulDispatch dispatch;
        dispatch.customKernelEligible = IsCustomKernelEligible(plan, shape);
        dispatch.graphFriendly = dispatch.customKernelEligible && plan.graph.enableGraphCapture &&
                                 shape.batchCount == 1U;

        if (dispatch.customKernelEligible)
        {
            dispatch.primary = CudaMatmulPrimitive::kCustomKernel;
            dispatch.fallback = ShouldPreferCublasLt(shape) ? CudaMatmulPrimitive::kCublasLt
                                                            : CudaMatmulPrimitive::kCublas;
            dispatch.policyReason =
                "shape_aligned_fp16_or_bf16_kernel_uses_custom_path_before_library_fallback";
        }
        else if (ShouldPreferCublasLt(shape))
        {
            dispatch.primary = CudaMatmulPrimitive::kCublasLt;
            dispatch.fallback = CudaMatmulPrimitive::kCublas;
            dispatch.policyReason = "irregular_or_batched_shape_prefers_cublaslt";
        }
        else
        {
            dispatch.primary = CudaMatmulPrimitive::kCublas;
            dispatch.fallback = CudaMatmulPrimitive::kCublasLt;
            dispatch.policyReason = "small_or_deterministic_shape_prefers_cublas";
        }

        dispatch.estimatedLatencyUs = EstimatePrimitiveLatencyUs(shape, dispatch.primary);
        dispatch.estimatedFallbackLatencyUs = EstimatePrimitiveLatencyUs(shape, dispatch.fallback);
        dispatch.estimatedSpeedupVsFallback =
            dispatch.estimatedLatencyUs > 0.0
                ? dispatch.estimatedFallbackLatencyUs / dispatch.estimatedLatencyUs
                : 1.0;

        if (!dispatch.customKernelEligible)
        {
            dispatch.issues.push_back(
                {"cuda.matmul.library_fallback",
                 "The requested shape does not stay on the custom CUDA kernel path.", false});
        }
        if (shape.preferDeterministic)
        {
            dispatch.issues.push_back(
                {"cuda.matmul.deterministic_policy",
                 "Deterministic preference disables the custom kernel path for this matmul shape.",
                 false});
        }
        if (shape.useBf16 && plan.device.computeCapabilityMajor < 8U)
        {
            dispatch.issues.push_back(
                {"cuda.matmul.bf16_degraded",
                 "BF16 requested on a pre-Ampere style device; prefer library fallback.", true});
        }

        return dispatch;
    }

} // namespace us4::runtime::backends::cuda
