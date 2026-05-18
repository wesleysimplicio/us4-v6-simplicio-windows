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
        std::size_t prefixCacheEntries = 0;
        std::size_t prefixCacheWarmEntries = 0;
        std::size_t telemetryEventCount = 0;
        std::size_t moeRouteCount = 0;
        std::size_t moeHostRouteCount = 0;
        float moeRouterEntropy = 0.0F;
        std::size_t moeHotExperts = 0;
        std::size_t moeWarmExperts = 0;
        std::size_t moeColdExperts = 0;
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
