#include "us4/runtime/benchmarks/matrix_runner.h"

#include "us4/runtime/benchmarks/benchmark_registry.h"
#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace us4::runtime::benchmarks
{
    namespace
    {
        constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

        double ToGiB(const std::size_t bytes)
        {
            return static_cast<double>(bytes) / kGiB;
        }

        int ProfileRank(const std::string& profileId)
        {
            if (profileId == "full")
            {
                return 6;
            }
            if (profileId == "balanced")
            {
                return 5;
            }
            if (profileId == "degraded")
            {
                return 4;
            }
            if (profileId == "ultra-low")
            {
                return 3;
            }
            if (profileId == "micro")
            {
                return 2;
            }
            if (profileId == "nano")
            {
                return 1;
            }
            if (profileId == "cpu-only")
            {
                return 0;
            }
            return -1;
        }

        std::string ResolveDecisionValue(const tuning::TuningPlan& plan, const std::string& key,
                                         const std::string& fallback = {})
        {
            const auto decision = std::find_if(plan.decisions.begin(), plan.decisions.end(),
                                               [&key](const tuning::TuningDecision& candidate)
                                               { return candidate.key == key; });
            if (decision == plan.decisions.end())
            {
                return fallback;
            }
            return decision->value;
        }

        bool IsExplicitWindowsMlRequest(const backends::SessionRequest& request)
        {
            return request.preferredBackend == "windows-ml" || request.preferredBackend == "npu";
        }

        bool ProbeIsSupported(const BenchmarkCase& benchmark, const tuning::TuningProbe& probe,
                              const backends::SessionRequest& request,
                              const backends::HardwareCapabilities& capabilities)
        {
            if (benchmark.backend == "cpu")
            {
                return true;
            }
            if (benchmark.backend == "cuda")
            {
                return capabilities.hasCuda;
            }
            if (benchmark.backend == "directml")
            {
                return capabilities.hasDirectMl;
            }
            if (benchmark.backend == "vulkan")
            {
                return capabilities.hasVulkan;
            }
            if (benchmark.backend == "windows-ml")
            {
                if (probe.benchmarkName == "windows_ml_qwen_no_npu_fallback")
                {
                    return request.allowNpu && IsExplicitWindowsMlRequest(request) &&
                           !capabilities.hasNpu;
                }
                return request.allowNpu && capabilities.hasNpu;
            }
            return true;
        }

        double ScoreProbe(const BenchmarkCase& benchmark, const tuning::TuningProbe& probe,
                          const backends::SessionRequest& request,
                          const backends::HardwareCapabilities& capabilities,
                          const std::string& requestedProfileId,
                          const std::string& recommendedProfileId)
        {
            double score = 60.0;

            if (benchmark.backend == "cpu")
            {
                score = 82.0 + ToGiB(capabilities.budget.hostBytes) * 0.45;
                if (capabilities.hasAvx512)
                {
                    score += 8.0;
                }
                if (capabilities.hasAmx)
                {
                    score += 8.0;
                }
            }
            else if (benchmark.backend == "cuda")
            {
                score = 138.0 + ToGiB(capabilities.budget.deviceBytes) * 1.35;
            }
            else if (benchmark.backend == "directml")
            {
                score = 110.0 + ToGiB(capabilities.budget.deviceBytes);
                if (capabilities.primaryAdapterClass == backends::DeviceClass::kIntegratedGpu)
                {
                    score += 6.0;
                }
            }
            else if (benchmark.backend == "vulkan")
            {
                score = 118.0 + ToGiB(capabilities.budget.deviceBytes) * 1.15;
                if (capabilities.primaryAdapterClass == backends::DeviceClass::kDiscreteGpu)
                {
                    score += 10.0;
                }
            }
            else if (benchmark.backend == "windows-ml")
            {
                if (probe.benchmarkName == "windows_ml_qwen_no_npu_fallback")
                {
                    score = 112.0 + ToGiB(capabilities.budget.hostBytes) * 0.30;
                }
                else
                {
                    score = 132.0 + static_cast<double>(capabilities.npuCount) * 16.0;
                    if (capabilities.hasVulkan)
                    {
                        score += 6.0;
                    }
                }
            }

            score += static_cast<double>(benchmark.promptTokens) / 64.0;
            score += static_cast<double>(benchmark.generationTokens) / 128.0;
            score += probe.regressionCritical ? 8.0 : 2.0;

            if (!request.preferredBackend.empty() && request.preferredBackend != "auto")
            {
                const bool backendMatch =
                    request.preferredBackend == benchmark.backend ||
                    (request.preferredBackend == "npu" && benchmark.backend == "windows-ml");
                if (backendMatch)
                {
                    score += 12.0;
                }
            }

            if (probe.profileId == requestedProfileId)
            {
                score += 6.0;
            }
            if (probe.profileId == recommendedProfileId)
            {
                score += 10.0;
            }

            score -= std::abs(ProfileRank(probe.profileId) - ProfileRank(recommendedProfileId));
            return score;
        }
    } // namespace

    MatrixRunner::MatrixRunner(std::filesystem::path storePath) : storePath_(std::move(storePath))
    {
    }

    MatrixTuneReport MatrixRunner::Tune(const backends::SessionRequest& request,
                                        const backends::HardwareCapabilities& capabilities) const
    {
        const auto resolvedStorePath =
            storePath_.empty() ? tuning::ProfileStore::DefaultPath() : storePath_;
        const tuning::AutoTuner tuner(resolvedStorePath);
        const auto plan = tuner.BuildPlan(request, capabilities);
        return ExecutePlan(plan, request, capabilities);
    }

    MatrixTuneReport
    MatrixRunner::ExecutePlan(const tuning::TuningPlan& plan,
                              const backends::SessionRequest& request,
                              const backends::HardwareCapabilities& capabilities) const
    {
        const auto resolvedStorePath =
            storePath_.empty() ? tuning::ProfileStore::DefaultPath() : storePath_;
        MatrixTuneReport report{};
        report.plan = plan;
        report.storePath = resolvedStorePath;
        report.requestedProfileId = ResolveDecisionValue(plan, "requested-profile", "balanced");
        report.recommendedProfileId =
            ResolveDecisionValue(plan, "recommended-profile", report.requestedProfileId);
        report.selectedProfileId = report.recommendedProfileId;

        struct AggregateScore
        {
            std::string backend;
            double totalScore = 0.0;
            double peakScore = 0.0;
            std::size_t supportedCount = 0;
            std::size_t regressionCriticalCount = 0;
        };

        std::map<std::string, AggregateScore> aggregates;
        for (const auto& probe : plan.probes)
        {
            MatrixSample sample{
                .benchmarkName = probe.benchmarkName,
                .backend = probe.backend,
                .profileId = probe.profileId,
                .regressionCritical = probe.regressionCritical,
            };

            const auto benchmark = BenchmarkRegistry::FindByName(probe.benchmarkName);
            if (!benchmark.has_value())
            {
                sample.rationale = "Probe is not registered in the benchmark catalog yet.";
                report.samples.push_back(std::move(sample));
                continue;
            }

            sample.supported = ProbeIsSupported(*benchmark, probe, request, capabilities);
            if (!sample.supported)
            {
                sample.rationale =
                    "Hardware capabilities do not satisfy the backend requirements for this probe.";
                report.samples.push_back(std::move(sample));
                continue;
            }

            sample.score = ScoreProbe(*benchmark, probe, request, capabilities,
                                      report.requestedProfileId, report.recommendedProfileId);
            sample.rationale = probe.rationale;
            report.samples.push_back(sample);

            auto& aggregate = aggregates[sample.profileId];
            aggregate.backend = sample.backend;
            aggregate.totalScore += sample.score;
            aggregate.peakScore = std::max(aggregate.peakScore, sample.score);
            aggregate.supportedCount += 1U;
            if (sample.regressionCritical)
            {
                aggregate.regressionCriticalCount += 1U;
            }
        }

        for (const auto& [profileId, aggregate] : aggregates)
        {
            if (aggregate.supportedCount == 0U)
            {
                continue;
            }

            const bool preferCandidate =
                profileId == report.selectedProfileId
                    ? aggregate.totalScore > report.selectedScore
                    : (aggregate.totalScore > report.selectedScore) ||
                          (std::abs(aggregate.totalScore - report.selectedScore) < 0.001 &&
                           profileId == report.recommendedProfileId);
            if (!preferCandidate)
            {
                continue;
            }

            report.selectedProfileId = profileId;
            report.selectedBackend = aggregate.backend;
            report.selectedScore = aggregate.totalScore;
        }

        if (report.selectedBackend.empty())
        {
            report.selectedBackend =
                request.preferredBackend.empty() ? "auto" : request.preferredBackend;
        }

        tuning::ProfileStore store(resolvedStorePath);
        report.persisted = store.SaveProfileId(capabilities, report.selectedProfileId);
        return report;
    }

} // namespace us4::runtime::benchmarks
