#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/runtime_context.h"
#include "us4/runtime/backends/cuda/cuda_backend.h"
#include "us4/runtime/backends/directml/dml_device.h"
#include "us4/runtime/backends/directml/dml_graph.h"
#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

    using Clock = std::chrono::steady_clock;

    struct BenchmarkSample
    {
        std::string benchmarkName;
        std::string backend;
        std::string status;
        std::string confidence;
        double elapsedMs = 0.0;
        double tokensPerSecond = 0.0;
        double latencyPerTokenMs = 0.0;
        std::size_t promptTokens = 0U;
        std::size_t generationTokens = 0U;
        std::size_t peakBytes = 0U;
        std::vector<std::string> issueCodes;
        std::string detail;
    };

    template <typename Fn> double MeasureMilliseconds(Fn&& fn)
    {
        const auto startedAt = Clock::now();
        fn();
        const auto finishedAt = Clock::now();
        return std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
    }

    std::string EscapeJson(std::string_view value)
    {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            default:
                escaped.push_back(ch);
                break;
            }
        }
        return escaped;
    }

    std::string RenderStringArray(const std::vector<std::string>& values)
    {
        std::ostringstream json;
        json << '[';
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0U)
            {
                json << ',';
            }
            json << '"' << EscapeJson(values[index]) << '"';
        }
        json << ']';
        return json.str();
    }

    us4::runtime::backends::RuntimeMode ParseRuntimeMode(const std::string& mode)
    {
        using us4::runtime::backends::RuntimeMode;

        if (mode == "FULL")
        {
            return RuntimeMode::kFull;
        }
        if (mode == "BALANCED")
        {
            return RuntimeMode::kBalanced;
        }
        if (mode == "DEGRADED")
        {
            return RuntimeMode::kDegraded;
        }
        if (mode == "ULTRA_LOW")
        {
            return RuntimeMode::kUltraLow;
        }
        if (mode == "MICRO")
        {
            return RuntimeMode::kMicro;
        }
        if (mode == "NANO")
        {
            return RuntimeMode::kNano;
        }
        return RuntimeMode::kCpuOnly;
    }

    us4::runtime::backends::SessionRequest
    MakeRequest(const us4::runtime::benchmarks::BenchmarkCase& benchmark,
                const std::string& backendName)
    {
        using namespace us4::runtime::backends;

        return SessionRequest{
            .modelId = benchmark.modelId,
            .preferredBackend = backendName,
            .mode = ParseRuntimeMode(benchmark.runtimeMode),
            .precision = backendName == "cpu" ? PrecisionMode::kFp32 : PrecisionMode::kFp16,
            .maxContextTokens = static_cast<std::uint32_t>(benchmark.promptTokens * 4U),
            .maxGenerationTokens = static_cast<std::uint32_t>(benchmark.generationTokens),
            .enableSpeculative = backendName == "cpu",
            .allowNpu = false,
            .preferLowLatency = backendName == "directml",
            .preferMaxThroughput = backendName == "cuda",
        };
    }

    us4::runtime::backends::HardwareCapabilities MakeCpuCapabilities()
    {
        using namespace us4::runtime::backends;

        HardwareCapabilities capabilities{};
        capabilities.hasAvx2 = true;
        capabilities.hasDirectMl = false;
        capabilities.hasCuda = false;
        capabilities.primaryAdapterName = "CPU Reference Host";
        capabilities.primaryAdapterVendor = BackendVendor::kIntel;
        capabilities.primaryAdapterClass = DeviceClass::kCpuOnly;
        capabilities.budget.hostBytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.storageBytes = 256ULL * 1024ULL * 1024ULL * 1024ULL;
        return capabilities;
    }

    us4::runtime::backends::HardwareCapabilities MakeCudaCapabilities()
    {
        using namespace us4::runtime::backends;

        HardwareCapabilities capabilities{};
        capabilities.hasCuda = true;
        capabilities.hasUnifiedMemory = true;
        capabilities.primaryAdapterName = "NVIDIA RTX 4090";
        capabilities.primaryAdapterVendor = BackendVendor::kNvidia;
        capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
        capabilities.discreteGpuCount = 1U;
        capabilities.budget.deviceBytes = 24ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.hostBytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.storageBytes = 256ULL * 1024ULL * 1024ULL * 1024ULL;
        return capabilities;
    }

    us4::runtime::backends::HardwareCapabilities MakeDirectMlCapabilities()
    {
        using namespace us4::runtime::backends;

        HardwareCapabilities capabilities{};
        capabilities.hasDirectMl = true;
        capabilities.hasAvx2 = true;
        capabilities.primaryAdapterName = "Radeon RX Test";
        capabilities.primaryAdapterVendor = BackendVendor::kAmd;
        capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
        capabilities.discreteGpuCount = 1U;
        capabilities.budget.deviceBytes = 20ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.hostBytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.storageBytes = 256ULL * 1024ULL * 1024ULL * 1024ULL;
        return capabilities;
    }

    std::vector<us4::runtime::backends::directml::DmlGraphNode> BuildLlamaGraphNodes()
    {
        using namespace us4::runtime::backends::directml;

        return {
            {
                .name = "llama.prefill.rmsnorm",
                .kind = DmlOperatorKind::kRmsNorm,
                .dataType = DmlTensorDataType::kFp16,
                .inputCount = 1U,
                .outputCount = 1U,
                .usesPersistentWeights = true,
                .allowChunking = true,
                .temporaryBytes = 8ULL * 1024ULL * 1024ULL,
                .persistentBytes = 768ULL * 1024ULL * 1024ULL,
            },
            {
                .name = "llama.prefill.attention",
                .kind = DmlOperatorKind::kAttention,
                .dataType = DmlTensorDataType::kFp16,
                .inputCount = 3U,
                .outputCount = 1U,
                .usesPersistentWeights = false,
                .allowChunking = true,
                .temporaryBytes = 96ULL * 1024ULL * 1024ULL,
                .persistentBytes = 512ULL * 1024ULL * 1024ULL,
            },
            {
                .name = "llama.decode.matmul",
                .kind = DmlOperatorKind::kMatMul,
                .dataType = DmlTensorDataType::kFp16,
                .inputCount = 2U,
                .outputCount = 1U,
                .usesPersistentWeights = true,
                .allowChunking = true,
                .temporaryBytes = 64ULL * 1024ULL * 1024ULL,
                .persistentBytes = 5ULL * 1024ULL * 1024ULL * 1024ULL,
            },
        };
    }

    BenchmarkSample
    RunCpuCase(const us4::runtime::benchmarks::BenchmarkCase& benchmark)
    {
        BenchmarkSample sample{
            .benchmarkName = benchmark.name,
            .backend = benchmark.backend,
            .status = "fail",
            .confidence = "synthetic-local-contract",
            .promptTokens = benchmark.promptTokens,
            .generationTokens = benchmark.generationTokens,
        };

        const auto capabilities = MakeCpuCapabilities();
        const auto request = MakeRequest(benchmark, "cpu");
        const auto plan = us4::core::RuntimeContext::BuildPlan(request, capabilities);
        const std::string prompt = "hello llama runtime contract bench";

        us4::core::CpuScalarRunResult runResult{};
        sample.elapsedMs = MeasureMilliseconds([&]() { runResult = us4::core::ExecuteCpuScalarRun(plan, prompt); });
        if (!runResult.ok)
        {
            sample.detail = runResult.error;
            return sample;
        }

        for (const auto& issue : plan.issues)
        {
            sample.issueCodes.push_back(issue.code);
        }

        const auto generatedCount = std::max<std::size_t>(1U, runResult.report.generatedTokens.size());
        sample.tokensPerSecond =
            static_cast<double>(generatedCount) / std::max(sample.elapsedMs / 1000.0, 0.000001);
        sample.latencyPerTokenMs = sample.elapsedMs / static_cast<double>(generatedCount);
        sample.peakBytes =
            std::max(plan.residency.expectedHostBytes, plan.residency.expectedDeviceBytes);
        sample.status = "pass";
        sample.detail = "cpu-scalar-avx-contract";
        return sample;
    }

    BenchmarkSample
    RunCudaCase(const us4::runtime::benchmarks::BenchmarkCase& benchmark)
    {
        using namespace us4::runtime::backends::cuda;

        BenchmarkSample sample{
            .benchmarkName = benchmark.name,
            .backend = benchmark.backend,
            .status = "fail",
            .confidence = "synthetic-local-contract",
            .promptTokens = benchmark.promptTokens,
            .generationTokens = benchmark.generationTokens,
        };

        const auto capabilities = MakeCudaCapabilities();
        const auto request = MakeRequest(benchmark, "cuda");
        const auto plan = CudaBackend::BuildExecutionPlan(request, capabilities);

        for (const auto& issue : plan.issues)
        {
            sample.issueCodes.push_back(issue.code);
        }

        CudaStepResult prefill{};
        std::vector<std::int32_t> generatedTokens;
        sample.elapsedMs = MeasureMilliseconds(
            [&]()
            {
                prefill = CudaBackend::PreparePrefill(plan, "hello llama cuda contract bench");
                std::uint32_t stepIndex = 0U;
                while (generatedTokens.size() < benchmark.generationTokens)
                {
                    const auto decode = CudaBackend::PrepareDecode(plan, prefill.chunk, stepIndex++);
                    generatedTokens.insert(generatedTokens.end(), decode.chunk.tokens.begin(),
                                           decode.chunk.tokens.end());
                    if (decode.chunk.isTerminal)
                    {
                        break;
                    }
                }
            });

        if (prefill.chunk.tokens.empty() || generatedTokens.empty())
        {
            sample.detail = "cuda-reference-steps-empty";
            return sample;
        }

        const auto generatedCount = std::max<std::size_t>(1U, generatedTokens.size());
        sample.tokensPerSecond =
            static_cast<double>(generatedCount) / std::max(sample.elapsedMs / 1000.0, 0.000001);
        sample.latencyPerTokenMs = sample.elapsedMs / static_cast<double>(generatedCount);
        sample.peakBytes = plan.memory.weightsDeviceBytes + plan.memory.kvDeviceBytes +
                           plan.memory.scratchBytes + plan.memory.hostStagingBytes;
        sample.status = "pass";
        sample.detail = std::string("cuda-") + ToString(plan.flavor);
        return sample;
    }

    BenchmarkSample
    RunDirectMlCase(const us4::runtime::benchmarks::BenchmarkCase& benchmark)
    {
        using namespace us4::runtime::backends;
        using namespace us4::runtime::backends::directml;

        BenchmarkSample sample{
            .benchmarkName = benchmark.name,
            .backend = benchmark.backend,
            .status = "fail",
            .confidence = "synthetic-local-contract",
            .promptTokens = benchmark.promptTokens,
            .generationTokens = benchmark.generationTokens,
        };

        const auto capabilities = MakeDirectMlCapabilities();
        DmlDevice device({
            .preferIntegratedGpu = false,
            .preferLowLatency = true,
            .allowTensorReuse = true,
            .enableBackgroundCompilation = true,
        });
        if (!device.Initialize(capabilities))
        {
            sample.detail = "directml-device-init-failed";
            if (const auto issue = device.LastIssue(); issue.has_value())
            {
                sample.issueCodes.push_back(issue->code);
            }
            return sample;
        }

        if (!device.PrepareGraphSession(benchmark.modelId, DmlExecutionPhase::kPrefill,
                                        6ULL * 1024ULL * 1024ULL * 1024ULL))
        {
            sample.detail = "directml-session-prepare-failed";
            if (const auto issue = device.LastIssue(); issue.has_value())
            {
                sample.issueCodes.push_back(issue->code);
            }
            return sample;
        }

        DmlGraph graph(&device);
        graph.RecordNodes(BuildLlamaGraphNodes());
        DmlCompileResult compile{};
        DmlDispatchResult prefill{};
        DmlDispatchResult decode{};
        sample.elapsedMs = MeasureMilliseconds(
            [&]()
            {
                compile = graph.Compile({
                    .precision = PrecisionMode::kFp16,
                    .enableOperatorFusion = true,
                    .enablePersistentDescriptors = true,
                    .enableChunkedCompilation = true,
                    .maxTemporaryBytes = 256ULL * 1024ULL * 1024ULL,
                    .maxPersistentBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL,
                });
                if (compile.compiled)
                {
                    prefill = graph.Dispatch({
                        .phase = DmlExecutionPhase::kPrefill,
                        .tokenCount = static_cast<std::uint32_t>(benchmark.promptTokens),
                        .batchSize = 1U,
                        .sequenceLength = static_cast<std::uint32_t>(benchmark.promptTokens),
                        .allowAsyncCompletion = true,
                    });
                    decode = graph.Dispatch({
                        .phase = DmlExecutionPhase::kDecode,
                        .tokenCount = static_cast<std::uint32_t>(benchmark.generationTokens),
                        .batchSize = 1U,
                        .sequenceLength = static_cast<std::uint32_t>(benchmark.promptTokens + benchmark.generationTokens),
                        .allowAsyncCompletion = true,
                    });
                }
            });

        if (!compile.compiled || !prefill.submitted || !decode.submitted)
        {
            sample.detail = "directml-graph-contract-failed";
            if (compile.issue.has_value())
            {
                sample.issueCodes.push_back(compile.issue->code);
            }
            if (prefill.issue.has_value())
            {
                sample.issueCodes.push_back(prefill.issue->code);
            }
            if (decode.issue.has_value())
            {
                sample.issueCodes.push_back(decode.issue->code);
            }
            return sample;
        }

        const auto generatedCount = std::max<std::size_t>(1U, benchmark.generationTokens);
        sample.tokensPerSecond =
            static_cast<double>(generatedCount) / std::max(sample.elapsedMs / 1000.0, 0.000001);
        sample.latencyPerTokenMs = sample.elapsedMs / static_cast<double>(generatedCount);
        sample.peakBytes = graph.Stats().temporaryBytes + graph.Stats().persistentBytes;
        sample.status = "pass";
        sample.detail = "directml-graph-contract";
        return sample;
    }

    std::string RenderReport(const std::vector<BenchmarkSample>& samples)
    {
        std::ostringstream json;
        json << "{\n";
        json << "  \"suite\": \"llama-backend-contract-bench\",\n";
        json << "  \"measurement_kind\": \"synthetic-local-contract\",\n";
        json << "  \"samples\": [\n";
        for (std::size_t index = 0U; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\n";
            json << "      \"benchmark\": \"" << EscapeJson(sample.benchmarkName) << "\",\n";
            json << "      \"backend\": \"" << EscapeJson(sample.backend) << "\",\n";
            json << "      \"status\": \"" << EscapeJson(sample.status) << "\",\n";
            json << "      \"confidence\": \"" << EscapeJson(sample.confidence) << "\",\n";
            json << "      \"elapsed_ms\": " << std::fixed << std::setprecision(6)
                 << sample.elapsedMs << ",\n";
            json << "      \"tokens_per_second\": " << sample.tokensPerSecond << ",\n";
            json << "      \"latency_per_token_ms\": " << sample.latencyPerTokenMs << ",\n";
            json << "      \"prompt_tokens\": " << sample.promptTokens << ",\n";
            json << "      \"generation_tokens\": " << sample.generationTokens << ",\n";
            json << "      \"peak_bytes\": " << sample.peakBytes << ",\n";
            json << "      \"issue_codes\": " << RenderStringArray(sample.issueCodes) << ",\n";
            json << "      \"detail\": \"" << EscapeJson(sample.detail) << "\"\n";
            json << "    }";
            if (index + 1U < samples.size())
            {
                json << ',';
            }
            json << '\n';
        }
        json << "  ]\n";
        json << "}\n";
        return json.str();
    }

} // namespace

