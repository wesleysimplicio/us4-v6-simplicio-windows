#pragma once

#include "us4/runtime/backends/backend_descriptor.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::backends::directml
{

    enum class DmlQueuePriority
    {
        kNormal,
        kHigh,
        kGlobalRealtime,
    };

    enum class DmlTensorDataType
    {
        kFp32,
        kFp16,
        kBf16,
        kInt8,
        kUInt8,
    };

    enum class DmlOperatorKind
    {
        kMatMul,
        kAttention,
        kRmsNorm,
        kElementWise,
        kCustom,
    };

    enum class DmlExecutionPhase
    {
        kPrefill,
        kDecode,
        kValidation,
    };

    enum class DmlBindingTier
    {
        kPersistent,
        kTemporary,
        kUpload,
        kReadback,
    };

    enum class DmlGraphState
    {
        kEmpty,
        kRecordable,
        kCompiled,
        kReady,
        kFaulted,
    };

    struct DmlAdapterInfo
    {
        std::string adapterName;
        BackendVendor vendor = BackendVendor::kUnknown;
        DeviceClass deviceClass = DeviceClass::kIntegratedGpu;
        bool isHardwareAccelerated = true;
        bool supportsFp16 = true;
        bool supportsBf16 = false;
        bool supportsInt8 = true;
        bool supportsGraphCompilation = true;
        bool supportsDescriptorReuse = true;
        std::uint32_t queueCount = 1;
        std::size_t dedicatedBytes = 0;
        std::size_t sharedBytes = 0;
    };

    struct DmlDeviceOptions
    {
        bool preferIntegratedGpu = false;
        bool preferLowLatency = false;
        bool allowTensorReuse = true;
        bool enableDebugLayer = false;
        bool enableBackgroundCompilation = true;
        DmlQueuePriority queuePriority = DmlQueuePriority::kNormal;
    };

    struct DmlBufferBinding
    {
        std::string name;
        DmlBindingTier tier = DmlBindingTier::kPersistent;
        std::size_t byteSize = 0;
        std::uint64_t lastFenceValue = 0;
    };

    struct DmlGraphNode
    {
        std::string name;
        DmlOperatorKind kind = DmlOperatorKind::kCustom;
        DmlTensorDataType dataType = DmlTensorDataType::kFp16;
        std::uint32_t inputCount = 0;
        std::uint32_t outputCount = 0;
        bool usesPersistentWeights = true;
        bool allowChunking = false;
        std::size_t temporaryBytes = 0;
        std::size_t persistentBytes = 0;
    };

    struct DmlGraphCompileOptions
    {
        PrecisionMode precision = PrecisionMode::kFp16;
        bool enableOperatorFusion = true;
        bool enablePersistentDescriptors = true;
        bool enableChunkedCompilation = false;
        std::size_t maxTemporaryBytes = 0;
        std::size_t maxPersistentBytes = 0;
    };

    struct DmlDispatchOptions
    {
        DmlExecutionPhase phase = DmlExecutionPhase::kDecode;
        std::uint32_t tokenCount = 1;
        std::uint32_t batchSize = 1;
        std::uint32_t sequenceLength = 1;
        bool allowAsyncCompletion = true;
    };

    struct DmlExecutionStats
    {
        std::uint32_t compileCount = 0;
        std::uint32_t dispatchCount = 0;
        std::uint32_t descriptorHeapReuses = 0;
        std::size_t temporaryBytes = 0;
        std::size_t persistentBytes = 0;
        double lastCompileMilliseconds = 0.0;
        double lastDispatchMilliseconds = 0.0;
        std::uint64_t lastFenceValue = 0;
    };

    struct DmlIssue
    {
        std::string code;
        std::string message;
        bool recoverable = true;
    };

    struct DmlCompileResult
    {
        bool compiled = false;
        bool cacheHit = false;
        bool chunked = false;
        DmlGraphState state = DmlGraphState::kEmpty;
        std::optional<DmlIssue> issue;
    };

    struct DmlDispatchResult
    {
        bool submitted = false;
        bool completedSynchronously = false;
        DmlGraphState state = DmlGraphState::kEmpty;
        std::uint64_t fenceValue = 0;
        std::optional<DmlIssue> issue;
    };

    [[nodiscard]] DmlTensorDataType ToDmlDataType(PrecisionMode precision);
    [[nodiscard]] bool IsPrecisionSupported(const DmlAdapterInfo& adapter,
                                            DmlTensorDataType dataType);
    [[nodiscard]] std::string ToString(DmlQueuePriority priority);
    [[nodiscard]] std::string ToString(DmlTensorDataType dataType);
    [[nodiscard]] std::string ToString(DmlOperatorKind kind);
    [[nodiscard]] std::string ToString(DmlExecutionPhase phase);
    [[nodiscard]] std::string ToString(DmlGraphState state);

} // namespace us4::runtime::backends::directml
