#include "us4/runtime/backends/cuda/cuda_kernels.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct KernelSample
    {
        std::string kernel;
        std::string precision;
        float maxAbsDiff = 0.0F;
    };

    us4::core::Tensor MakeTensor(const std::size_t rows, const std::size_t cols, const float seed)
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

    us4::core::Tensor ScalarMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right)
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

    us4::core::Tensor ScalarRmsNorm(const us4::core::Tensor& input, const us4::core::Tensor& gamma)
    {
        us4::core::Tensor output({input.Dim(0), input.Dim(1)});
        output.Fill(0.0F);
        constexpr float epsilon = 1.0e-5F;
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

    void PersistReport(const std::vector<KernelSample>& samples)
    {
        const std::filesystem::path outputPath = std::filesystem::path("runtime") / "benchmarks" /
                                                 "correctness" / "reports" /
                                                 "cuda_kernel_correctness_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n  \"suite\": \"cuda-kernel-correctness-bench\",\n  \"samples\": [\n";
        for (std::size_t index = 0U; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\"kernel\":\"" << sample.kernel << "\",\"precision\":\""
                 << sample.precision << "\",\"max_abs_diff\":" << std::fixed << std::setprecision(6)
                 << sample.maxAbsDiff << "}";
            if (index + 1U != samples.size())
            {
                json << ',';
            }
            json << "\n";
        }
        json << "  ]\n}\n";

        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        output << json.str();
    }
} // namespace

int main()
{
    using namespace us4::runtime::backends::cuda;

    const auto left = MakeTensor(16U, 32U, 0.25F);
    const auto right = MakeTensor(32U, 12U, -0.5F);
    auto softmaxInput = MakeTensor(8U, 16U, 0.0F);
    softmaxInput.At({0U, 0U}) = 40.0F;
    softmaxInput.At({0U, 1U}) = 39.5F;
    softmaxInput.At({0U, 2U}) = -40.0F;
    const auto rmsInput = MakeTensor(12U, 24U, 0.125F);
    auto gamma = MakeTensor(1U, 24U, 1.0F);
    for (std::size_t col = 0U; col < gamma.Dim(1); ++col)
    {
        gamma.At({0U, col}) = 0.9F + static_cast<float>(col % 5U) * 0.05F;
    }

    const auto matmulDiff =
        MaxAbsDiff(ScalarMatMul(left, right), CudaMatMul(left, right, CudaKernelPrecision::kFp16));
    const auto softmaxDiff = MaxAbsDiff(ScalarSoftmax(softmaxInput),
                                        CudaSoftmax(softmaxInput, CudaKernelPrecision::kBf16));
    const auto rmsnormFp16Diff = MaxAbsDiff(ScalarRmsNorm(rmsInput, gamma),
                                            CudaRmsNorm(rmsInput, gamma,
                                                        CudaRmsNormOptions{
                                                            .epsilon = 1.0e-5F,
                                                            .precision = CudaKernelPrecision::kFp16,
                                                        }));
    const auto rmsnormBf16Diff = MaxAbsDiff(ScalarRmsNorm(rmsInput, gamma),
                                            CudaRmsNorm(rmsInput, gamma,
                                                        CudaRmsNormOptions{
                                                            .epsilon = 1.0e-5F,
                                                            .precision = CudaKernelPrecision::kBf16,
                                                        }));

    const std::vector<KernelSample> samples = {
        {.kernel = "matmul", .precision = "fp16", .maxAbsDiff = matmulDiff},
        {.kernel = "softmax", .precision = "bf16", .maxAbsDiff = softmaxDiff},
        {.kernel = "rmsnorm", .precision = "fp16", .maxAbsDiff = rmsnormFp16Diff},
        {.kernel = "rmsnorm", .precision = "bf16", .maxAbsDiff = rmsnormBf16Diff},
    };

    PersistReport(samples);

    std::cout << "matmul fp16 max_abs_diff=" << std::fixed << std::setprecision(6) << matmulDiff
              << '\n';
    std::cout << "softmax bf16 max_abs_diff=" << softmaxDiff << '\n';
    std::cout << "rmsnorm fp16 max_abs_diff=" << rmsnormFp16Diff << '\n';
    std::cout << "rmsnorm bf16 max_abs_diff=" << rmsnormBf16Diff << '\n';
    return (matmulDiff <= 1.0e-2F && softmaxDiff <= 5.0e-3F && rmsnormFp16Diff <= 5.0e-3F &&
            rmsnormBf16Diff <= 1.0e-2F)
               ? 0
               : 1;
}