int main()
{
    using us4::runtime::benchmarks::BenchmarkRegistry;

    const auto cpuCase = BenchmarkRegistry::FindByName("llama_8b_cpu_avx_contract");
    const auto cudaCase = BenchmarkRegistry::FindByName("llama_8b_cuda_contract");
    const auto directmlCase = BenchmarkRegistry::FindByName("llama_8b_directml_contract");

    if (!cpuCase.has_value() || !cudaCase.has_value() || !directmlCase.has_value())
    {
        std::cerr << "Llama backend contract benchmark cases are missing from the registry.\n";
        return 1;
    }

    const std::vector<BenchmarkSample> samples{
        RunCpuCase(*cpuCase),
        RunCudaCase(*cudaCase),
        RunDirectMlCase(*directmlCase),
    };

    bool failed = false;
    for (const auto& sample : samples)
    {
        failed = failed || sample.status != "pass";
        std::cout << sample.benchmarkName << " backend=" << sample.backend
                  << " status=" << sample.status << " elapsed_ms=" << std::fixed
                  << std::setprecision(6) << sample.elapsedMs
                  << " tokens_per_second=" << sample.tokensPerSecond
                  << " latency_per_token_ms=" << sample.latencyPerTokenMs
                  << " peak_bytes=" << sample.peakBytes;
        if (!sample.issueCodes.empty())
        {
            std::cout << " issue_codes=";
            for (std::size_t index = 0U; index < sample.issueCodes.size(); ++index)
            {
                if (index > 0U)
                {
                    std::cout << ',';
                }
                std::cout << sample.issueCodes[index];
            }
        }
        if (!sample.detail.empty())
        {
            std::cout << " detail=" << sample.detail;
        }
        std::cout << '\n';
    }

    const std::filesystem::path reportDirectory =
        std::filesystem::path("runtime") / "benchmarks" / "correctness" / "reports";
    std::filesystem::create_directories(reportDirectory);
    const std::filesystem::path reportPath = reportDirectory / "llama_backend_contract_bench.json";
    std::ofstream report(reportPath, std::ios::binary);
    report << RenderReport(samples);

    if (failed)
    {
        std::cerr << "Llama backend contract benchmark failed. Report: " << reportPath.string()
                  << '\n';
        return 1;
    }

    std::cout << "Llama backend contract benchmark passed. Report: " << reportPath.string()
              << '\n';
    return 0;
}
