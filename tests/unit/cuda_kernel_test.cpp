#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/scalar_attention.h"
#include "us4/runtime/backends/cuda/cuda_kernels.h"

#include <cmath>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        us4::core::Tensor MakeTensor(const std::size_t rows, const std::size_t cols,
                                     const float seed)
        {
            us4::core::Tensor tensor({rows, cols});
            for (std::size_t row = 0U; row < rows; ++row)
            {
                for (std::size_t col = 0U; col < cols; ++col)
                {
                    tensor.At({row, col}) =
                        seed + static_cast<float>((row * 17U + col * 7U) % 23U) * 0.0625F - 0.5F;
                }
            }
            return tensor;
        }

        float MaxAbsDiff(const us4::core::Tensor& left, const us4::core::Tensor& right)
        {
            float maxDiff = 0.0F;
            for (std::size_t index = 0U; index < left.ElementCount(); ++index)
            {
                maxDiff = std::max(maxDiff, std::fabs(left[index] - right[index]));
            }
            return maxDiff;
        }

        us4::core::Tensor ScalarMatMul(const us4::core::Tensor& left,
                                       const us4::core::Tensor& right)
        {
            us4::core::Tensor output({left.Dim(0), right.Dim(1)});
            output.Fill(0.0F);

            for (std::size_t row = 0U; row < left.Dim(0); ++row)
            {
                for (std::size_t col = 0U; col < right.Dim(1); ++col)
                {
                    float acc = 0.0F;
                    for (std::size_t depth = 0U; depth < left.Dim(1); ++depth)
                    {
                        acc += left.At({row, depth}) * right.At({depth, col});
                    }
                    output.At({row, col}) = acc;
                }
            }

            return output;
        }

        us4::core::Tensor ScalarSoftmax(const us4::core::Tensor& input)
        {
            us4::core::Tensor output({input.Dim(0), input.Dim(1)});
            output.Fill(0.0F);

            for (std::size_t row = 0U; row < input.Dim(0); ++row)
            {
                float maxValue = input.At({row, 0U});
                for (std::size_t col = 1U; col < input.Dim(1); ++col)
                {
                    maxValue = std::max(maxValue, input.At({row, col}));
                }

                float normalizer = 0.0F;
                for (std::size_t col = 0U; col < input.Dim(1); ++col)
                {
                    const float exponent = std::exp(input.At({row, col}) - maxValue);
                    output.At({row, col}) = exponent;
                    normalizer += exponent;
                }

                for (std::size_t col = 0U; col < input.Dim(1); ++col)
                {
                    output.At({row, col}) /= normalizer;
                }
            }

            return output;
        }

        us4::core::Tensor ScalarRmsNorm(const us4::core::Tensor& input,
                                        const us4::core::Tensor& gamma, const float epsilon)
        {
            us4::core::Tensor output({input.Dim(0), input.Dim(1)});
            output.Fill(0.0F);

            for (std::size_t row = 0U; row < input.Dim(0); ++row)
            {
                float meanSquare = 0.0F;
                for (std::size_t col = 0U; col < input.Dim(1); ++col)
                {
                    const float value = input.At({row, col});
                    meanSquare += value * value;
                }
                meanSquare /= static_cast<float>(input.Dim(1));
                const float inverseRms = 1.0F / std::sqrt(meanSquare + epsilon);

                for (std::size_t col = 0U; col < input.Dim(1); ++col)
                {
                    output.At({row, col}) = input.At({row, col}) * inverseRms * gamma.At({0U, col});
                }
            }

            return output;
        }

        TEST(CudaKernelTest, MatMulMatchesScalarReferenceWithinFp16Tolerance)
        {
            const auto left = MakeTensor(16U, 32U, 0.25F);
            const auto right = MakeTensor(32U, 12U, -0.5F);

            const auto scalar = ScalarMatMul(left, right);
            const auto cuda = us4::runtime::backends::cuda::CudaMatMul(
                left, right, us4::runtime::backends::cuda::CudaKernelPrecision::kFp16);

            EXPECT_LE(MaxAbsDiff(scalar, cuda), 1.0e-2F);
        }

        TEST(CudaKernelTest, SoftmaxStaysNumericallyStableAndRowsSumToOne)
        {
            auto input = MakeTensor(8U, 16U, 0.0F);
            input.At({0U, 0U}) = 40.0F;
            input.At({0U, 1U}) = 39.5F;
            input.At({0U, 2U}) = -40.0F;

            const auto cuda = us4::runtime::backends::cuda::CudaSoftmax(
                input, us4::runtime::backends::cuda::CudaKernelPrecision::kBf16);
            const auto scalar = ScalarSoftmax(input);

            EXPECT_LE(MaxAbsDiff(scalar, cuda), 5.0e-3F);
            for (std::size_t row = 0U; row < cuda.Dim(0); ++row)
            {
                float sum = 0.0F;
                for (std::size_t col = 0U; col < cuda.Dim(1); ++col)
                {
                    sum += cuda.At({row, col});
                    EXPECT_TRUE(std::isfinite(cuda.At({row, col})));
                }
                EXPECT_NEAR(sum, 1.0F, 5.0e-3F);
            }
        }

        TEST(CudaKernelTest, RmsNormMatchesScalarReferenceWithinFp16Tolerance)
        {
            const auto input = MakeTensor(12U, 24U, 0.125F);
            auto gamma = MakeTensor(1U, 24U, 1.0F);
            for (std::size_t col = 0U; col < gamma.Dim(1); ++col)
            {
                gamma.At({0U, col}) = 0.9F + static_cast<float>(col % 5U) * 0.05F;
            }

            const auto scalar = ScalarRmsNorm(input, gamma, 1.0e-5F);
            const auto cuda = us4::runtime::backends::cuda::CudaRmsNorm(
                input, gamma,
                us4::runtime::backends::cuda::CudaRmsNormOptions{
                    .epsilon = 1.0e-5F,
                    .precision = us4::runtime::backends::cuda::CudaKernelPrecision::kFp16,
                });

            EXPECT_LE(MaxAbsDiff(scalar, cuda), 5.0e-3F);
        }

        TEST(CudaKernelTest, RmsNormBf16PathStaysFiniteAndCloseToReference)
        {
            const auto input = MakeTensor(12U, 24U, 0.125F);
            auto gamma = MakeTensor(1U, 24U, 1.0F);
            for (std::size_t col = 0U; col < gamma.Dim(1); ++col)
            {
                gamma.At({0U, col}) = 0.9F + static_cast<float>(col % 5U) * 0.05F;
            }

            const auto scalar = ScalarRmsNorm(input, gamma, 1.0e-5F);
            const auto cuda = us4::runtime::backends::cuda::CudaRmsNorm(
                input, gamma,
                us4::runtime::backends::cuda::CudaRmsNormOptions{
                    .epsilon = 1.0e-5F,
                    .precision = us4::runtime::backends::cuda::CudaKernelPrecision::kBf16,
                });

            EXPECT_LE(MaxAbsDiff(scalar, cuda), 1.0e-2F);
            for (std::size_t index = 0U; index < cuda.ElementCount(); ++index)
            {
                EXPECT_TRUE(std::isfinite(cuda[index]));
            }
        }
    } // namespace
} // namespace us4::runtime::tests
