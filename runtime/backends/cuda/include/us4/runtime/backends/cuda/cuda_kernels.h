#pragma once

#include "runtime/core/tensor.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace us4::runtime::backends::cuda
{

    enum class CudaKernelPrecision
    {
        kFp16,
        kBf16,
    };

    struct CudaKernelValidation
    {
        bool ok = false;
        std::string message;
    };

    struct CudaRmsNormOptions
    {
        float epsilon = 1.0e-5F;
        CudaKernelPrecision precision = CudaKernelPrecision::kFp16;
    };

    struct CudaBitNetPackedRow
    {
        std::vector<std::uint8_t> positiveBits;
        std::vector<std::uint8_t> negativeBits;
        std::size_t elementCount = 0U;
    };

    [[nodiscard]] CudaKernelValidation ValidateCudaMatMul(const us4::core::Tensor& left,
                                                          const us4::core::Tensor& right);
    [[nodiscard]] us4::core::Tensor CudaMatMul(const us4::core::Tensor& left,
                                               const us4::core::Tensor& right,
                                               CudaKernelPrecision precision);

    [[nodiscard]] CudaKernelValidation ValidateCudaSoftmax(const us4::core::Tensor& input);
    [[nodiscard]] us4::core::Tensor CudaSoftmax(const us4::core::Tensor& input,
                                                CudaKernelPrecision precision);

    [[nodiscard]] CudaKernelValidation ValidateCudaRmsNorm(const us4::core::Tensor& input,
                                                           const us4::core::Tensor& gamma);
    [[nodiscard]] us4::core::Tensor CudaRmsNorm(const us4::core::Tensor& input,
                                                const us4::core::Tensor& gamma,
                                                const CudaRmsNormOptions& options);
    [[nodiscard]] CudaBitNetPackedRow PackCudaBitNetRow(
        const std::vector<float>& values, float ternaryThreshold = 0.25F);
    [[nodiscard]] float CudaBitNetMatMul(const std::vector<float>& activations,
                                         const CudaBitNetPackedRow& packed);

} // namespace us4::runtime::backends::cuda
