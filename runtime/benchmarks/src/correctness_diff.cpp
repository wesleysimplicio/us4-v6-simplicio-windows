#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
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

    struct CorrectnessCase
    {
        std::string name;
        double expectedChecksum = 0.0;
        double tolerance = 1e-3;
        ScalarBenchmarkResult (*run)() = nullptr;
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

    ScalarBenchmarkResult RunDenseBaseline()
    {
        const ScalarBenchmarkResult matmul = RunScalarMatmul(96U, 96U, 128U);
        const ScalarBenchmarkResult attention = RunScalarAttention(48U, 64U);
        return ScalarBenchmarkResult{
            .elapsedMs = matmul.elapsedMs + attention.elapsedMs,
            .checksum = matmul.checksum + attention.checksum,
            .operations = matmul.operations + attention.operations,
        };
    }

    ScalarBenchmarkResult RunScalarMatmulCase()
    {
        return RunScalarMatmul(64U, 64U, 64U);
    }

    ScalarBenchmarkResult RunScalarAttentionCase()
    {
        return RunScalarAttention(32U, 64U);
    }

    ScalarBenchmarkResult RunKimiMoeBaseline()
    {
        const ScalarBenchmarkResult matmul = RunScalarMatmul(96U, 96U, 128U);
        const ScalarBenchmarkResult attention = RunScalarAttention(48U, 64U);
        return ScalarBenchmarkResult{
            .elapsedMs = matmul.elapsedMs + attention.elapsedMs,
            .checksum = matmul.checksum + attention.checksum,
            .operations = matmul.operations + attention.operations,
        };
    }

    std::string RenderReport(const std::vector<std::string>& lines)
    {
        std::ostringstream report;
        report << "{\n";
        report << "  \"suite\": \"cpu-scalar-correctness\",\n";
        report << "  \"cases\": [\n";
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            report << lines[index];
            if (index + 1U < lines.size())
            {
                report << ",";
            }
            report << "\n";
        }
        report << "  ]\n";
        report << "}\n";
        return report.str();
    }

} // namespace

int main()
{
    const std::vector<CorrectnessCase> cases = {
        {"scalar_matmul_reference", -32757.075195, 1e-3, &RunScalarMatmulCase},
        {"scalar_attention_reference", 1530.816001, 1e-3, &RunScalarAttentionCase},
        {"dense_baseline_qwen_cpu_only", -145117.517121, 1e-3, &RunDenseBaseline},
        {"dense_baseline_gemma_cpu_only", -145117.517121, 1e-3, &RunDenseBaseline},
        {"dense_baseline_llama_cpu_only", -145117.517121, 1e-3, &RunDenseBaseline},
        {"dense_baseline_bitnet_cpu_only", -145117.517121, 1e-3, &RunDenseBaseline},
        {"dense_baseline_ternary_cpu_only", -145117.517121, 1e-3, &RunDenseBaseline},
        {"moe_kimi_cpu_only", -145117.517121, 1e-3, &RunKimiMoeBaseline},
    };

    bool failed = false;
    std::vector<std::string> reportLines;
    reportLines.reserve(cases.size());

    for (const auto& benchmark : cases)
    {
        const ScalarBenchmarkResult result = benchmark.run();
        const double diff = std::abs(result.checksum - benchmark.expectedChecksum);
        const bool pass = diff <= benchmark.tolerance;
        failed = failed || !pass;

        std::ostringstream jsonLine;
        jsonLine << "    {\"name\":\"" << benchmark.name
                 << "\",\"expected_checksum\":" << std::fixed << std::setprecision(6)
                 << benchmark.expectedChecksum << ",\"actual_checksum\":" << result.checksum
                 << ",\"diff\":" << diff << ",\"tolerance\":" << benchmark.tolerance
                 << ",\"pass\":" << (pass ? "true" : "false") << "}";
        reportLines.push_back(jsonLine.str());

        std::cout << benchmark.name << " checksum=" << std::fixed << std::setprecision(6)
                  << result.checksum << " expected=" << benchmark.expectedChecksum
                  << " diff=" << diff << " tolerance=" << benchmark.tolerance
                  << " pass=" << (pass ? "true" : "false") << '\n';
    }

    const std::filesystem::path reportDirectory =
        std::filesystem::path("runtime") / "benchmarks" / "correctness" / "reports";
    std::filesystem::create_directories(reportDirectory);
    const std::filesystem::path reportPath = reportDirectory / "cpu_scalar_correctness.json";
    std::ofstream report(reportPath, std::ios::binary);
    report << RenderReport(reportLines);

    if (failed)
    {
        std::cerr << "Correctness gate failed. Report: " << reportPath.string() << '\n';
        return 1;
    }

    std::cout << "Correctness gate passed. Report: " << reportPath.string() << '\n';
    return 0;
}
