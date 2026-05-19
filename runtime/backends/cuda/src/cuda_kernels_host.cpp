#include "us4/runtime/backends/cuda/cuda_kernels.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace us4::runtime::backends::cuda
{

    namespace
    {
        [[nodiscard]] float RoundToBf16(const float value)
        {
            std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            const std::uint32_t roundingBias = 0x00007FFFU + ((bits >> 16U) & 1U);
            bits += roundingBias;
            bits &= 0xFFFF0000U;
            return std::bit_cast<float>(bits);
        }

        [[nodiscard]] float RoundToFp16(const float value)
        {
            const float sign = std::signbit(value) ? -1.0F : 1.0F;
            const float magnitude = std::fabs(value);
            if (magnitude == 0.0F || !std::isfinite(magnitude))
            {
                return value;
            }

            const int exponent = static_cast<int>(std::floor(std::log2(magnitude)));
            const float step = std::ldexp(1.0F, exponent - 10);
            return sign * std::round(magnitude / step) * step;
        }

        [[nodiscard]] float Quantize(const float value, const CudaKernelPrecision precision)
        {
            return precision == CudaKernelPrecision::kBf16 ? RoundToBf16(value)
                                                           : RoundToFp16(value);
        }

        [[nodiscard]] bool IsRankTwo(const us4::core::Tensor& tensor)
        {
            return tensor.Rank() == 2U;
        }

        void SetBit(std::vector<std::uint8_t>& bytes, const std::size_t index)
        {
            bytes[index / 8U] |= static_cast<std::uint8_t>(1U << (index % 8U));
        }
    } // namespace

} // namespace us4::runtime::backends::cuda

#include "../kernels/bitnet_matmul.cu"
#include "../kernels/matmul.cu"
#include "../kernels/rmsnorm.cu"
#include "../kernels/softmax.cu"

namespace us4::runtime::backends::cuda
{

    CudaKernelValidation ValidateCudaMatMul(const us4::core::Tensor& left,
                                            const us4::core::Tensor& right)
    {
        if (!IsRankTwo(left) || !IsRankTwo(right) || left.Dim(1) != right.Dim(0))
        {
            return {false, "CudaMatMul expects rank-2 tensors shaped [M, K] x [K, N]."};
        }
        return {true, {}};
    }

    us4::core::Tensor CudaMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right,
                                 const CudaKernelPrecision precision)
    {
        const auto validation = ValidateCudaMatMul(left, right);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        return kernels::MatMulHostKernel(left, right, [&](const float value)
                                         { return Quantize(value, precision); });
    }

    CudaKernelValidation ValidateCudaSoftmax(const us4::core::Tensor& input)
    {
        if (!IsRankTwo(input))
        {
            return {false, "CudaSoftmax expects a rank-2 [M, N] tensor."};
        }
        return {true, {}};
    }

    us4::core::Tensor CudaSoftmax(const us4::core::Tensor& input,
                                  const CudaKernelPrecision precision)
    {
        const auto validation = ValidateCudaSoftmax(input);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        return kernels::SoftmaxHostKernel(input, [&](const float value)
                                          { return Quantize(value, precision); });
    }

    CudaKernelValidation ValidateCudaRmsNorm(const us4::core::Tensor& input,
                                             const us4::core::Tensor& gamma)
    {
        if (!IsRankTwo(input) || !IsRankTwo(gamma) || gamma.Dim(0) != 1U ||
            gamma.Dim(1) != input.Dim(1))
        {
            return {false, "CudaRmsNorm expects input [M, N] and gamma [1, N]."};
        }
        return {true, {}};
    }

    us4::core::Tensor CudaRmsNorm(const us4::core::Tensor& input, const us4::core::Tensor& gamma,
                                  const CudaRmsNormOptions& options)
    {
        const auto validation = ValidateCudaRmsNorm(input, gamma);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        return kernels::RmsNormHostKernel(input, gamma, options.epsilon, [&](const float value)
                                          { return Quantize(value, options.precision); });
    }

    CudaBitNetPackedRow PackCudaBitNetRow(const std::vector<float>& values,
                                          const float ternaryThreshold)
    {
        CudaBitNetPackedRow packed;
        packed.elementCount = values.size();
        const std::size_t byteCount = (values.size() + 7U) / 8U;
        packed.positiveBits.assign(byteCount, 0U);
        packed.negativeBits.assign(byteCount, 0U);

        for (std::size_t index = 0U; index < values.size(); ++index)
        {
            if (values[index] >= ternaryThreshold)
            {
                SetBit(packed.positiveBits, index);
            }
            else if (values[index] <= -ternaryThreshold)
            {
                SetBit(packed.negativeBits, index);
            }
        }

        return packed;
    }

    float CudaBitNetMatMul(const std::vector<float>& activations, const CudaBitNetPackedRow& packed)
    {
        if (activations.size() != packed.elementCount)
        {
            throw std::invalid_argument("CudaBitNetMatMul activation length mismatch.");
        }
        if (packed.positiveBits.size() != packed.negativeBits.size())
        {
            throw std::invalid_argument(
                "CudaBitNetMatMul packed bit lanes must have matching sizes.");
        }

        return kernels::BitNetMatMulHostKernel(activations, packed);
    }

} // namespace us4::runtime::backends::cuda
