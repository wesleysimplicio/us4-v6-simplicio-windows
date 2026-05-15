#include "us4/runtime/benchmarks/benchmark_registry.h"

namespace us4::runtime::benchmarks
{

    std::vector<BenchmarkCase> BenchmarkRegistry::DefaultCases()
    {
        return {
            BenchmarkCase{
                .name = "scalar_matmul_reference",
                .backend = "cpu",
                .scenario = "fp32-naive-gemm",
                .modelId = "synthetic-dense",
                .adapterId = "dense-reference",
                .profileId = "cpu-only",
                .runtimeMode = "CPU_ONLY",
                .promptTokens = 128,
                .generationTokens = 0,
                .requiresGpu = false,
                .participatesInCorrectnessGate = true,
                .touchesCli = false,
            },
            BenchmarkCase{
                .name = "scalar_attention_reference",
                .backend = "cpu",
                .scenario = "causal-softmax-kv-concat",
                .modelId = "synthetic-dense",
                .adapterId = "dense-reference",
                .profileId = "cpu-only",
                .runtimeMode = "CPU_ONLY",
                .promptTokens = 128,
                .generationTokens = 32,
                .requiresGpu = false,
                .participatesInCorrectnessGate = true,
                .touchesCli = false,
            },
            BenchmarkCase{
                .name = "dense_baseline_qwen_cpu_only",
                .backend = "cpu",
                .scenario = "prefill-decode-dense",
                .modelId = "qwen-0.5b",
                .adapterId = "dense-qwen",
                .profileId = "cpu-only",
                .runtimeMode = "CPU_ONLY",
                .promptTokens = 128,
                .generationTokens = 32,
                .requiresGpu = false,
                .participatesInCorrectnessGate = true,
                .touchesCli = true,
            },
            BenchmarkCase{
                .name = "dense_baseline_gemma_cpu_only",
                .backend = "cpu",
                .scenario = "prefill-decode-dense",
                .modelId = "gemma-2b",
                .adapterId = "dense-gemma",
                .profileId = "cpu-only",
                .runtimeMode = "CPU_ONLY",
                .promptTokens = 128,
                .generationTokens = 32,
                .requiresGpu = false,
                .participatesInCorrectnessGate = true,
                .touchesCli = true,
            },
            BenchmarkCase{
                .name = "dense_baseline",
                .backend = "cpu",
                .scenario = "single-batch-dense",
                .modelId = "qwen-0.5b",
                .adapterId = "dense-qwen",
                .profileId = "cpu-only",
                .runtimeMode = "CPU_ONLY",
                .promptTokens = 128,
                .generationTokens = 32,
                .requiresGpu = false,
                .participatesInCorrectnessGate = true,
                .touchesCli = true,
            },
            BenchmarkCase{
                .name = "moe_throughput",
                .backend = "cuda",
                .scenario = "expert-routing-throughput",
                .modelId = "deepseek-moe",
                .adapterId = "moe-deepseek",
                .profileId = "full",
                .runtimeMode = "FULL",
                .promptTokens = 512,
                .generationTokens = 64,
                .requiresGpu = true,
                .participatesInCorrectnessGate = false,
                .touchesCli = false,
            },
            BenchmarkCase{
                .name = "correctness_diff",
                .backend = "cross-backend",
                .scenario = "logit-delta-gate",
                .modelId = "qwen-0.5b",
                .adapterId = "dense-qwen",
                .profileId = "cpu-only",
                .runtimeMode = "CPU_ONLY",
                .promptTokens = 32,
                .generationTokens = 32,
                .requiresGpu = false,
                .participatesInCorrectnessGate = true,
                .touchesCli = true,
            },
        };
    }

    std::vector<BenchmarkCase> BenchmarkRegistry::CasesForBackend(const std::string& backend)
    {
        std::vector<BenchmarkCase> cases;
        for (const BenchmarkCase& benchmark : DefaultCases())
        {
            if (benchmark.backend == backend || benchmark.backend == "cross-backend")
            {
                cases.push_back(benchmark);
            }
        }
        return cases;
    }

} // namespace us4::runtime::benchmarks
