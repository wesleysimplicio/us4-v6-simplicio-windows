#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace us4::runtime::backends
{

    enum class RuntimeMode
    {
        kFull,
        kBalanced,
        kDegraded,
        kUltraLow,
        kMicro,
        kNano,
        kCpuOnly,
    };

    enum class BackendKind
    {
        kCuda,
        kDirectML,
        kVulkan,
        kCpu,
        kWindowsMl,
    };

    enum class BackendVendor
    {
        kUnknown,
        kNvidia,
        kAmd,
        kIntel,
        kQualcomm,
        kMicrosoft,
    };

    enum class DeviceClass
    {
        kDiscreteGpu,
        kIntegratedGpu,
        kCpuOnly,
        kNpu,
    };

    enum class BackendAvailability
    {
        kUnknown,
        kAvailable,
        kUnavailable,
        kOptIn,
        kStubbed,
    };

    enum class PrecisionMode
    {
        kFp32,
        kFp16,
        kBf16,
        kInt8,
    };

    enum class RuntimeStatus
    {
        kIdle,
        kLoading,
        kReady,
        kGenerating,
        kDegraded,
        kError,
    };

    struct MemoryBudget
    {
        std::size_t hostBytes = 0;
        std::size_t deviceBytes = 0;
        std::size_t storageBytes = 0;
    };

    struct HardwareCapabilities
    {
        bool hasCuda = false;
        bool hasDirectMl = false;
        bool hasVulkan = false;
        bool hasAvx2 = false;
        bool hasAvx512 = false;
        bool hasAmx = false;
        bool hasNpu = false;
        bool hasUnifiedMemory = false;
        bool prefersIntegratedPath = false;
        std::uint32_t discreteGpuCount = 0;
        std::uint32_t integratedGpuCount = 0;
        std::uint32_t npuCount = 0;
        std::string cpuName;
        std::string primaryAdapterName;
        std::string npuName;
        BackendVendor primaryAdapterVendor = BackendVendor::kUnknown;
        BackendVendor npuVendor = BackendVendor::kUnknown;
        DeviceClass primaryAdapterClass = DeviceClass::kCpuOnly;
        MemoryBudget budget;
    };

    struct SessionRequest
    {
        std::string modelId;
        RuntimeMode mode = RuntimeMode::kBalanced;
        PrecisionMode precision = PrecisionMode::kFp16;
        std::uint32_t maxContextTokens = 4096;
        std::uint32_t maxGenerationTokens = 512;
        std::uint32_t seed = 0;
        bool enableSpeculative = false;
        bool allowNpu = false;
        bool preferLowLatency = false;
        bool preferMaxThroughput = true;
        bool preferIntegratedGpu = false;
        bool requireDeterministic = false;
    };

    struct TokenChunk
    {
        std::vector<std::int32_t> tokens;
        bool isTerminal = false;
    };

    struct RuntimeIssue
    {
        std::string code;
        std::string message;
        bool recoverable = true;
    };

} // namespace us4::runtime::backends
