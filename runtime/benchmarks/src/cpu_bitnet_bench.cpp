#include "us4/runtime/backends/cpu_avx/bitnet_matmul.h"
#include "us4/runtime/backends/cpu_avx/bitnet_reference.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"

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
    constexpr std::size_t kBenchmarkWidth = 4096U;
    constexpr std::size_t kIterations = 4096U;

    struct BitNetSample
    {
        std::string variant;
        std::size_t width = 0U;
        std::size_t iterations = 0U;
        double elapsedMs = 0.0;
        double checksum = 0.0;
        double speedupVsScalar = 1.0;
        double tokensPerSecond = 0.0;
    };

    std::vector<float> MakeActivations(const std::size_t width)
    {
        std::vector<float> values(width, 0.0F);
        for (std::size_t index = 0U; index < width; ++index)
        {
            values[index] = static_cast<float>((index % 19U) - 9U) * 0.125F;
        }
        return values;
    }

    std::vector<float> MakeWeights(const std::size_t width)
    {
        std::vector<float> values(width, 0.0F);
        for (std::size_t index = 0U; index < width; ++index)
        {
            values[index] = static_cast<float>((index % 7U) - 3U) * 0.35F;
        }
        return values;
    }

    void PersistReport(const std::vector<BitNetSample>& samples)
    {
        const std::filesystem::path outputPath = std::filesystem::path("runtime") / "benchmarks" /
                                                 "correctness" / "reports" /
                                                 "cpu_bitnet_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n  \"suite\": \"cpu-bitnet-bench\",\n  \"samples\": [\n";
        for (std::size_t index = 0U; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\"variant\":\"" << sample.variant << "\",\"width\":" << sample.width
                 << ",\"iterations\":" << sample.iterations << ",\"elapsed_ms\":" << std::fixed
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

    template <typename Executor>
    BitNetSample MeasureVariant(const std::string& variant, const std::size_t width,
                                const std::size_t iterations, const Executor& executor)
    {
        double checksum = 0.0;
        volatile float sink = 0.0F;

        const auto startedAt = Clock::now();
        for (std::size_t iteration = 0U; iteration < iterations; ++iteration)
        {
            const float value = executor();
            checksum += static_cast<double>(value);
            sink = value;
        }
        const auto finishedAt = Clock::now();

        const double elapsedMs =
            std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
        const double tokensPerSecond =
            elapsedMs > 0.0
                ? ((static_cast<double>(width) * static_cast<double>(iterations) * 1000.0) /
                   elapsedMs)
                : 0.0;

        return {
            .variant = variant,
            .width = width,
            .iterations = iterations,
            .elapsedMs = elapsedMs,
            .checksum = checksum + static_cast<double>(sink * 0.0F),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = tokensPerSecond,
        };
    }
} // namespace

int main()
{
    const std::size_t width = kBenchmarkWidth;
    const auto activations = MakeActivations(width);
    const auto packed = us4::runtime::backends::cpu_avx::PackBitNetRow(MakeWeights(width));

    auto scalar = MeasureVariant(
        "scalar-bitnet", width, kIterations,
        [&]() { return us4::runtime::backends::cpu_avx::DotPackedBitNet(activations, packed); });
    std::vector<BitNetSample> samples;
    samples.push_back(scalar);

    std::cout << "scalar-bitnet elapsed_ms=" << std::fixed << std::setprecision(6)
              << scalar.elapsedMs << " tokens_per_second=" << scalar.tokensPerSecond
              << " checksum=" << scalar.checksum << '\n';

    if (us4::runtime::backends::cpu_avx::HostSupportsAvx2MatMul())
    {
        auto avx = MeasureVariant(
            "avx2-bitnet", width, kIterations, [&]()
            { return us4::runtime::backends::cpu_avx::DotPackedBitNetAvx2(activations, packed); });
        avx.speedupVsScalar = avx.elapsedMs > 0.0 ? (scalar.elapsedMs / avx.elapsedMs) : 1.0;
        samples.push_back(avx);
        std::cout << "avx2-bitnet elapsed_ms=" << avx.elapsedMs
                  << " speedup_vs_scalar=" << avx.speedupVsScalar
                  << " tokens_per_second=" << avx.tokensPerSecond << " checksum=" << avx.checksum
                  << '\n';
    }
    else
    {
        samples.push_back({
            .variant = "avx2-bitnet-unavailable",
            .width = width,
            .iterations = kIterations,
            .elapsedMs = 0.0,
            .checksum = 0.0,
            .speedupVsScalar = 1.0,
            .tokensPerSecond = 0.0,
        });
        std::cout << "avx2-bitnet-unavailable elapsed_ms=0.000000 speedup_vs_scalar=1.000000 "
                     "tokens_per_second=0.000000 checksum=0.000000\n";
    }

    PersistReport(samples);
    return 0;
}
