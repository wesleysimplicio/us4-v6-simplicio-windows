#include "us4/runtime/backends/cuda/cublas_fallback.h"
#include "us4/runtime/backends/cuda/cuda_backend.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;

    struct PolicySample
    {
        std::string label;
        std::string primary;
        std::string fallback;
        double primaryLatencyUs = 0.0;
        double fallbackLatencyUs = 0.0;
        double speedupVsFallback = 1.0;
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

    void PersistReport(const std::vector<PolicySample>& samples)
    {
        const std::filesystem::path outputPath = std::filesystem::path("runtime") / "benchmarks" /
                                                 "correctness" / "reports" /
                                                 "cuda_fallback_policy_bench.json";
        std::filesystem::create_directories(outputPath.parent_path());

        std::ostringstream json;
        json << "{\n  \"suite\": \"cuda-fallback-policy-bench\",\n  \"samples\": [\n";
        for (std::size_t index = 0U; index < samples.size(); ++index)
        {
            const auto& sample = samples[index];
            json << "    {\"label\":\"" << sample.label << "\",\"primary\":\"" << sample.primary
                 << "\",\"fallback\":\"" << sample.fallback
                 << "\",\"primary_latency_us\":" << std::fixed << std::setprecision(6)
                 << sample.primaryLatencyUs
                 << ",\"fallback_latency_us\":" << sample.fallbackLatencyUs
                 << ",\"speedup_vs_fallback\":" << sample.speedupVsFallback << "}";
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
    using namespace us4::runtime::backends::cuda;

    const auto plan = CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
    const std::vector<std::pair<std::string, CudaMatmulShape>> cases = {
        {"aligned_fp16_decode",
         {.rows = 512U,
          .columns = 512U,
          .innerDimension = 256U,
          .batchCount = 1U,
          .transposeWeights = false,
          .preferDeterministic = false,
          .useFp16 = true,
          .useBf16 = false,
          .allowCustomKernel = true}},
        {"irregular_batched_prefill",
         {.rows = 1536U,
          .columns = 1792U,
          .innerDimension = 960U,
          .batchCount = 8U,
          .transposeWeights = true,
          .preferDeterministic = false,
          .useFp16 = true,
          .useBf16 = false,
          .allowCustomKernel = true}},
        {"deterministic_small_step",
         {.rows = 256U,
          .columns = 256U,
          .innerDimension = 256U,
          .batchCount = 1U,
          .transposeWeights = false,
          .preferDeterministic = true,
          .useFp16 = true,
          .useBf16 = false,
          .allowCustomKernel = true}},
    };

    std::vector<PolicySample> samples;
    for (const auto& [label, shape] : cases)
    {
        const auto dispatch = CublasFallbackWrapper::SelectMatmulDispatch(plan, shape);
        samples.push_back({
            .label = label,
            .primary = ToString(dispatch.primary),
            .fallback = ToString(dispatch.fallback),
            .primaryLatencyUs = dispatch.estimatedLatencyUs,
            .fallbackLatencyUs = dispatch.estimatedFallbackLatencyUs,
            .speedupVsFallback = dispatch.estimatedSpeedupVsFallback,
        });

        std::cout << label << " primary=" << ToString(dispatch.primary)
                  << " fallback=" << ToString(dispatch.fallback)
                  << " primary_latency_us=" << std::fixed << std::setprecision(6)
                  << dispatch.estimatedLatencyUs
                  << " fallback_latency_us=" << dispatch.estimatedFallbackLatencyUs
                  << " speedup_vs_fallback=" << dispatch.estimatedSpeedupVsFallback << '\n';
    }

    PersistReport(samples);
    return 0;
}
