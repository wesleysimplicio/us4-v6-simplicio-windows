#include "runtime/core/hardware_probe.h"

#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/runtime/backends/backend_selector.h"
#include "us4/runtime/backends/hardware_probe.h"
#include "us4/runtime/moe/expert_pager.h"
#include "us4/runtime/moe/expert_router.h"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace us4::core
{
    namespace
    {
        constexpr std::uint64_t kOneGiB = 1024ULL * 1024ULL * 1024ULL;

        std::string FormatPercent(const float ratio)
        {
            std::ostringstream output;
            output << std::fixed << std::setprecision(2) << (ratio * 100.0F);
            return output.str();
        }

        ProbeSummary::MoeTelemetryPreview BuildMoeTelemetryPreview()
        {
            ProbeSummary::MoeTelemetryPreview preview{};

            us4::runtime::moe::ExpertRouter router;
            const auto routes = router.BuildPlan("probe-moe-preview",
                                                 us4::runtime::moe::RoutingPolicy{
                                                     .topK = 6U,
                                                     .preferDeterministic = true,
                                                     .allowCrossDeviceExperts = true,
                                                 });
            if (routes.empty())
            {
                return preview;
            }

            us4::runtime::moe::ExpertPager pager;
            pager.ConfigureBudget(us4::runtime::moe::ExpertPagerBudget{
                .hotBytes = 3072U,
                .warmBytes = 3072U,
                .coldBytes = 9216U,
            });
            for (const auto& route : routes)
            {
                const auto preferredResidency =
                    route.placement == "host" ? us4::runtime::moe::ExpertResidency::kWarm
                                              : us4::runtime::moe::ExpertResidency::kHot;
                static_cast<void>(pager.Touch(route.expertId, 1536U, preferredResidency));
            }

            const auto coldEntry =
                std::find_if(routes.begin(), routes.end(), [&pager](const auto& route)
                             {
                                 const auto entry = pager.Find(route.expertId);
                                 return entry.has_value() && entry->offloaded;
                             });
            if (coldEntry != routes.end())
            {
                static_cast<void>(pager.Reload(coldEntry->expertId,
                                               us4::runtime::moe::ExpertResidency::kHot));
            }

            const auto routingStats = router.Evaluate(routes);
            const auto pagerStats = pager.Stats();
            const std::vector<std::size_t> lookupOrder = {0U, 2U, 4U, 1U, 3U, 5U};
            std::size_t hotHits = 0U;
            std::size_t warmHits = 0U;
            std::size_t coldHits = 0U;

            for (const auto expertId : lookupOrder)
            {
                const auto entry = pager.Find(expertId);
                if (!entry.has_value())
                {
                    continue;
                }

                switch (entry->residency)
                {
                case us4::runtime::moe::ExpertResidency::kHot:
                    ++hotHits;
                    break;
                case us4::runtime::moe::ExpertResidency::kWarm:
                    ++warmHits;
                    break;
                case us4::runtime::moe::ExpertResidency::kCold:
                    ++coldHits;
                    break;
                }
            }

            const float totalHits = static_cast<float>(hotHits + warmHits + coldHits);
            preview.routeCount = routes.size();
            preview.hotHitRate = totalHits == 0.0F ? 0.0F : static_cast<float>(hotHits) / totalHits;
            preview.warmHitRate =
                totalHits == 0.0F ? 0.0F : static_cast<float>(warmHits) / totalHits;
            preview.coldHitRate =
                totalHits == 0.0F ? 0.0F : static_cast<float>(coldHits) / totalHits;
            preview.evictionCount = pagerStats.evictionCount;
            preview.coldOffloadCount = pagerStats.coldOffloadCount;
            preview.reloadCount = pagerStats.reloadCount;
            preview.routerEntropy = routingStats.entropy;

            us4::runtime::telemetry::TelemetrySink telemetry;
            telemetry.Record({
                .name = "moe.hot_hit_rate_pct",
                .value = FormatPercent(preview.hotHitRate),
                .category = "moe",
                .numericValue = static_cast<double>(preview.hotHitRate * 100.0F),
            });
            telemetry.Record({
                .name = "moe.warm_hit_rate_pct",
                .value = FormatPercent(preview.warmHitRate),
                .category = "moe",
                .numericValue = static_cast<double>(preview.warmHitRate * 100.0F),
            });
            telemetry.Record({
                .name = "moe.cold_hit_rate_pct",
                .value = FormatPercent(preview.coldHitRate),
                .category = "moe",
                .numericValue = static_cast<double>(preview.coldHitRate * 100.0F),
            });
            telemetry.Record({
                .name = "moe.eviction_count",
                .value = std::to_string(preview.evictionCount),
                .category = "moe",
                .numericValue = static_cast<double>(preview.evictionCount),
            });
            telemetry.Record({
                .name = "moe.cold_offload_count",
                .value = std::to_string(preview.coldOffloadCount),
                .category = "moe",
                .numericValue = static_cast<double>(preview.coldOffloadCount),
            });
            telemetry.Record({
                .name = "moe.reload_count",
                .value = std::to_string(preview.reloadCount),
                .category = "moe",
                .numericValue = static_cast<double>(preview.reloadCount),
            });
            telemetry.Record({
                .name = "moe.router_entropy",
                .value = std::to_string(preview.routerEntropy),
                .category = "moe",
                .numericValue = static_cast<double>(preview.routerEntropy),
            });
            preview.events = telemetry.Snapshot();
            return preview;
        }

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
        summary.moeTelemetry = BuildMoeTelemetryPreview();
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

        if (summary.moeTelemetry.routeCount > 0U)
        {
            output << "moe.preview:\n";
            output << "  route_count: " << summary.moeTelemetry.routeCount << '\n';
            output << "  hot_hit_rate_pct: " << FormatPercent(summary.moeTelemetry.hotHitRate)
                   << '\n';
            output << "  warm_hit_rate_pct: " << FormatPercent(summary.moeTelemetry.warmHitRate)
                   << '\n';
            output << "  cold_hit_rate_pct: " << FormatPercent(summary.moeTelemetry.coldHitRate)
                   << '\n';
            output << "  eviction_count: " << summary.moeTelemetry.evictionCount << '\n';
            output << "  cold_offload_count: " << summary.moeTelemetry.coldOffloadCount << '\n';
            output << "  reload_count: " << summary.moeTelemetry.reloadCount << '\n';
            output << "  router_entropy: " << summary.moeTelemetry.routerEntropy << '\n';
            output << "  telemetry_events: " << summary.moeTelemetry.events.size() << '\n';
        }

        output << "next: " << BuildLaunchHint(summary) << '\n';
        return output.str();
    }

} // namespace us4::core
