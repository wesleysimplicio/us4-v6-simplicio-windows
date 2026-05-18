#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"
#include "us4/runtime/backends/cpu_avx/kernel_profile.h"
#include "us4/runtime/backends/onednn/onednn_backend.h"

#include <chrono>
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
                 << ",\"speedup_vs_scalar\":" << sample.speedupVsScalar << "}";
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
        auto avx512 = MeasureBlocked(shape.modelId, left, right, avx512Profile, "cpu-avx512");
        auto oneDnn = MeasureOneDnn(shape.modelId, left, right, avx512Profile);

        avx512.speedupVsScalar = scalar.elapsedMs / avx512.elapsedMs;
        oneDnn.speedupVsScalar = scalar.elapsedMs / oneDnn.elapsedMs;

        std::cout << shape.modelId << " scalar elapsed_ms=" << std::fixed << std::setprecision(3)
                  << scalar.elapsedMs << " checksum=" << scalar.checksum << '\n';
        std::cout << shape.modelId << " " << avx512.variant << " elapsed_ms=" << avx512.elapsedMs
                  << " speedup_vs_scalar=" << avx512.speedupVsScalar
                  << " kernel_tag=" << avx512.kernelTag << " checksum=" << avx512.checksum << '\n';
        std::cout << shape.modelId << " " << oneDnn.variant << " elapsed_ms=" << oneDnn.elapsedMs
                  << " speedup_vs_scalar=" << oneDnn.speedupVsScalar
                  << " kernel_tag=" << oneDnn.kernelTag << " checksum=" << oneDnn.checksum << '\n';

        samples.push_back(std::move(scalar));
        samples.push_back(std::move(avx512));
        samples.push_back(std::move(oneDnn));
    }

    PersistReport(samples);
    return 0;
}
