#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/avx2_matmul.h"
#include "us4/runtime/backends/cpu_avx/avx512_matmul.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"
#include "us4/runtime/backends/cpu_avx/dequant_int4.h"
#include "us4/runtime/backends/cpu_avx/dequant_int8.h"
#include "us4/runtime/backends/cpu_avx/kernel_profile.h"
#include "us4/runtime/backends/onednn/onednn_backend.h"

#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace
{

    using Clock = std::chrono::steady_clock;

    struct BenchmarkSample
    {
        std::string modelId;
        std::string variant;
        std::string kernelTag;
        double elapsedMs = 0.0;
        double checksum = 0.0;
        double speedupVsScalar = 1.0;
        double tokensPerSecond = 0.0;
    };

    us4::core::Tensor MakeTensor(std::size_t rows, std::size_t cols, float seed)
    {
        us4::core::Tensor tensor({rows, cols});
        for (std::size_t index = 0; index < tensor.ElementCount(); ++index)
        {
            tensor[index] = seed + static_cast<float>((index % 23U) - 11U) * 0.03125F;
        }
        return tensor;
    }

    double Checksum(const us4::core::Tensor& tensor)
    {
        return std::accumulate(tensor.Data(), tensor.Data() + tensor.ElementCount(), 0.0);
    }

    us4::core::Tensor RunNaiveScalar(const us4::core::Tensor& left, const us4::core::Tensor& right)
    {
        us4::core::Tensor output({left.Dim(0), right.Dim(1)});
        output.Fill(0.0F);

        for (std::size_t row = 0; row < left.Dim(0); ++row)
        {
            for (std::size_t col = 0; col < right.Dim(1); ++col)
            {
                float acc = 0.0F;
                for (std::size_t depth = 0; depth < left.Dim(1); ++depth)
                {
                    acc += left.At({row, depth}) * right.At({depth, col});
                }
                output.At({row, col}) = acc;
            }
        }

        return output;
    }

    BenchmarkSample MeasureScalar(const std::string& modelId, const us4::core::Tensor& left,
                                  const us4::core::Tensor& right)
    {
        const auto startedAt = Clock::now();
        const auto output = RunNaiveScalar(left, right);
        const auto finishedAt = Clock::now();

        return {
            .modelId = modelId,
            .variant = "scalar",
            .kernelTag = "naive-scalar",
            .elapsedMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            .checksum = Checksum(output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = 0.0,
        };
    }

    BenchmarkSample MeasureBlocked(const std::string& modelId, const us4::core::Tensor& left,
                                   const us4::core::Tensor& right,
                                   const us4::runtime::backends::cpu_avx::CpuKernelProfile& profile,
                                   const std::string& variant)
    {
        const auto plan = us4::runtime::backends::cpu_avx::MakeReferenceMatMulPlan(
            left.Dim(0), left.Dim(1), right.Dim(1), &profile);
        const auto startedAt = Clock::now();
        const auto output = us4::runtime::backends::cpu_avx::BlockedMatMul(left, right, plan);
        const auto finishedAt = Clock::now();

        return {
            .modelId = modelId,
            .variant = variant,
            .kernelTag = plan.kernelTag,
            .elapsedMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            .checksum = Checksum(output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = 0.0,
        };
    }

    BenchmarkSample MeasureDirectKernel(
        const std::string& modelId, const us4::core::Tensor& left, const us4::core::Tensor& right,
        const us4::runtime::backends::cpu_avx::CpuKernelProfile& profile, const std::string& variant,
        us4::core::Tensor (*executor)(const us4::core::Tensor&, const us4::core::Tensor&,
                                      const us4::runtime::backends::cpu_avx::BlockedMatMulPlan&))
    {
        const auto plan = us4::runtime::backends::cpu_avx::MakeReferenceMatMulPlan(
            left.Dim(0), left.Dim(1), right.Dim(1), &profile);
        const auto startedAt = Clock::now();
        const auto output = executor(left, right, plan);
        const auto finishedAt = Clock::now();

        return {
            .modelId = modelId,
            .variant = variant,
            .kernelTag = plan.kernelTag,
            .elapsedMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            .checksum = Checksum(output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = 0.0,
        };
    }

    BenchmarkSample MeasureOneDnn(const std::string& modelId, const us4::core::Tensor& left,
                                  const us4::core::Tensor& right,
                                  const us4::runtime::backends::cpu_avx::CpuKernelProfile& profile)
    {
        us4::runtime::backends::onednn::OneDnnBackend backend;
        const auto startedAt = Clock::now();
        const auto output = backend.ExecuteMatMul(left, right, &profile);
        const auto finishedAt = Clock::now();

        return {
            .modelId = modelId,
            .variant = "onednn",
            .kernelTag = output.blockedPlan.kernelTag,
            .elapsedMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            .checksum = Checksum(output.output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = 0.0,
        };
    }

    std::vector<std::int8_t> QuantizeInt8GroupWise(const us4::core::Tensor& source,
                                                   const std::size_t groupSize,
                                                   std::vector<float>& scales)
    {
        const std::size_t elementCount = source.ElementCount();
        const std::size_t groupCount = (elementCount + groupSize - 1U) / groupSize;
        scales.assign(groupCount, 1.0F);
        std::vector<std::int8_t> quantized(elementCount, 0);

        for (std::size_t groupIndex = 0U; groupIndex < groupCount; ++groupIndex)
        {
            const std::size_t groupBegin = groupIndex * groupSize;
            const std::size_t groupEnd = std::min(groupBegin + groupSize, elementCount);
            float maxAbs = 0.0F;
            for (std::size_t index = groupBegin; index < groupEnd; ++index)
            {
                maxAbs = std::max(maxAbs, std::fabs(source[index]));
            }

            const float scale = maxAbs > 0.0F ? (maxAbs / 127.0F) : 1.0F;
            scales[groupIndex] = scale;
            for (std::size_t index = groupBegin; index < groupEnd; ++index)
            {
                const float scaled = source[index] / scale;
                const float rounded = std::round(std::clamp(scaled, -127.0F, 127.0F));
                quantized[index] = static_cast<std::int8_t>(rounded);
            }
        }

        return quantized;
    }

    std::vector<std::uint8_t> QuantizeInt4GroupWise(const us4::core::Tensor& source,
                                                    const std::size_t groupSize,
                                                    std::vector<float>& scales)
    {
        const std::size_t elementCount = source.ElementCount();
        const std::size_t groupCount = (elementCount + groupSize - 1U) / groupSize;
        scales.assign(groupCount, 1.0F);
        std::vector<std::uint8_t> quantized((elementCount + 1U) / 2U, 0U);

        auto encodeNibble = [](const std::int8_t value) -> std::uint8_t
        {
            return static_cast<std::uint8_t>(value < 0 ? value + 16 : value);
        };

        for (std::size_t groupIndex = 0U; groupIndex < groupCount; ++groupIndex)
        {
            const std::size_t groupBegin = groupIndex * groupSize;
            const std::size_t groupEnd = std::min(groupBegin + groupSize, elementCount);
            float maxAbs = 0.0F;
            for (std::size_t index = groupBegin; index < groupEnd; ++index)
            {
                maxAbs = std::max(maxAbs, std::fabs(source[index]));
            }

            const float scale = maxAbs > 0.0F ? (maxAbs / 7.0F) : 1.0F;
            scales[groupIndex] = scale;
            for (std::size_t index = groupBegin; index < groupEnd; ++index)
            {
                const float scaled = source[index] / scale;
                const float rounded = std::round(std::clamp(scaled, -8.0F, 7.0F));
                const std::uint8_t nibble = encodeNibble(static_cast<std::int8_t>(rounded));
                const std::size_t packedIndex = index / 2U;
                if ((index % 2U) == 0U)
                {
                    quantized[packedIndex] =
                        static_cast<std::uint8_t>((quantized[packedIndex] & 0xF0U) | nibble);
                }
                else
                {
                    quantized[packedIndex] = static_cast<std::uint8_t>(
                        (quantized[packedIndex] & 0x0FU) | static_cast<std::uint8_t>(nibble << 4));
                }
            }
        }

        return quantized;
    }

    BenchmarkSample MeasureQuantizedPath(
        const std::string& modelId, const std::string& variant, const us4::core::Tensor& left,
        const us4::core::Tensor& right,
        const us4::runtime::backends::cpu_avx::CpuKernelProfile& profile,
        const std::size_t groupSize, const bool useInt4)
    {
        const auto plan = us4::runtime::backends::cpu_avx::MakeReferenceMatMulPlan(
            left.Dim(0), left.Dim(1), right.Dim(1), &profile);

        std::vector<float> scales;
        const auto quantizedInt8 =
            useInt4 ? std::vector<std::int8_t>() : QuantizeInt8GroupWise(right, groupSize, scales);
        const auto quantizedInt4 = useInt4 ? QuantizeInt4GroupWise(right, groupSize, scales)
                                           : std::vector<std::uint8_t>();
        const auto startedAt = Clock::now();
        const auto dequantizedRight = useInt4
                                          ? us4::runtime::backends::cpu_avx::DequantizeInt4GroupWise(
                                                quantizedInt4,
                                                right.Dim(0), right.Dim(1), groupSize, scales)
                                          : us4::runtime::backends::cpu_avx::DequantizeInt8GroupWise(
                                                quantizedInt8,
                                                right.Dim(0), right.Dim(1), groupSize, scales);
        const auto output = us4::runtime::backends::cpu_avx::BlockedMatMul(left, dequantizedRight, plan);
        const auto finishedAt = Clock::now();

        const double elapsedMs =
            std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
        const double tokensPerSecond =
            elapsedMs > 0.0 ? (static_cast<double>(left.Dim(0)) * 1000.0 / elapsedMs) : 0.0;

        return {
            .modelId = modelId,
            .variant = variant,
            .kernelTag = useInt4 ? "avx2-groupwise-int4-dequant" : "avx2-groupwise-int8-dequant",
            .elapsedMs = elapsedMs,
            .checksum = Checksum(output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = tokensPerSecond,
        };
    }

    void PersistReport(const std::vector<BenchmarkSample>& samples)
    {
        const std::filesystem::path outputPath = std::filesystem::path("runtime") / "benchmarks" /
                                                 "correctness" / "reports" /
                                                 "cpu_block_gemm_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n  \"suite\": \"cpu-block-gemm-bench\",\n  \"samples\": [\n";
        for (std::size_t index = 0; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\"model\":\"" << sample.modelId << "\",\"variant\":\"" << sample.variant
                 << "\",\"kernel_tag\":\"" << sample.kernelTag << "\",\"elapsed_ms\":" << std::fixed
                 << std::setprecision(6) << sample.elapsedMs << ",\"checksum\":" << sample.checksum
                 << ",\"speedup_vs_scalar\":" << sample.speedupVsScalar
                 << ",\"tokens_per_second\":" << sample.tokensPerSecond << "}";
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
    using us4::runtime::backends::cpu_avx::BuildKernelProfile;

    const auto scalarProfile = BuildKernelProfile({
        .hardwareThreadCount = 1U,
    });
    const auto avx512Profile = BuildKernelProfile({
        .avx2 = true,
        .avx512f = true,
        .avx512Vnni = true,
        .f16c = true,
        .l2BytesPerCore = 2048U * 1024U,
        .hardwareThreadCount = 8U,
    });
    const auto avx2Profile = BuildKernelProfile({
        .avx2 = true,
        .f16c = true,
        .l2BytesPerCore = 2048U * 1024U,
        .hardwareThreadCount = 8U,
    });

    struct ModelShape
    {
        std::string modelId;
        std::size_t rows;
        std::size_t inner;
        std::size_t cols;
    };

    const std::vector<ModelShape> shapes = {
        {"qwen-0.5b", 192U, 384U, 384U},
        {"gemma-2b", 256U, 384U, 512U},
    };

    std::vector<BenchmarkSample> samples;
    for (const auto& shape : shapes)
    {
        const auto left = MakeTensor(shape.rows, shape.inner, 0.25F);
        const auto right = MakeTensor(shape.inner, shape.cols, -0.5F);

        auto scalar = MeasureScalar(shape.modelId, left, right);
        const bool hostSupportsAvx2 = us4::runtime::backends::cpu_avx::HostSupportsAvx2MatMul();
        const bool hostSupportsAvx512 =
            us4::runtime::backends::cpu_avx::HostSupportsAvx512MatMul();

        auto avx2 = hostSupportsAvx2
                        ? MeasureDirectKernel(shape.modelId, left, right, avx2Profile, "cpu-avx2",
                                              us4::runtime::backends::cpu_avx::Avx2BlockedMatMul)
                        : BenchmarkSample{.modelId = shape.modelId,
                                          .variant = "cpu-avx2-unavailable",
                                          .kernelTag = "host-no-avx2"};
        auto avx512Direct =
            hostSupportsAvx512
                ? MeasureDirectKernel(shape.modelId, left, right, avx512Profile,
                                      "cpu-avx512-direct",
                                      us4::runtime::backends::cpu_avx::Avx512BlockedMatMul)
                : BenchmarkSample{.modelId = shape.modelId,
                                  .variant = "cpu-avx512-direct-unavailable",
                                  .kernelTag = "host-no-avx512"};
        auto avx512 = hostSupportsAvx512
                          ? MeasureBlocked(shape.modelId, left, right, avx512Profile, "cpu-avx512")
                          : BenchmarkSample{.modelId = shape.modelId,
                                            .variant = "cpu-avx512-blocked-unavailable",
                                            .kernelTag = "host-no-avx512"};
        auto oneDnn = MeasureOneDnn(shape.modelId, left, right,
                                    hostSupportsAvx512 ? avx512Profile : avx2Profile);
        auto q8Dequant = hostSupportsAvx2
                             ? MeasureQuantizedPath(shape.modelId, "cpu-q8-avx2", left, right,
                                                    avx2Profile, 32U, false)
                             : BenchmarkSample{.modelId = shape.modelId,
                                               .variant = "cpu-q8-avx2-unavailable",
                                               .kernelTag = "host-no-avx2"};
        auto q4Dequant = hostSupportsAvx2
                             ? MeasureQuantizedPath(shape.modelId, "cpu-q4-avx2", left, right,
                                                    avx2Profile, 32U, true)
                             : BenchmarkSample{.modelId = shape.modelId,
                                               .variant = "cpu-q4-avx2-unavailable",
                                               .kernelTag = "host-no-avx2"};

        if (hostSupportsAvx2)
        {
            avx2.speedupVsScalar = scalar.elapsedMs / avx2.elapsedMs;
        }
        if (hostSupportsAvx512)
        {
            avx512Direct.speedupVsScalar = scalar.elapsedMs / avx512Direct.elapsedMs;
        }
        if (hostSupportsAvx512)
        {
            avx512.speedupVsScalar = scalar.elapsedMs / avx512.elapsedMs;
        }
        oneDnn.speedupVsScalar = scalar.elapsedMs / oneDnn.elapsedMs;
        if (hostSupportsAvx2)
        {
            q8Dequant.speedupVsScalar = scalar.elapsedMs / q8Dequant.elapsedMs;
            q4Dequant.speedupVsScalar = scalar.elapsedMs / q4Dequant.elapsedMs;
        }

        std::cout << shape.modelId << " scalar elapsed_ms=" << std::fixed << std::setprecision(3)
                  << scalar.elapsedMs << " checksum=" << scalar.checksum << '\n';
        std::cout << shape.modelId << " " << avx2.variant << " elapsed_ms=" << avx2.elapsedMs
                  << " speedup_vs_scalar=" << avx2.speedupVsScalar
                  << " kernel_tag=" << avx2.kernelTag << " checksum=" << avx2.checksum << '\n';
        std::cout << shape.modelId << " " << avx512Direct.variant
                  << " elapsed_ms=" << avx512Direct.elapsedMs
                  << " speedup_vs_scalar=" << avx512Direct.speedupVsScalar
                  << " kernel_tag=" << avx512Direct.kernelTag
                  << " checksum=" << avx512Direct.checksum << '\n';
        std::cout << shape.modelId << " " << avx512.variant << " elapsed_ms=" << avx512.elapsedMs
                  << " speedup_vs_scalar=" << avx512.speedupVsScalar
                  << " kernel_tag=" << avx512.kernelTag << " checksum=" << avx512.checksum << '\n';
        std::cout << shape.modelId << " " << oneDnn.variant << " elapsed_ms=" << oneDnn.elapsedMs
                  << " speedup_vs_scalar=" << oneDnn.speedupVsScalar
                  << " kernel_tag=" << oneDnn.kernelTag << " checksum=" << oneDnn.checksum << '\n';
        std::cout << shape.modelId << " " << q8Dequant.variant << " elapsed_ms=" << q8Dequant.elapsedMs
                  << " speedup_vs_scalar=" << q8Dequant.speedupVsScalar
                  << " tokens_per_second=" << q8Dequant.tokensPerSecond
                  << " kernel_tag=" << q8Dequant.kernelTag << " checksum=" << q8Dequant.checksum << '\n';
        std::cout << shape.modelId << " " << q4Dequant.variant << " elapsed_ms=" << q4Dequant.elapsedMs
                  << " speedup_vs_scalar=" << q4Dequant.speedupVsScalar
                  << " tokens_per_second=" << q4Dequant.tokensPerSecond
                  << " kernel_tag=" << q4Dequant.kernelTag << " checksum=" << q4Dequant.checksum << '\n';

        samples.push_back(std::move(scalar));
        samples.push_back(std::move(avx2));
        samples.push_back(std::move(avx512Direct));
        samples.push_back(std::move(avx512));
        samples.push_back(std::move(oneDnn));
        samples.push_back(std::move(q8Dequant));
        samples.push_back(std::move(q4Dequant));
    }

    PersistReport(samples);
    return 0;
}
