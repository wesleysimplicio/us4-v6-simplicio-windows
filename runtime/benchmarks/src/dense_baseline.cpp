#include "us4/profiles/profile_catalog.h"
#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

namespace
{

    using Clock = std::chrono::steady_clock;

    struct ScalarBenchmarkResult
    {
        double elapsedMs = 0.0;
        double checksum = 0.0;
        std::size_t operations = 0;
    };

    std::vector<float> MakeSequence(std::size_t size, float seed)
    {
        std::vector<float> values(size, 0.0F);
        for (std::size_t index = 0; index < size; ++index)
        {
            const int centered = static_cast<int>(index % 23U) - 11;
            values[index] = seed + static_cast<float>(centered) * 0.03125F;
        }
        return values;
    }

    ScalarBenchmarkResult RunScalarMatmul(std::size_t m, std::size_t n, std::size_t k)
    {
        const std::vector<float> lhs = MakeSequence(m * k, 0.25F);
        const std::vector<float> rhs = MakeSequence(k * n, -0.5F);
        std::vector<float> output(m * n, 0.0F);

        const auto startedAt = Clock::now();
        for (std::size_t row = 0; row < m; ++row)
        {
            for (std::size_t col = 0; col < n; ++col)
            {
                float acc = 0.0F;
                for (std::size_t depth = 0; depth < k; ++depth)
                {
                    acc += lhs[row * k + depth] * rhs[depth * n + col];
                }
                output[row * n + col] = acc;
            }
        }
        const auto finishedAt = Clock::now();

        return ScalarBenchmarkResult{
            .elapsedMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            .checksum = std::accumulate(output.begin(), output.end(), 0.0),
            .operations = m * n * k,
        };
    }

    ScalarBenchmarkResult RunScalarAttention(std::size_t sequenceLength, std::size_t headDim)
    {
        const std::vector<float> query = MakeSequence(sequenceLength * headDim, 0.125F);
        const std::vector<float> key = MakeSequence(sequenceLength * headDim, -0.25F);
        const std::vector<float> value = MakeSequence(sequenceLength * headDim, 0.75F);
        std::vector<float> output(sequenceLength * headDim, 0.0F);

        const float scale = 1.0F / std::sqrt(static_cast<float>(headDim));
        const auto startedAt = Clock::now();
        for (std::size_t token = 0; token < sequenceLength; ++token)
        {
            std::vector<float> logits(token + 1U, 0.0F);
            float maxLogit = -std::numeric_limits<float>::infinity();
            for (std::size_t keyToken = 0; keyToken <= token; ++keyToken)
            {
                float dot = 0.0F;
                for (std::size_t channel = 0; channel < headDim; ++channel)
                {
                    dot += query[token * headDim + channel] * key[keyToken * headDim + channel];
                }
                logits[keyToken] = dot * scale;
                maxLogit = std::max(maxLogit, logits[keyToken]);
            }

            float partition = 0.0F;
            for (float& logit : logits)
            {
                logit = std::exp(logit - maxLogit);
                partition += logit;
            }

            for (std::size_t channel = 0; channel < headDim; ++channel)
            {
                float acc = 0.0F;
                for (std::size_t keyToken = 0; keyToken <= token; ++keyToken)
                {
                    const float weight = logits[keyToken] / partition;
                    acc += weight * value[keyToken * headDim + channel];
                }
                output[token * headDim + channel] = acc;
            }
        }
        const auto finishedAt = Clock::now();

        return ScalarBenchmarkResult{
            .elapsedMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            .checksum = std::accumulate(output.begin(), output.end(), 0.0),
            .operations = sequenceLength * sequenceLength * headDim,
        };
    }

    void PrintResult(const us4::runtime::benchmarks::BenchmarkCase& benchmark,
                     const ScalarBenchmarkResult& result)
    {
        std::cout << benchmark.name << " backend=" << benchmark.backend
                  << " scenario=" << benchmark.scenario << " model=" << benchmark.modelId
                  << " adapter=" << benchmark.adapterId << " profile=" << benchmark.profileId
                  << " mode=" << benchmark.runtimeMode
                  << " prompt_tokens=" << benchmark.promptTokens
                  << " generation_tokens=" << benchmark.generationTokens
                  << " elapsed_ms=" << std::fixed << std::setprecision(3) << result.elapsedMs
                  << " checksum=" << result.checksum << " operations=" << result.operations << '\n';
    }

} // namespace

int main()
{
    const auto cpuProfile = us4::profiles::ProfileCatalog::FindById("cpu-only");
    if (cpuProfile.has_value())
    {
        std::cout << "dense_baseline profile=" << cpuProfile->id
                  << " target_context=" << cpuProfile->targetContextTokens
                  << " target_batch=" << cpuProfile->targetBatchSize
                  << " backend=" << cpuProfile->preferredBackend << '\n';
    }

    for (const auto& benchmark :
         us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("cpu"))
    {
        if (benchmark.name == "scalar_matmul_reference")
        {
            PrintResult(benchmark, RunScalarMatmul(64U, 64U, 64U));
            continue;
        }

        if (benchmark.name == "scalar_attention_reference")
        {
            PrintResult(benchmark, RunScalarAttention(32U, 64U));
            continue;
        }

        if (benchmark.name == "dense_baseline_qwen_cpu_only" ||
            benchmark.name == "dense_baseline_gemma_cpu_only" ||
            benchmark.name == "dense_baseline_llama_cpu_only" ||
            benchmark.name == "dense_baseline_bitnet_cpu_only" ||
            benchmark.name == "dense_baseline_ternary_cpu_only" ||
            benchmark.name == "dense_baseline")
        {
            const ScalarBenchmarkResult matmul = RunScalarMatmul(96U, 96U, 128U);
            const ScalarBenchmarkResult attention = RunScalarAttention(48U, 64U);
            PrintResult(benchmark, ScalarBenchmarkResult{
                                       .elapsedMs = matmul.elapsedMs + attention.elapsedMs,
                                       .checksum = matmul.checksum + attention.checksum,
                                       .operations = matmul.operations + attention.operations,
                                   });
        }
    }
    return 0;
}
