#include "runtime/core/cpu_scalar_runtime.h"

#include "runtime/core/rope.h"
#include "runtime/core/tensor.h"
#include "us4/runtime/adapters/adapter_factory.h"
#include "us4/runtime/backends/cpu_avx/scalar_attention.h"
#include "us4/runtime/backends/cpu_avx/scalar_matmul.h"
#include "us4/runtime/cache/prefix_cache.h"
#include "us4/runtime/moe/expert_pager.h"
#include "us4/runtime/moe/expert_router.h"
#include "us4/runtime/kv/summarizer.h"
#include "us4/runtime/telemetry/telemetry_sink.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace us4::core
{
    namespace
    {
        constexpr std::size_t kHiddenSize = 8;
        constexpr std::size_t kSummaryHeadTokens = 4;
        constexpr std::size_t kSummaryTailTokens = 4;

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
        return result;
    }

} // namespace us4::core
