#pragma once

#include "us4/runtime/backends/backend_descriptor.h"
#include "us4/runtime/backends/runtime_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace us4::runtime::backends::cuda
{

    enum class CudaArchitectureTier
    {
        kUnknown,
        kTuring,
        kAmpere,
        kAda,
        kHopper,
    };

    enum class CudaExecutionFlavor
    {
        kReference,
        kStreamed,
        kGraphCaptured,
    };

    enum class CudaResidencyMode
    {
        kDeviceOnly,
        kUnifiedMemory,
        kTieredKv,
        kHostAssist,
    };

    struct CudaDeviceProperties
    {
        std::string adapterName;
        CudaArchitectureTier architecture = CudaArchitectureTier::kUnknown;
        std::uint32_t computeCapabilityMajor = 0;
        std::uint32_t computeCapabilityMinor = 0;
        std::uint32_t streamingMultiprocessors = 0;
        std::uint32_t asyncEngineCount = 0;
        std::uint32_t copyEngineCount = 0;
        std::uint32_t maxSharedMemoryPerBlockKb = 0;
        std::size_t totalGlobalMemoryBytes = 0;
        bool supportsGraphs = false;
        bool supportsManagedMemory = false;
        bool supportsPeerAccess = false;
    };

    struct CudaAvailabilityProbe
    {
        BackendAvailability availability = BackendAvailability::kUnavailable;
        bool enabledByBuild = false;
        bool detectedOnHost = false;
        bool hasDiscreteGpu = false;
        bool requiresDriverUpgrade = false;
        std::vector<RuntimeIssue> issues;
    };

    struct CudaStreamPlan
    {
        std::uint32_t prefillStreams = 1;
        std::uint32_t decodeStreams = 1;
        bool useDedicatedTransferStream = false;
        bool usePriorityStreams = false;
    };

    struct CudaGraphPlan
    {
        bool enableGraphCapture = false;
        bool preferPersistentGraph = false;
        std::uint32_t warmupIterations = 0;
    };

    struct CudaMemoryPlan
    {
        CudaResidencyMode residency = CudaResidencyMode::kDeviceOnly;
        std::size_t weightsDeviceBytes = 0;
        std::size_t kvDeviceBytes = 0;
        std::size_t hostStagingBytes = 0;
        std::size_t scratchBytes = 0;
        bool enablePoolAllocator = false;
        bool enableAsyncPrefetch = false;
    };

    struct CudaExecutionPlan
    {
        BackendDescriptor descriptor;
        SessionRequest request;
        CudaDeviceProperties device;
        CudaExecutionFlavor flavor = CudaExecutionFlavor::kReference;
        CudaStreamPlan streams;
        CudaGraphPlan graph;
        CudaMemoryPlan memory;
        RuntimeStatus status = RuntimeStatus::kIdle;
        std::uint32_t recommendedBatchSize = 1;
        std::uint32_t recommendedPrefillChunkTokens = 128;
        std::vector<RuntimeIssue> issues;
    };

    struct CudaStepResult
    {
        RuntimeStatus status = RuntimeStatus::kIdle;
        std::string stage;
        TokenChunk chunk;
        std::size_t deviceBytesTouched = 0;
        bool usedGraphCapture = false;
        std::uint32_t activeStreams = 0;
        std::vector<RuntimeIssue> issues;
    };

    [[nodiscard]] const char* ToString(CudaArchitectureTier value);
    [[nodiscard]] const char* ToString(CudaExecutionFlavor value);
    [[nodiscard]] const char* ToString(CudaResidencyMode value);

} // namespace us4::runtime::backends::cuda
