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

    enum class KvCacheTier
    {
        kDevice,
        kHost,
        kStorage,
        kSummary,
    };

    struct KvCacheEntry
    {
        std::string sequenceId;
        std::size_t tokenCount = 0;
        std::size_t residentBytes = 0;
        KvCacheTier tier = KvCacheTier::kHost;
        bool pinned = false;
        std::size_t accessCount = 0;
        std::size_t summaryTokenCount = 0;
    };

    struct KvHookSnapshot
    {
        std::size_t appendCount = 0;
        std::size_t lookupCount = 0;
        std::size_t evictCount = 0;
        std::size_t summarizeCount = 0;
        std::size_t segmentCount = 0;
        std::size_t pinnedSegmentCount = 0;
        std::size_t residentBytes = 0;
        std::size_t deviceBytes = 0;
        std::size_t hostBytes = 0;
        std::size_t storageBytes = 0;
        std::size_t summaryBytes = 0;
        std::size_t deviceHitCount = 0;
        std::size_t hostHitCount = 0;
        std::size_t storageHitCount = 0;
        std::size_t summaryHitCount = 0;
        std::size_t evictionPagerCount = 0;
        std::size_t restoreCount = 0;
        std::size_t pagerSummarizeCount = 0;
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
