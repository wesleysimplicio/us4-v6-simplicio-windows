#pragma once

#include "runtime/core/runtime_context.h"
#include "us4/runtime/adapters/i_us4_windows_adapter.h"
#include "us4/runtime/adapters/model_loader.h"
#include "us4/runtime/kv/kv_pager.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace us4::core
{

    struct SpeculativeStepTelemetry
    {
        std::size_t draftTokens = 0;
        std::size_t acceptedTokens = 0;
        std::size_t rejectedTokens = 0;
        float acceptanceRate = 0.0F;
        float deltaFromPreviousStep = 0.0F;
    };

    struct SpeculativeTelemetryReport
    {
        bool active = false;
        std::string decoder;
        bool draftModelLoaded = false;
        std::size_t draftModelParameterCount = 0;
        std::size_t draftedTokens = 0;
        std::size_t acceptedTokens = 0;
        std::size_t rejectedTokens = 0;
        float acceptanceRate = 0.0F;
        float lastStepDelta = 0.0F;
        float estimatedSpeedup = 1.0F;
        std::vector<int> tokenAcceptanceTrace;
        std::vector<SpeculativeStepTelemetry> steps;
    };

    struct MoePrefetchTelemetryReport
    {
        std::size_t observationCount = 0;
        std::size_t predictionCount = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
        float hitRatio = 0.0F;
        std::vector<std::size_t> predictedExperts;
    };

    struct MoeSparsityTelemetryReport
    {
        std::size_t entryCount = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
        std::size_t evictedEntries = 0;
        std::size_t residentBytes = 0;
        float averageSparsity = 0.0F;
        float hitRatio = 0.0F;
    };

    struct MultimodalCacheTelemetryReport
    {
        std::size_t entryCount = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
        std::size_t residentBytes = 0;
        std::size_t imageEntries = 0;
        std::size_t audioEntries = 0;
        std::size_t videoEntries = 0;
    };

    struct KvTierTelemetryReport
    {
        std::size_t deviceHits = 0;
        std::size_t hostHits = 0;
        std::size_t storageHits = 0;
        std::size_t summaryHits = 0;
        float deviceHitRate = 0.0F;
        float hostHitRate = 0.0F;
        float storageHitRate = 0.0F;
        float summaryHitRate = 0.0F;
    };

    struct CpuScalarRunReport
    {
        std::string modelPath;
        us4::runtime::adapters::ModelAssetFormat modelFormat =
            us4::runtime::adapters::ModelAssetFormat::kUnknown;
        std::vector<std::int32_t> prefillTokens;
        std::vector<std::int32_t> generatedTokens;
        us4::runtime::adapters::GenerationFrame frame;
        double scalarMatMulChecksum = 0.0;
        double scalarAttentionChecksum = 0.0;
        std::string generatedText;
        us4::runtime::kv::KvPagerStats kvStats;
        KvTierTelemetryReport kvTierTelemetry;
        std::size_t prefixCacheEntries = 0;
        std::size_t prefixCacheWarmEntries = 0;
        std::size_t telemetryEventCount = 0;
        std::size_t moeRouteCount = 0;
        std::size_t moeHostRouteCount = 0;
        float moeRouterEntropy = 0.0F;
        float moeLoadBalanceLoss = 0.0F;
        std::size_t moeHotExperts = 0;
        std::size_t moeWarmExperts = 0;
        std::size_t moeColdExperts = 0;
        std::size_t moeMappedExpertCount = 0;
        std::vector<std::size_t> moeMappedExpertIds;
        MoePrefetchTelemetryReport moePrefetchTelemetry;
        MoeSparsityTelemetryReport moeSparsityTelemetry;
        MultimodalCacheTelemetryReport multimodalCacheTelemetry;
        SpeculativeTelemetryReport speculativeTelemetry;
    };

    struct CpuScalarRunResult
    {
        bool ok = false;
        CpuScalarRunReport report;
        std::string error;
    };

    [[nodiscard]] CpuScalarRunResult ExecuteCpuScalarRun(const RuntimePlan& plan,
                                                         std::string_view prompt,
                                                         std::string_view modelPath = {});

} // namespace us4::core
