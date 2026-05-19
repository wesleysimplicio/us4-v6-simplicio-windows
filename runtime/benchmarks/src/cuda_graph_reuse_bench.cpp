#include "us4/runtime/backends/cuda/cuda_backend.h"
#include "us4/runtime/backends/cuda/cuda_context.h"

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
    using Clock = std::chrono::steady_clock;
    constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;
    constexpr std::uint32_t kSteps = 512U;

    struct GraphReuseSample
    {
        std::string variant;
        std::uint32_t steps = 0U;
        std::uint32_t graphCaptures = 0U;
        std::uint32_t cacheHits = 0U;
        std::uint32_t graphReplays = 0U;
        double elapsedMs = 0.0;
        double speedupVsReset = 1.0;
    };

    us4::runtime::backends::HardwareCapabilities MakeCudaCapabilities()
    {
        using namespace us4::runtime::backends;

        HardwareCapabilities capabilities;
        capabilities.hasCuda = true;
        capabilities.primaryAdapterVendor = BackendVendor::kNvidia;
        capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
        capabilities.primaryAdapterName = "GeForce RTX 4090";
        capabilities.discreteGpuCount = 1U;
        capabilities.hasUnifiedMemory = true;
        capabilities.budget.hostBytes = 64ULL * kGiB;
        capabilities.budget.deviceBytes = 24ULL * kGiB;
        capabilities.budget.storageBytes = 2ULL * 1024ULL * kGiB;
        return capabilities;
    }

    us4::runtime::backends::SessionRequest MakeCudaRequest()
    {
        using namespace us4::runtime::backends;

        SessionRequest request;
        request.modelId = "qwen-0.5b";
        request.mode = RuntimeMode::kBalanced;
        request.maxContextTokens = 4096U;
        request.maxGenerationTokens = 64U;
        request.preferMaxThroughput = true;
        request.seed = 77U;
        return request;
    }

    void PersistReport(const std::vector<GraphReuseSample>& samples)
    {
        const std::filesystem::path outputPath = std::filesystem::path("runtime") / "benchmarks" /
                                                 "correctness" / "reports" /
                                                 "cuda_graph_reuse_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n  \"suite\": \"cuda-graph-reuse-bench\",\n  \"samples\": [\n";
        for (std::size_t index = 0U; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\"variant\":\"" << sample.variant << "\",\"steps\":" << sample.steps
                 << ",\"graph_captures\":" << sample.graphCaptures
                 << ",\"cache_hits\":" << sample.cacheHits
                 << ",\"graph_replays\":" << sample.graphReplays << ",\"elapsed_ms\":" << std::fixed
                 << std::setprecision(6) << sample.elapsedMs
                 << ",\"speedup_vs_reset\":" << sample.speedupVsReset << "}";
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

    GraphReuseSample
    MeasureResetPerStep(const us4::runtime::backends::cuda::CudaExecutionPlan& plan)
    {
        using namespace us4::runtime::backends::cuda;

        auto context = CudaContext::Create(plan);
        const auto startedAt = Clock::now();
        for (std::uint32_t step = 0U; step < kSteps; ++step)
        {
            const auto executable =
                context.CaptureGraph("decode-step", {"draft", "verify", "sample"});
            if (!executable.has_value())
            {
                break;
            }
            (void)context.ReplayGraph(*executable, 6U, 100U + step);
            context.ClearGraphCache();
        }
        const auto finishedAt = Clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

        return {
            .variant = "reset-per-step",
            .steps = kSteps,
            .graphCaptures = context.GraphCaptureCount(),
            .cacheHits = context.GraphCacheHitCount(),
            .graphReplays = context.GraphReplayCount(),
            .elapsedMs = elapsedMs,
            .speedupVsReset = 1.0,
        };
    }

    GraphReuseSample
    MeasureReuseCrossStep(const us4::runtime::backends::cuda::CudaExecutionPlan& plan)
    {
        using namespace us4::runtime::backends::cuda;

        auto context = CudaContext::Create(plan);
        const auto startedAt = Clock::now();
        for (std::uint32_t step = 0U; step < kSteps; ++step)
        {
            const auto lease = context.AcquireSpeculativeGraph(
                "peagle:decode:batch1:tree4", "decode-step", {"draft", "verify", "sample"});
            if (!lease.has_value())
            {
                break;
            }
            (void)context.ReplayGraph(lease->executable, 6U, 100U + step);
        }
        const auto finishedAt = Clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

        return {
            .variant = "reuse-cross-step",
            .steps = kSteps,
            .graphCaptures = context.GraphCaptureCount(),
            .cacheHits = context.GraphCacheHitCount(),
            .graphReplays = context.GraphReplayCount(),
            .elapsedMs = elapsedMs,
            .speedupVsReset = 1.0,
        };
    }
} // namespace

int main()
{
    using namespace us4::runtime::backends::cuda;

    const auto plan = CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
    const auto reset = MeasureResetPerStep(plan);
    auto reuse = MeasureReuseCrossStep(plan);
    reuse.speedupVsReset = reuse.elapsedMs > 0.0 ? (reset.elapsedMs / reuse.elapsedMs) : 1.0;

    std::vector<GraphReuseSample> samples{reset, reuse};
    PersistReport(samples);

    std::cout << reset.variant << " elapsed_ms=" << std::fixed << std::setprecision(6)
              << reset.elapsedMs << " graph_captures=" << reset.graphCaptures
              << " cache_hits=" << reset.cacheHits << " graph_replays=" << reset.graphReplays
              << '\n';
    std::cout << reuse.variant << " elapsed_ms=" << reuse.elapsedMs
              << " graph_captures=" << reuse.graphCaptures << " cache_hits=" << reuse.cacheHits
              << " graph_replays=" << reuse.graphReplays
              << " speedup_vs_reset=" << reuse.speedupVsReset << '\n';
    return 0;
}
