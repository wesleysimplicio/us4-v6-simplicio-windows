#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/avx_attention.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"
#include "us4/runtime/backends/cpu_avx/scalar_attention.h"

#include <chrono>
#include <cmath>
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

    struct AttentionBenchmarkSample
    {
        std::string variant;
        std::size_t queryTokens = 0U;
        std::size_t keyTokens = 0U;
        std::size_t hiddenSize = 0U;
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

    AttentionBenchmarkSample MeasureScalar(const us4::core::Tensor& query, const us4::core::Tensor& key,
                                           const us4::core::Tensor& value,
                                           const us4::runtime::backends::cpu_avx::AttentionOptions& options)
    {
        const auto startedAt = Clock::now();
        const auto output = us4::runtime::backends::cpu_avx::ScalarAttention(query, key, value, options);
        const auto finishedAt = Clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

        return {
            .variant = "scalar-attention",
            .queryTokens = query.Dim(0),
            .keyTokens = key.Dim(0),
            .hiddenSize = query.Dim(1),
            .elapsedMs = elapsedMs,
            .checksum = Checksum(output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = elapsedMs > 0.0 ? (static_cast<double>(query.Dim(0)) * 1000.0 / elapsedMs) : 0.0,
        };
    }

    AttentionBenchmarkSample MeasureAvx(const us4::core::Tensor& query, const us4::core::Tensor& key,
                                        const us4::core::Tensor& value,
                                        const us4::runtime::backends::cpu_avx::AttentionOptions& options)
    {
        const auto startedAt = Clock::now();
        const auto output = us4::runtime::backends::cpu_avx::AvxAttention(query, key, value, options);
        const auto finishedAt = Clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

        return {
            .variant = "avx2-fused-attention",
            .queryTokens = query.Dim(0),
            .keyTokens = key.Dim(0),
            .hiddenSize = query.Dim(1),
            .elapsedMs = elapsedMs,
            .checksum = Checksum(output),
            .speedupVsScalar = 1.0,
            .tokensPerSecond = elapsedMs > 0.0 ? (static_cast<double>(query.Dim(0)) * 1000.0 / elapsedMs) : 0.0,
        };
    }

    void PersistReport(const std::vector<AttentionBenchmarkSample>& samples)
    {
        const std::filesystem::path outputPath = std::filesystem::path("runtime") / "benchmarks" /
                                                 "correctness" / "reports" / "cpu_attention_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n  \"suite\": \"cpu-attention-bench\",\n  \"samples\": [\n";
        for (std::size_t index = 0; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\"variant\":\"" << sample.variant << "\",\"query_tokens\":" << sample.queryTokens
                 << ",\"key_tokens\":" << sample.keyTokens << ",\"hidden_size\":" << sample.hiddenSize
                 << ",\"elapsed_ms\":" << std::fixed << std::setprecision(6) << sample.elapsedMs
                 << ",\"checksum\":" << sample.checksum << ",\"speedup_vs_scalar\":" << sample.speedupVsScalar
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
    const auto query = MakeTensor(64U, 128U, 0.25F);
    const auto key = MakeTensor(192U, 128U, -0.5F);
    const auto value = MakeTensor(192U, 128U, 0.75F);
    const auto cachedKey = MakeTensor(64U, 128U, 0.125F);
    const auto cachedValue = MakeTensor(64U, 128U, -0.25F);
    const us4::runtime::backends::cpu_avx::AttentionOptions options{
        .scale = 1.0F / std::sqrt(128.0F),
        .causalMask = true,
        .cachedKey = &cachedKey,
        .cachedValue = &cachedValue,
    };

    auto scalar = MeasureScalar(query, key, value, options);
    std::vector<AttentionBenchmarkSample> samples;
    samples.push_back(scalar);

    std::cout << "scalar-attention elapsed_ms=" << std::fixed << std::setprecision(3) << scalar.elapsedMs
              << " tokens_per_second=" << scalar.tokensPerSecond << " checksum=" << scalar.checksum << '\n';

    if (us4::runtime::backends::cpu_avx::HostSupportsAvx2MatMul())
    {
        auto avx = MeasureAvx(query, key, value, options);
        avx.speedupVsScalar = scalar.elapsedMs / avx.elapsedMs;
        samples.push_back(avx);
        std::cout << avx.variant << " elapsed_ms=" << avx.elapsedMs
                  << " speedup_vs_scalar=" << avx.speedupVsScalar
                  << " tokens_per_second=" << avx.tokensPerSecond
                  << " checksum=" << avx.checksum << '\n';
    }
    else
    {
        samples.push_back({
            .variant = "avx2-fused-attention-unavailable",
            .queryTokens = query.Dim(0),
            .keyTokens = key.Dim(0),
            .hiddenSize = query.Dim(1),
            .elapsedMs = 0.0,
            .checksum = 0.0,
            .speedupVsScalar = 1.0,
            .tokensPerSecond = 0.0,
        });
        std::cout << "avx2-fused-attention-unavailable elapsed_ms=0.000 speedup_vs_scalar=1.000 tokens_per_second=0.000 checksum=0.000\n";
    }

    PersistReport(samples);
    return 0;
}
