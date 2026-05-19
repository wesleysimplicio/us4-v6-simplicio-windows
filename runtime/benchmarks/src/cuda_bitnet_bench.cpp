#include "us4/runtime/backends/cpu_avx/bitnet_reference.h"
#include "us4/runtime/backends/cuda/cuda_kernels.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::vector<float> MakeWeights(const std::size_t width)
    {
        std::vector<float> weights(width, 0.0F);
        for (std::size_t index = 0U; index < width; ++index)
        {
            const std::size_t mod = index % 7U;
            weights[index] = mod < 2U ? -0.85F : (mod < 4U ? 0.0F : 0.9F);
        }
        return weights;
    }

    std::vector<float> MakeActivations(const std::size_t width)
    {
        std::vector<float> activations(width, 0.0F);
        for (std::size_t index = 0U; index < width; ++index)
        {
            activations[index] = static_cast<float>((index % 19U) - 9U) * 0.125F;
        }
        return activations;
    }

    template <typename Fn>
    double MeasureMilliseconds(const std::size_t iterations, Fn&& fn, float& checksum)
    {
        const auto start = std::chrono::steady_clock::now();
        checksum = 0.0F;
        for (std::size_t iteration = 0U; iteration < iterations; ++iteration)
        {
            checksum += fn();
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    void PersistReport(const std::size_t width, const std::size_t iterations,
                       const double scalarMs, const double cudaMs, const float scalarChecksum,
                       const float cudaChecksum)
    {
        const std::filesystem::path outputPath =
            std::filesystem::path("runtime") / "benchmarks" / "correctness" / "reports" /
            "cuda_bitnet_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n"
             << "  \"suite\": \"cuda-bitnet-bench\",\n"
             << "  \"width\": " << width << ",\n"
             << "  \"iterations\": " << iterations << ",\n"
             << "  \"scalar_elapsed_ms\": " << std::fixed << std::setprecision(6) << scalarMs
             << ",\n"
             << "  \"cuda_elapsed_ms\": " << cudaMs << ",\n"
             << "  \"speedup_vs_scalar\": " << (scalarMs / cudaMs) << ",\n"
             << "  \"tokens_per_second\": " << ((static_cast<double>(iterations) * width) /
                                                (cudaMs / 1000.0))
             << ",\n"
             << "  \"scalar_checksum\": " << scalarChecksum << ",\n"
             << "  \"cuda_checksum\": " << cudaChecksum << "\n"
             << "}\n";

        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        output << json.str();
    }
} // namespace

int main()
{
    constexpr std::size_t width = 4096U;
    constexpr std::size_t iterations = 4096U;

    const auto weights = MakeWeights(width);
    const auto activations = MakeActivations(width);
    const auto scalarPacked = us4::runtime::backends::cpu_avx::PackBitNetRow(weights);
    const auto cudaPacked = us4::runtime::backends::cuda::PackCudaBitNetRow(weights);

    float scalarChecksum = 0.0F;
    const double scalarMs = MeasureMilliseconds(
        iterations,
        [&]()
        { return us4::runtime::backends::cpu_avx::DotPackedBitNet(activations, scalarPacked); },
        scalarChecksum);

    float cudaChecksum = 0.0F;
    const double cudaMs = MeasureMilliseconds(
        iterations,
        [&]()
        { return us4::runtime::backends::cuda::CudaBitNetMatMul(activations, cudaPacked); },
        cudaChecksum);

    PersistReport(width, iterations, scalarMs, cudaMs, scalarChecksum, cudaChecksum);

    std::cout << "scalar-bitnet-cuda elapsed_ms=" << std::fixed << std::setprecision(6) << scalarMs
              << " checksum=" << scalarChecksum << '\n';
    std::cout << "cuda-bitnet elapsed_ms=" << cudaMs << " speedup_vs_scalar=" << (scalarMs / cudaMs)
              << " tokens_per_second=" << ((static_cast<double>(iterations) * width) /
                                             (cudaMs / 1000.0))
              << " checksum=" << cudaChecksum << '\n';

    return std::fabs(scalarChecksum - cudaChecksum) <= 5.0e-3F ? 0 : 1;
}
