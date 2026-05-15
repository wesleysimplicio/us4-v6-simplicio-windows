#pragma once

#include "us4/runtime/backends/backend_descriptor.h"
#include "us4/runtime/backends/runtime_types.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace us4::runtime::adapters
{

    enum class AdapterLifecycle
    {
        kDetached,
        kBound,
        kModelLoaded,
        kServing,
        kFaulted,
    };

    struct AdapterFeatureFlags
    {
        bool supportsContinuousBatching = false;
        bool supportsToolCalling = false;
        bool supportsVision = false;
        bool supportsEmbeddings = false;
        bool supportsStructuredOutput = false;
        bool supportsKvTiering = false;
        bool supportsSpeculativeDecoding = false;
        bool supportsMoE = false;
        bool supportsDeterministicReplay = false;
    };

    struct AdapterDescriptor
    {
        std::string adapterId;
        std::string family;
        std::string revision;
        AdapterFeatureFlags features;
        backends::PrecisionMode preferredPrecision = backends::PrecisionMode::kFp16;
        std::size_t maxContextTokens = 0;
        std::size_t maxBatchSize = 0;
        bool supportsGqa = false;
        bool supportsSlidingWindowAttention = false;
        bool supportsRoPE = true;
    };

    struct ModelResidencyPlan
    {
        std::string modelId;
        std::string backendName;
        std::string profileId;
        std::size_t expectedHostBytes = 0;
        std::size_t expectedDeviceBytes = 0;
        std::size_t kvBytesPerToken = 0;
        std::size_t expectedPrefillScratchBytes = 0;
        std::size_t expectedDecodeScratchBytes = 0;
        bool enableKvTiering = false;
        bool enableSpeculative = false;
    };

    struct GenerationFrame
    {
        std::size_t prefillTokens = 0;
        std::size_t decodeTokens = 0;
        bool reachedEos = false;
    };

    struct AdapterHealth
    {
        AdapterLifecycle lifecycle = AdapterLifecycle::kDetached;
        std::vector<backends::RuntimeIssue> issues;
    };

    struct RuntimeBinding
    {
        backends::BackendDescriptor backend;
        std::string profileId;
        backends::RuntimeMode mode = backends::RuntimeMode::kBalanced;
        std::string modelId;
        bool allowNpu = false;
    };

} // namespace us4::runtime::adapters
