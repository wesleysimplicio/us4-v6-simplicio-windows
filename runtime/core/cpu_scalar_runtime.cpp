#include "runtime/core/cpu_scalar_runtime.h"

#include "runtime/core/rope.h"
#include "runtime/core/tensor.h"
#include "us4/runtime/adapters/adapter_factory.h"
#include "us4/runtime/backends/cpu_avx/scalar_attention.h"
#include "us4/runtime/backends/cpu_avx/scalar_matmul.h"
#include "us4/runtime/cache/prefix_cache.h"
#include "us4/runtime/cache/sparsity_aware_cache.h"
#include "us4/runtime/moe/expert_pager.h"
#include "us4/runtime/moe/expert_router.h"
#include "us4/runtime/moe/speculative_prefetch.h"
#include "us4/runtime/kv/summarizer.h"
#include "us4/runtime/speculative/draft_model_loader.h"
#include "us4/runtime/speculative/eagle3_decoder.h"
#include "us4/runtime/speculative/peagle_decoder.h"
#include "us4/runtime/telemetry/telemetry_sink.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace us4::core
{
    namespace
    {
        constexpr std::size_t kHiddenSize = 8;
        constexpr std::size_t kSummaryHeadTokens = 4;
        constexpr std::size_t kSummaryTailTokens = 4;
        constexpr std::size_t kSpeculativeWindowSize = 2;

        us4::core::Tensor MakeTokenTensor(const std::vector<std::int32_t>& tokens, const float bias)
        {
            const std::size_t rows = std::max<std::size_t>(1U, tokens.size());
            us4::core::Tensor tensor({rows, kHiddenSize});
            tensor.Fill(0.0F);

            for (std::size_t row = 0; row < rows; ++row)
            {
                const float tokenValue =
                    static_cast<float>(tokens.empty() ? 1 : tokens[row % tokens.size()]);
                for (std::size_t col = 0; col < kHiddenSize; ++col)
                {
                    tensor.At({row, col}) = bias + tokenValue * 0.001F +
                                            static_cast<float>((row + 1U) * (col + 1U)) * 0.01F;
                }
            }

            return tensor;
        }

        us4::core::Tensor MakeProjectionMatrix()
        {
            us4::core::Tensor weights({kHiddenSize, kHiddenSize});
            for (std::size_t row = 0; row < kHiddenSize; ++row)
            {
                for (std::size_t col = 0; col < kHiddenSize; ++col)
                {
                    weights.At({row, col}) = static_cast<float>((row + 1U) + (col + 1U)) * 0.03125F;
                }
            }
            return weights;
        }

        double Checksum(const us4::core::Tensor& tensor)
        {
            double sum = 0.0;
            for (std::size_t index = 0; index < tensor.ElementCount(); ++index)
            {
                sum += tensor[index];
            }
            return sum;
        }

        std::string RenderGeneratedText(const std::vector<std::int32_t>& tokens)
        {
            std::ostringstream output;
            for (std::size_t index = 0; index < tokens.size(); ++index)
            {
                if (index > 0)
                {
                    output << ' ';
                }
                output << "tok" << tokens[index];
            }
            return output.str();
        }

        us4::runtime::kv::KvTier PreferredTierForPlan(const RuntimePlan& plan)
        {
            using us4::runtime::backends::BackendKind;
            using us4::runtime::backends::RuntimeMode;
            using us4::runtime::kv::KvTier;

            if (plan.backend.kind != BackendKind::kCpu)
            {
                return KvTier::kDevice;
            }
            if (plan.mode == RuntimeMode::kMicro || plan.mode == RuntimeMode::kNano)
            {
                return KvTier::kSummary;
            }
            return KvTier::kHost;
        }

        us4::runtime::kv::KvPagerBudget BuildKvBudget(const RuntimePlan& plan)
        {
            const std::size_t hostBudget =
                std::max<std::size_t>(plan.residency.expectedHostBytes, 32ULL * 1024ULL);
            const std::size_t deviceBudget =
                std::max<std::size_t>(plan.residency.expectedDeviceBytes, 16ULL * 1024ULL);
            const std::size_t storageBudget =
                std::max<std::size_t>(hostBudget * 2U, 64ULL * 1024ULL);
            const std::size_t summaryBudget =
                std::max<std::size_t>(plan.residency.kvBytesPerToken * 16U, 4ULL * 1024ULL);

            return us4::runtime::kv::KvPagerBudget{
                .deviceBytes = deviceBudget,
                .hostBytes = hostBudget,
                .storageBytes = storageBudget,
                .summaryBytes = summaryBudget,
            };
        }

        std::string FormatPercentValue(const float rate)
        {
            std::ostringstream output;
            output << std::fixed << std::setprecision(2) << (rate * 100.0F);
            return output.str();
        }

        us4::runtime::backends::TokenChunk SliceTokenChunk(
            const std::vector<std::int32_t>& tokens, const std::size_t offset, const std::size_t count)
        {
            us4::runtime::backends::TokenChunk chunk;
            if (offset >= tokens.size() || count == 0U)
            {
                return chunk;
            }

            const auto begin = tokens.begin() + static_cast<std::ptrdiff_t>(offset);
            const auto end = begin + static_cast<std::ptrdiff_t>(
                                       std::min<std::size_t>(count, tokens.size() - offset));
            chunk.tokens.assign(begin, end);
            return chunk;
        }

        us4::runtime::backends::TokenChunk BuildDraftChunk(
            const us4::runtime::backends::TokenChunk& target, const std::size_t stepIndex)
        {
            auto draft = target;
            if (draft.tokens.empty())
            {
                return draft;
            }

            if (stepIndex % 3U == 1U)
            {
                draft.tokens.back() += 17;
            }
            else if (stepIndex % 3U == 2U)
            {
                const std::size_t mismatchCount = std::min<std::size_t>(2U, draft.tokens.size());
                for (std::size_t index = 0; index < mismatchCount; ++index)
                {
                    draft.tokens[draft.tokens.size() - 1U - index] +=
                        static_cast<std::int32_t>(11 + index);
                }
            }

            return draft;
        }

        SpeculativeTelemetryReport SimulateSpeculativeTelemetry(
            const RuntimePlan& plan, const std::vector<std::int32_t>& generatedTokens,
            us4::runtime::telemetry::TelemetrySink& telemetry)
        {
            SpeculativeTelemetryReport report{};
            if (!plan.model.supportsSpeculative || generatedTokens.empty())
            {
                return report;
            }

            report.active = true;

            us4::runtime::speculative::DraftModelLoader loader;
            const us4::runtime::speculative::DraftModelDescriptor draftDescriptor{
                .modelId = plan.model.id + "-draft",
                .path = {},
                .parameterCount = std::max<std::size_t>(generatedTokens.size(), 1U) * 1000000U,
                .hiddenSize = 2048U,
                .numLayers = 8U,
                .vocabSize = 32000U,
                .sharedTokenizer = true,
            };
            const auto handle = loader.Load(draftDescriptor);
            report.draftModelLoaded = handle.has_value();
            report.draftModelParameterCount =
                handle.has_value() ? handle->parameterCount : 0U;

            telemetry.Record({
                .name = "speculative.draft_model_loaded",
                .value = report.draftModelLoaded ? "1" : "0",
                .category = "speculative",
                .numericValue = report.draftModelLoaded ? 1.0 : 0.0,
            });

            const bool useEagle3 =
                plan.model.family == ModelFamily::kLlama || plan.model.supportsMoE;
            float previousStepRate = 0.0F;
            std::size_t tokenEventIndex = 0U;

            if (useEagle3)
            {
                us4::runtime::speculative::Eagle3Decoder decoder;
                decoder.Configure({
                    .treeDepth = 5U,
                    .treeWidth = 4U,
                    .acceptanceThreshold = 0.7F,
                    .useTreeAttention = true,
                });
                report.decoder = "eagle3";

                for (std::size_t offset = 0U, stepIndex = 0U; offset < generatedTokens.size();
                     offset += kSpeculativeWindowSize, ++stepIndex)
                {
                    const auto target =
                        SliceTokenChunk(generatedTokens, offset, kSpeculativeWindowSize);
                    const auto draft = BuildDraftChunk(target, stepIndex);
                    const auto window = decoder.Decode(draft, target);
                    const float delta = window.acceptanceRate - previousStepRate;
                    previousStepRate = window.acceptanceRate;

                    report.draftedTokens += window.draftTokens;
                    report.acceptedTokens += window.acceptedTokens;
                    report.rejectedTokens += window.rejectedTokens;
                    report.lastStepDelta = delta;
                    report.steps.push_back({
                        .draftTokens = window.draftTokens,
                        .acceptedTokens = window.acceptedTokens,
                        .rejectedTokens = window.rejectedTokens,
                        .acceptanceRate = window.acceptanceRate,
                        .deltaFromPreviousStep = delta,
                    });

                    const std::string stepNumber = std::to_string(stepIndex + 1U);
                    telemetry.Record({
                        .name = std::string("speculative.step_") + stepNumber +
                                ".acceptance_rate_pct",
                        .value = FormatPercentValue(window.acceptanceRate),
                        .category = "speculative",
                        .numericValue = static_cast<double>(window.acceptanceRate * 100.0F),
                    });
                    telemetry.Record({
                        .name = std::string("speculative.step_") + stepNumber + ".delta_pct",
                        .value = FormatPercentValue(delta),
                        .category = "speculative",
                        .numericValue = static_cast<double>(delta * 100.0F),
                    });

                    for (std::size_t tokenIndex = 0U; tokenIndex < window.draftTokens;
                         ++tokenIndex)
                    {
                        const int accepted = tokenIndex < window.acceptedTokens ? 1 : 0;
                        report.tokenAcceptanceTrace.push_back(accepted);
                        telemetry.Record({
                            .name = std::string("speculative.token_") +
                                    std::to_string(++tokenEventIndex) + ".accepted",
                            .value = accepted == 1 ? "1" : "0",
                            .category = "speculative",
                            .numericValue = static_cast<double>(accepted),
                        });
                    }
                }

                const auto stats = decoder.Stats();
                report.acceptanceRate = stats.averageAcceptanceRate;
                report.estimatedSpeedup = stats.estimatedSpeedup;
            }
            else
            {
                us4::runtime::speculative::PEagleDecoder decoder;
                decoder.Configure({
                    .draftDepth = 4U,
                    .acceptanceThreshold = 0.6F,
                    .useLayerPruning = true,
                });
                report.decoder = "peagle";

                for (std::size_t offset = 0U, stepIndex = 0U; offset < generatedTokens.size();
                     offset += kSpeculativeWindowSize, ++stepIndex)
                {
                    const auto target =
                        SliceTokenChunk(generatedTokens, offset, kSpeculativeWindowSize);
                    const auto draft = BuildDraftChunk(target, stepIndex);
                    const auto window = decoder.Decode(draft, target);
                    const float delta = window.acceptanceRate - previousStepRate;
                    previousStepRate = window.acceptanceRate;

                    report.draftedTokens += window.draftTokens;
                    report.acceptedTokens += window.acceptedTokens;
                    report.rejectedTokens += window.rejectedTokens;
                    report.lastStepDelta = delta;
                    report.steps.push_back({
                        .draftTokens = window.draftTokens,
                        .acceptedTokens = window.acceptedTokens,
                        .rejectedTokens = window.rejectedTokens,
                        .acceptanceRate = window.acceptanceRate,
                        .deltaFromPreviousStep = delta,
                    });

                    const std::string stepNumber = std::to_string(stepIndex + 1U);
                    telemetry.Record({
                        .name = std::string("speculative.step_") + stepNumber +
                                ".acceptance_rate_pct",
                        .value = FormatPercentValue(window.acceptanceRate),
                        .category = "speculative",
                        .numericValue = static_cast<double>(window.acceptanceRate * 100.0F),
                    });
                    telemetry.Record({
                        .name = std::string("speculative.step_") + stepNumber + ".delta_pct",
                        .value = FormatPercentValue(delta),
                        .category = "speculative",
                        .numericValue = static_cast<double>(delta * 100.0F),
                    });

                    for (std::size_t tokenIndex = 0U; tokenIndex < window.draftTokens;
                         ++tokenIndex)
                    {
                        const int accepted = tokenIndex < window.acceptedTokens ? 1 : 0;
                        report.tokenAcceptanceTrace.push_back(accepted);
                        telemetry.Record({
                            .name = std::string("speculative.token_") +
                                    std::to_string(++tokenEventIndex) + ".accepted",
                            .value = accepted == 1 ? "1" : "0",
                            .category = "speculative",
                            .numericValue = static_cast<double>(accepted),
                        });
                    }
                }

                const auto stats = decoder.Stats();
                report.acceptanceRate = stats.averageAcceptanceRate;
                report.estimatedSpeedup =
                    1.0F + stats.averageAcceptanceRate * static_cast<float>(4U - 1U);
            }

            telemetry.Record({
                .name = "speculative.acceptance_rate_pct",
                .value = FormatPercentValue(report.acceptanceRate),
                .category = "speculative",
                .numericValue = static_cast<double>(report.acceptanceRate * 100.0F),
            });
            telemetry.Record({
                .name = "speculative.accepted_tokens",
                .value = std::to_string(report.acceptedTokens),
                .category = "speculative",
                .numericValue = static_cast<double>(report.acceptedTokens),
            });
            telemetry.Record({
                .name = "speculative.rejected_tokens",
                .value = std::to_string(report.rejectedTokens),
                .category = "speculative",
                .numericValue = static_cast<double>(report.rejectedTokens),
            });

            if (handle.has_value())
            {
                static_cast<void>(loader.Unload(handle->modelId));
            }

            return report;
        }

        void SimulateMoeTelemetry(const std::vector<us4::runtime::moe::ExpertRoute>& routes,
                                  CpuScalarRunReport& report,
                                  us4::runtime::telemetry::TelemetrySink& telemetry)
        {
            if (routes.empty())
            {
                return;
            }

            us4::runtime::moe::SpeculativePrefetch prefetch;
            prefetch.Configure(std::max<std::size_t>(routes.size(), 2U), 2U);
            for (const auto& route : routes)
            {
                prefetch.Observe(route.expertId);
            }
            prefetch.Observe(routes.front().expertId);

            const auto predictions = prefetch.PredictNext();
            report.moePrefetchTelemetry.predictedExperts = predictions;
            for (const auto predictedExpert : predictions)
            {
                prefetch.RecordPrefetchOutcome(predictedExpert, predictedExpert == routes.front().expertId);
            }
            const auto prefetchStats = prefetch.Stats();
            report.moePrefetchTelemetry.observationCount = prefetchStats.observationCount;
            report.moePrefetchTelemetry.predictionCount = prefetchStats.predictionCount;
            report.moePrefetchTelemetry.hitCount = prefetchStats.prefetchHitCount;
            report.moePrefetchTelemetry.missCount = prefetchStats.prefetchMissCount;
            report.moePrefetchTelemetry.hitRatio = prefetchStats.hitRatio;

            telemetry.Record({
                .name = "moe.prefetch_hit_ratio_pct",
                .value = FormatPercentValue(prefetchStats.hitRatio),
                .category = "moe",
                .numericValue = static_cast<double>(prefetchStats.hitRatio * 100.0F),
            });
            telemetry.Record({
                .name = "moe.prefetch_prediction_count",
                .value = std::to_string(prefetchStats.predictionCount),
                .category = "moe",
                .numericValue = static_cast<double>(prefetchStats.predictionCount),
            });

            us4::runtime::cache::SparsityAwareCache sparsityCache;
            sparsityCache.Configure(1024U, 0.5F);
            for (const auto& route : routes)
            {
                us4::runtime::cache::SparsityCacheEntry entry{};
                entry.key = "expert-" + std::to_string(route.expertId);
                entry.activeChannels = route.placement == "host" ? 192U : 64U;
                entry.totalChannels = 256U;
                entry.residentBytes = 2048U + route.expertId * 256U;
                sparsityCache.Upsert(std::move(entry));
            }
            report.moeSparsityTelemetry.evictedEntries = sparsityCache.EvictBelowThreshold();
            for (const auto& route : routes)
            {
                static_cast<void>(sparsityCache.TryGet("expert-" + std::to_string(route.expertId)));
            }
            const auto sparsityStats = sparsityCache.Stats();
            const auto totalSparsityLookups = sparsityStats.hitCount + sparsityStats.missCount;
            report.moeSparsityTelemetry.entryCount = sparsityStats.entryCount;
            report.moeSparsityTelemetry.hitCount = sparsityStats.hitCount;
            report.moeSparsityTelemetry.missCount = sparsityStats.missCount;
            report.moeSparsityTelemetry.residentBytes = sparsityStats.residentBytes;
            report.moeSparsityTelemetry.averageSparsity = sparsityStats.averageSparsity;
            report.moeSparsityTelemetry.hitRatio =
                totalSparsityLookups == 0U
                    ? 0.0F
                    : static_cast<float>(sparsityStats.hitCount) /
                          static_cast<float>(totalSparsityLookups);

            telemetry.Record({
                .name = "moe.sparsity_hit_ratio_pct",
                .value = FormatPercentValue(report.moeSparsityTelemetry.hitRatio),
                .category = "moe",
                .numericValue = static_cast<double>(report.moeSparsityTelemetry.hitRatio * 100.0F),
            });
            telemetry.Record({
                .name = "moe.sparsity_cache_entries",
                .value = std::to_string(sparsityStats.entryCount),
                .category = "moe",
                .numericValue = static_cast<double>(sparsityStats.entryCount),
            });
        }
    } // namespace

    CpuScalarRunResult ExecuteCpuScalarRun(const RuntimePlan& plan, std::string_view prompt,
                                           std::string_view modelPath)
    {
        CpuScalarRunResult result{};
        if (plan.backend.kind != us4::runtime::backends::BackendKind::kCpu &&
            plan.mode != us4::runtime::backends::RuntimeMode::kCpuOnly)
        {
            result.error = "CPU scalar execution is only available for CPU_ONLY runtime plans.";
            return result;
        }

        auto adapter = us4::runtime::adapters::CreateAdapterForModelId(plan.model.id);
        if (!adapter)
        {
            result.error = "Could not resolve a scalar adapter for the selected model.";
            return result;
        }

        if (!adapter->Attach(plan.binding))
        {
            result.error = "Scalar adapter rejected the runtime binding.";
            return result;
        }

        const std::string resolvedModelPath =
            modelPath.empty() ? "builtin://" + plan.model.id : std::string(modelPath);
        if (!adapter->LoadModel(resolvedModelPath))
        {
            result.error = "Scalar adapter could not load the requested model asset.";
            return result;
        }

        auto request = plan.request;
        request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
        request.maxGenerationTokens = std::max<std::uint32_t>(request.maxGenerationTokens, 5U);

        const auto prefill = adapter->Prefill(std::string(prompt));
        const auto generated = adapter->Generate(request);
        const auto frame = adapter->LastGenerationFrame();
        const std::vector<std::int32_t> combinedTokens = [&]
        {
            std::vector<std::int32_t> tokens = prefill.tokens;
            tokens.insert(tokens.end(), generated.tokens.begin(), generated.tokens.end());
            return tokens;
        }();

        const auto baseQuery = MakeTokenTensor(prefill.tokens, 0.25F);
        const auto query = plan.model.family == ModelFamily::kLlama
                               ? ApplyRoPE(baseQuery, prefill.tokens.size(),
                                           RopeConfig{
                                               .scalingFactor = 1.0F,
                                               .scalingMode = RopeScalingMode::kLinear,
                                           })
                               : baseQuery;
        const auto projection = MakeProjectionMatrix();
        const auto projected = us4::runtime::backends::cpu_avx::ScalarMatMul(query, projection);
        const auto cachedKey = MakeTokenTensor({11, 17}, 0.125F);
        const auto cachedValue = MakeTokenTensor({23, 29}, 0.5F);
        const auto attention = us4::runtime::backends::cpu_avx::ScalarAttention(
            query, query, projected,
            us4::runtime::backends::cpu_avx::AttentionOptions{
                .scale = 1.0F / std::sqrt(static_cast<float>(kHiddenSize)),
                .causalMask = true,
                .cachedKey = &cachedKey,
                .cachedValue = &cachedValue,
            });

        us4::runtime::cache::PrefixCache prefixCache;
        const std::string sequenceId = "cpu-scalar:" + plan.model.id;
        const std::string prefixKey =
            us4::runtime::cache::PrefixCache::BuildKey(prompt, prefill.tokens.size());
        prefixCache.Upsert(us4::runtime::cache::PrefixCacheEntry{
            .key = prefixKey,
            .sequenceId = sequenceId,
            .promptTokens = prefill.tokens.size(),
            .kvBytes = plan.residency.kvBytesPerToken * std::max<std::size_t>(prefill.tokens.size(), 1U),
        });
        const auto prefixHit = prefixCache.TryGet(prefixKey);

        us4::runtime::kv::KvPager pager;
        pager.ConfigureBudget(BuildKvBudget(plan));
        const us4::runtime::kv::KvTier preferredTier = PreferredTierForPlan(plan);
        static_cast<void>(pager.Append(
            sequenceId, prefill.tokens.size(),
            plan.residency.kvBytesPerToken * std::max<std::size_t>(prefill.tokens.size(), 1U),
            preferredTier, plan.mode != us4::runtime::backends::RuntimeMode::kNano));
        static_cast<void>(pager.Touch(sequenceId));
        static_cast<void>(pager.Append(
            sequenceId, generated.tokens.size(),
            plan.residency.kvBytesPerToken * std::max<std::size_t>(generated.tokens.size(), 1U),
            preferredTier));

        if (plan.mode == us4::runtime::backends::RuntimeMode::kMicro ||
            plan.mode == us4::runtime::backends::RuntimeMode::kNano)
        {
            const auto summary = us4::runtime::kv::Summarizer::Compact(
                combinedTokens, us4::runtime::kv::SummaryWindow{
                                    .headTokens = kSummaryHeadTokens,
                                    .tailTokens = kSummaryTailTokens,
                                });
            static_cast<void>(
                pager.Summarize(sequenceId, summary.retainedTokens.size(), summary.estimatedBytes));
        }
        static_cast<void>(pager.Rebalance());

        const us4::runtime::kv::KvPagerStats kvStats = pager.Stats();

        us4::runtime::telemetry::TelemetrySink telemetry;
        telemetry.Record({
            .name = "kv.segment_count",
            .value = std::to_string(kvStats.segmentCount),
            .category = "kv",
            .numericValue = static_cast<double>(kvStats.segmentCount),
        });
        telemetry.Record({
            .name = "prefix_cache.hit",
            .value = prefixHit.has_value() ? "1" : "0",
            .category = "cache",
            .numericValue = prefixHit.has_value() ? 1.0 : 0.0,
        });
        telemetry.Record({
            .name = "kv.host_bytes",
            .value = std::to_string(kvStats.hostBytes),
            .category = "kv",
            .numericValue = static_cast<double>(kvStats.hostBytes),
        });

        const auto speculativeTelemetry =
            SimulateSpeculativeTelemetry(plan, generated.tokens, telemetry);

        us4::runtime::moe::RoutingStats moeStats{};
        us4::runtime::moe::ExpertPagerStats expertPagerStats{};
        if (plan.model.supportsMoE)
        {
            us4::runtime::moe::ExpertRouter router;
            const auto routes = router.BuildPlan(std::string(prompt),
                                                 us4::runtime::moe::RoutingPolicy{
                                                     .topK = 2U,
                                                     .preferDeterministic = true,
                                                     .allowCrossDeviceExperts = true,
                                                 });
            moeStats = router.Evaluate(routes);

            us4::runtime::moe::ExpertPager expertPager;
            expertPager.ConfigureBudget(us4::runtime::moe::ExpertPagerBudget{
                .hotBytes = std::max<std::size_t>(plan.residency.expectedDeviceBytes / 4U, 1024U),
                .warmBytes = std::max<std::size_t>(plan.residency.expectedHostBytes / 8U, 2048U),
                .coldBytes = std::max<std::size_t>(plan.residency.expectedHostBytes / 4U, 4096U),
            });
            for (const auto& route : routes)
            {
                const auto preferredResidency = route.placement == "host"
                                                    ? us4::runtime::moe::ExpertResidency::kWarm
                                                    : us4::runtime::moe::ExpertResidency::kHot;
                static_cast<void>(expertPager.Touch(route.expertId, 1024U, preferredResidency));
            }
            expertPagerStats = expertPager.Stats();

            telemetry.Record({
                .name = "moe.route_count",
                .value = std::to_string(routes.size()),
                .category = "moe",
                .numericValue = static_cast<double>(routes.size()),
            });
            telemetry.Record({
                .name = "moe.router_entropy",
                .value = std::to_string(moeStats.entropy),
                .category = "moe",
                .numericValue = static_cast<double>(moeStats.entropy),
            });
            SimulateMoeTelemetry(routes, result.report, telemetry);
        }

        result.ok = true;
        result.report.modelPath = resolvedModelPath;
        result.report.modelFormat =
            resolvedModelPath.rfind("builtin://", 0) == 0
                ? us4::runtime::adapters::ModelAssetFormat::kSynthetic
                : us4::runtime::adapters::LoadModelAsset(resolvedModelPath, plan.model.id)
                      .descriptor.format;
        result.report.prefillTokens = prefill.tokens;
        result.report.generatedTokens = generated.tokens;
        result.report.frame = frame;
        result.report.scalarMatMulChecksum = Checksum(projected);
        result.report.scalarAttentionChecksum = Checksum(attention);
        result.report.generatedText = RenderGeneratedText(generated.tokens);
        result.report.kvStats = kvStats;
        result.report.prefixCacheEntries = prefixCache.Size();
        result.report.prefixCacheWarmEntries = prefixCache.WarmEntryCount();
        result.report.telemetryEventCount = telemetry.Snapshot().size();
        result.report.moeRouteCount = moeStats.deviceRouteCount + moeStats.hostRouteCount;
        result.report.moeHostRouteCount = moeStats.hostRouteCount;
        result.report.moeRouterEntropy = moeStats.entropy;
        result.report.moeHotExperts = expertPagerStats.hotExperts;
        result.report.moeWarmExperts = expertPagerStats.warmExperts;
        result.report.moeColdExperts = expertPagerStats.coldExperts;
        result.report.speculativeTelemetry = speculativeTelemetry;
        return result;
    }

} // namespace us4::core
