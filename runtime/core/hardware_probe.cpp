#include "runtime/core/hardware_probe.h"

#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/runtime/backends/backend_selector.h"
#include "us4/runtime/backends/hardware_probe.h"

#include <cstdint>
#include <sstream>

namespace us4::core
{
    namespace
    {
        constexpr std::uint64_t kOneGiB = 1024ULL * 1024ULL * 1024ULL;

        std::vector<ProbeAdvisory> BuildAdvisories(const ProbeSummary& summary)
        {
            std::vector<ProbeAdvisory> advisories;
            if (!summary.HasAccelerator())
            {
                advisories.push_back({
                    "warning",
                    "fallback.cpu_only",
                    "No GPU or NPU backend was detected. The runtime will stay on the CPU path.",
                });
            }

            if (summary.capabilities.primaryAdapterName == "Unknown GPU")
            {
                advisories.push_back({
                    "info",
                    "probe.adapter_unknown",
                    "GPU probing returned an unknown adapter name. Install vendor tooling for "
                    "richer detection.",
                });
            }

            if (summary.selectedBackend == "cuda" &&
                summary.capabilities.budget.deviceBytes < (8ULL * kOneGiB))
            {
                advisories.push_back({
                    "warning",
                    "budget.low_vram",
                    "CUDA was selected with less than 8 GiB of device memory. Expect degraded "
                    "context windows.",
                });
            }

            if ((summary.selectedBackend == "directml" || summary.selectedBackend == "vulkan") &&
                summary.capabilities.budget.deviceBytes < (6ULL * kOneGiB))
            {
                advisories.push_back({
                    "warning",
                    "budget.low_shared_memory",
                    "The selected cross-vendor backend has a tight device budget. Prefer smaller "
                    "models or shorter context.",
                });
            }

            if (summary.hasNpu && summary.selectedBackend != "windows-ml")
            {
                advisories.push_back({
                    "info",
                    "npu.opt_in",
                    "An NPU is available but remains opt-in. Use the run command with --npu when "
                    "the runtime path exists.",
                });
            }

            return advisories;
        }

    } // namespace

    bool ProbeSummary::HasAccelerator() const
    {
        return selectedBackend != "cpu-avx2" && selectedBackend != "cpu-avx512" &&
               selectedBackend != "cpu-amx";
    }

    bool ProbeSummary::IsDegraded() const
    {
        return mode == "DEGRADED" || mode == "ULTRA_LOW" || mode == "MICRO" || mode == "CPU_ONLY";
    }

    ProbeSummary ProbeHardware()
    {
        using namespace us4::runtime::backends;

        ProbeSummary summary{};
        summary.osName = "Windows x86-64";
        summary.capabilities = HardwareProbe::DetectHost();
        summary.cpuName = summary.capabilities.cpuName;
        summary.gpuName = summary.capabilities.primaryAdapterName;
        summary.hasNpu = summary.capabilities.hasNpu;
        summary.backends =
            us4::runtime::backends::BackendSelector::BuildCatalog(summary.capabilities);

        us4::runtime::backends::SessionRequest request{};
        request.mode = us4::runtime::backends::RuntimeMode::kBalanced;
        request.allowNpu = summary.hasNpu;

        const RuntimePlan plan = RuntimeContext::BuildPlan(request, summary.capabilities);
        summary.selectedBackend = plan.backend.name;
        summary.mode = ToString(plan.mode);
        for (const auto& fallback : plan.fallbacks)
        {
            if (fallback.name != plan.backend.name)
            {
                summary.fallbackBackends.push_back(fallback.name);
            }
        }
        summary.advisories = BuildAdvisories(summary);
        return summary;
    }

    std::string BuildLaunchHint(const ProbeSummary& summary)
    {
        std::ostringstream output;
        output << "us4-cli run --model qwen-0.5b --prompt \"hello\"";
        if (summary.hasNpu)
        {
            output << " --npu";
        }
        return output.str();
    }

    std::string FormatProbeSummary(const ProbeSummary& summary)
    {
        std::ostringstream output;
        output << "US4 Windows Edition Probe\n";
        output << "os: " << summary.osName << '\n';
        output << "cpu: " << summary.cpuName << '\n';
        output << "gpu: " << summary.gpuName << '\n';
        output << "npu: " << (summary.hasNpu ? "present" : "absent") << '\n';
        output << "backend: " << summary.selectedBackend << '\n';
        output << "mode: " << summary.mode << '\n';
        output << "budget.host_bytes: " << summary.capabilities.budget.hostBytes << '\n';
        output << "budget.device_bytes: " << summary.capabilities.budget.deviceBytes << '\n';
        output << "budget.storage_bytes: " << summary.capabilities.budget.storageBytes << '\n';
        output << "backends:\n";

        for (const auto& backend : summary.backends)
        {
            output << "  - " << backend.name
                   << " device_class=" << static_cast<int>(backend.deviceClass)
                   << " precision=" << static_cast<int>(backend.defaultPrecision)
                   << " paged_kv=" << (backend.supportsPagedKv ? "yes" : "no")
                   << " moe=" << (backend.supportsMoE ? "yes" : "no")
                   << " speculative=" << (backend.supportsSpeculative ? "yes" : "no") << '\n';
        }

        if (!summary.fallbackBackends.empty())
        {
            output << "fallbacks:\n";
            for (const std::string& fallback : summary.fallbackBackends)
            {
                output << "  - " << fallback << '\n';
            }
        }

        if (!summary.advisories.empty())
        {
            output << "advisories:\n";
            for (const ProbeAdvisory& advisory : summary.advisories)
            {
                output << "  - [" << advisory.severity << "] " << advisory.code << ": "
                       << advisory.message << '\n';
            }
        }

        output << "next: " << BuildLaunchHint(summary) << '\n';
        return output.str();
    }

} // namespace us4::core
