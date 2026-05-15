#include "us4/runtime/tuning/auto_tuner.h"

#include "us4/profiles/profile_catalog.h"
#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>

namespace us4::runtime::tuning
{
    namespace
    {
        std::string ResolveRequestedProfileId(const backends::SessionRequest& request)
        {
            using backends::RuntimeMode;

            switch (request.mode)
            {
            case RuntimeMode::kFull:
                return "full";
            case RuntimeMode::kBalanced:
                return "balanced";
            case RuntimeMode::kDegraded:
                return "degraded";
            case RuntimeMode::kUltraLow:
                return "ultra-low";
            case RuntimeMode::kMicro:
                return "micro";
            case RuntimeMode::kNano:
                return "nano";
            case RuntimeMode::kCpuOnly:
                return "cpu-only";
            }

            return "balanced";
        }

        bool IsExplicitWindowsMlRequest(const backends::SessionRequest& request)
        {
            return request.preferredBackend == "windows-ml" || request.preferredBackend == "npu";
        }

        void AddProbeIfMissing(std::vector<TuningProbe>* probes, TuningProbe probe)
        {
            const auto exists =
                std::any_of(probes->begin(), probes->end(), [&probe](const TuningProbe& candidate)
                            { return candidate.benchmarkName == probe.benchmarkName; });
            if (!exists)
            {
                probes->push_back(std::move(probe));
            }
        }
    } // namespace

    AutoTuner::AutoTuner(std::filesystem::path storePath) : storePath_(std::move(storePath)) {}

    TuningPlan AutoTuner::BuildPlan(const backends::SessionRequest& request,
                                    const backends::HardwareCapabilities& capabilities) const
    {
        TuningPlan plan{};
        const ProfileStore profileStore(storePath_.empty() ? ProfileStore::DefaultPath()
                                                           : storePath_);
        const auto cachedProfileId = profileStore.LoadProfileId(capabilities);
        const std::string recommendedProfileId =
            cachedProfileId.has_value() ? *cachedProfileId
                                        : us4::profiles::ProfileCatalog::RecommendId(capabilities);
        const std::string requestedProfileId = ResolveRequestedProfileId(request);
        const bool explicitWindowsMlRequest = IsExplicitWindowsMlRequest(request);
        const bool noNpuFallbackRegression =
            request.allowNpu && explicitWindowsMlRequest && !capabilities.hasNpu;

        plan.decisions.push_back(TuningDecision{
            .key = "mode",
            .value = request.mode == backends::RuntimeMode::kCpuOnly ? "cpu-only" : "hybrid",
            .rationale = request.mode == backends::RuntimeMode::kCpuOnly
                             ? "Request explicitly asked for CPU-only execution."
                             : "Hybrid mode keeps accelerator paths eligible when available.",
        });
        plan.decisions.push_back(TuningDecision{
            .key = "requested-profile",
            .value = requestedProfileId,
            .rationale = "Requested runtime mode anchors the first tuning profile.",
        });
        plan.decisions.push_back(TuningDecision{
            .key = "recommended-profile",
            .value = recommendedProfileId,
            .rationale =
                cachedProfileId.has_value()
                    ? "Persisted profile cache overrides the catalog recommendation for this "
                      "hardware fingerprint."
                    : "Hardware-driven recommendation seeds the mini-bench plan.",
        });
        plan.decisions.push_back(TuningDecision{
            .key = "profile-store",
            .value = cachedProfileId.has_value() ? "hit" : "miss",
            .rationale = cachedProfileId.has_value()
                             ? "A persisted tuning profile already exists for this hardware "
                               "fingerprint."
                             : "No persisted tuning profile was found for this hardware "
                               "fingerprint.",
        });
        plan.decisions.push_back(TuningDecision{
            .key = "speculative",
            .value = request.enableSpeculative ? "enabled" : "disabled",
            .rationale =
                request.enableSpeculative
                    ? "Speculative decoding remains eligible for latency-focused probes."
                    : "Speculative decoding stays off for the simplest deterministic path.",
        });
        plan.decisions.push_back(TuningDecision{
            .key = "npu",
            .value = (request.allowNpu && capabilities.hasNpu) ? "enabled" : "disabled",
            .rationale = (request.allowNpu && capabilities.hasNpu)
                             ? "NPU is available and the request opted in."
                             : "NPU is unavailable or was not requested.",
        });
        plan.decisions.push_back(TuningDecision{
            .key = "npu-fallback-regression",
            .value = noNpuFallbackRegression ? "enabled" : "disabled",
            .rationale = noNpuFallbackRegression
                             ? "An explicit Windows ML request without NPU availability must keep "
                               "the cpu-fallback session path green."
                             : "No explicit Windows ML fallback regression is required for this "
                               "host/request pair.",
        });

        if (request.mode == backends::RuntimeMode::kCpuOnly ||
            (!capabilities.hasCuda && !capabilities.hasDirectMl && !capabilities.hasVulkan &&
             !capabilities.hasNpu))
        {
            AddProbeIfMissing(&plan.probes,
                              TuningProbe{
                                  .benchmarkName = "dense_baseline_qwen_cpu_only",
                                  .backend = "cpu",
                                  .profileId = "cpu-only",
                                  .promptTokens = 128U,
                                  .generationTokens = 32U,
                                  .regressionCritical = true,
                                  .rationale =
                                      "CPU baseline anchors the tuning plan when no accelerator "
                                      "path is selected.",
                              });
        }

        if (capabilities.hasVulkan && request.mode != backends::RuntimeMode::kCpuOnly)
        {
            AddProbeIfMissing(&plan.probes,
                              TuningProbe{
                                  .benchmarkName = "vulkan_qwen_balanced",
                                  .backend = "vulkan",
                                  .profileId = "balanced",
                                  .promptTokens = 256U,
                                  .generationTokens = 32U,
                                  .regressionCritical = false,
                                  .rationale =
                                      "Cross-vendor Vulkan remains the broadest GPU probe for the "
                                      "hybrid tuning path.",
                              });
        }

        if (request.allowNpu && capabilities.hasNpu)
        {
            AddProbeIfMissing(&plan.probes,
                              TuningProbe{
                                  .benchmarkName = "windows_ml_qwen_opt_in",
                                  .backend = "windows-ml",
                                  .profileId = "micro",
                                  .promptTokens = 256U,
                                  .generationTokens = 32U,
                                  .regressionCritical = true,
                                  .rationale =
                                      "Windows ML opt-in should be sampled when the host NPU is "
                                      "available.",
                              });
        }

        if (noNpuFallbackRegression)
        {
            AddProbeIfMissing(&plan.probes,
                              TuningProbe{
                                  .benchmarkName = "windows_ml_qwen_no_npu_fallback",
                                  .backend = "windows-ml",
                                  .profileId = "micro",
                                  .promptTokens = 256U,
                                  .generationTokens = 32U,
                                  .regressionCritical = true,
                                  .rationale =
                                      "Explicit Windows ML requests must preserve compiled "
                                      "cpu-fallback behavior when no NPU is present.",
                              });
        }

        plan.decisions.push_back(TuningDecision{
            .key = "mini-bench-count",
            .value = std::to_string(plan.probes.size()),
            .rationale = "The first tuning slice should stay small and hardware-specific.",
        });

        return plan;
    }

} // namespace us4::runtime::tuning
