#include "us4/runtime/backends/cuda/cuda_backend.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#if !defined(US4_ENABLE_CUDA)
#define US4_ENABLE_CUDA 0
#endif

namespace us4::runtime::backends::cuda
{

    namespace
    {
        constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;

        std::string ToLower(std::string_view value)
        {
            std::string lowered(value);
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return lowered;
        }

        bool Contains(std::string_view haystack, std::string_view needle)
        {
            return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
        }

        std::uint32_t EstimateSmCount(CudaArchitectureTier architecture,
                                      const std::size_t deviceBytes)
        {
            switch (architecture)
            {
            case CudaArchitectureTier::kHopper:
                return deviceBytes >= 60ULL * kGiB ? 114U : 84U;
            case CudaArchitectureTier::kAda:
                return deviceBytes >= 24ULL * kGiB ? 128U : 80U;
            case CudaArchitectureTier::kAmpere:
                return deviceBytes >= 24ULL * kGiB ? 108U : 68U;
            case CudaArchitectureTier::kTuring:
                return 40U;
            case CudaArchitectureTier::kUnknown:
                break;
            }
            return deviceBytes >= 16ULL * kGiB ? 64U : 32U;
        }

        CudaArchitectureTier DetectArchitecture(std::string_view adapterName)
        {
            if (Contains(adapterName, "h100") || Contains(adapterName, "h200") ||
                Contains(adapterName, "hopper"))
            {
                return CudaArchitectureTier::kHopper;
            }
            if (Contains(adapterName, "rtx 40") || Contains(adapterName, "l40") ||
                Contains(adapterName, "ada"))
            {
                return CudaArchitectureTier::kAda;
            }
            if (Contains(adapterName, "rtx 30") || Contains(adapterName, "a100") ||
                Contains(adapterName, "a10") || Contains(adapterName, "ampere"))
            {
                return CudaArchitectureTier::kAmpere;
            }
            if (Contains(adapterName, "rtx 20") || Contains(adapterName, "t4") ||
                Contains(adapterName, "turing"))
            {
                return CudaArchitectureTier::kTuring;
            }
            return CudaArchitectureTier::kUnknown;
        }

        std::pair<std::uint32_t, std::uint32_t>
        DetectComputeCapability(const CudaArchitectureTier architecture)
        {
            switch (architecture)
            {
            case CudaArchitectureTier::kHopper:
                return {9U, 0U};
            case CudaArchitectureTier::kAda:
                return {8U, 9U};
            case CudaArchitectureTier::kAmpere:
                return {8U, 0U};
            case CudaArchitectureTier::kTuring:
                return {7U, 5U};
            case CudaArchitectureTier::kUnknown:
                break;
            }
            return {0U, 0U};
        }

        std::vector<RuntimeIssue> BuildAvailabilityIssues(const HardwareCapabilities& capabilities)
        {
            std::vector<RuntimeIssue> issues;
            if (!US4_ENABLE_CUDA)
            {
                issues.push_back({"cuda.build_disabled",
                                  "The binary was built without CUDA support enabled.", true});
            }
            if (!capabilities.hasCuda)
            {
                issues.push_back({"cuda.not_detected",
                                  "The hardware probe did not report a CUDA-capable adapter.",
                                  true});
            }
            if (capabilities.primaryAdapterVendor != BackendVendor::kUnknown &&
                capabilities.primaryAdapterVendor != BackendVendor::kNvidia)
            {
                issues.push_back({"cuda.vendor_mismatch",
                                  "The selected primary adapter is not reported as NVIDIA.", true});
            }
            if (capabilities.primaryAdapterClass != DeviceClass::kDiscreteGpu &&
                capabilities.discreteGpuCount == 0)
            {
                issues.push_back({"cuda.discrete_gpu_missing",
                                  "CUDA expects a discrete NVIDIA GPU in this Windows build.",
                                  true});
            }
            return issues;
        }

        CudaExecutionFlavor ChooseFlavor(const SessionRequest& request,
                                         const CudaDeviceProperties& device)
        {
            if (device.supportsGraphs && request.preferMaxThroughput &&
                request.mode != RuntimeMode::kMicro && request.mode != RuntimeMode::kNano)
            {
                return CudaExecutionFlavor::kGraphCaptured;
            }

            if (request.preferLowLatency || request.mode == RuntimeMode::kDegraded ||
                request.mode == RuntimeMode::kUltraLow)
            {
                return CudaExecutionFlavor::kStreamed;
            }

            return CudaExecutionFlavor::kReference;
        }

        CudaResidencyMode ChooseResidency(const SessionRequest& request,
                                          const HardwareCapabilities& capabilities)
        {
            if (request.mode == RuntimeMode::kDegraded || request.mode == RuntimeMode::kUltraLow ||
                request.mode == RuntimeMode::kMicro || request.mode == RuntimeMode::kNano)
            {
                return CudaResidencyMode::kTieredKv;
            }
            if (capabilities.hasUnifiedMemory)
            {
                return CudaResidencyMode::kUnifiedMemory;
            }
            if (request.requireDeterministic)
            {
                return CudaResidencyMode::kDeviceOnly;
            }
            return CudaResidencyMode::kHostAssist;
        }

        std::uint32_t RecommendBatchSize(const SessionRequest& request,
                                         const std::size_t deviceBytes)
        {
            if (request.mode == RuntimeMode::kMicro || request.mode == RuntimeMode::kNano ||
                request.mode == RuntimeMode::kCpuOnly)
            {
                return 1U;
            }
            if (deviceBytes >= 32ULL * kGiB)
            {
                return request.preferMaxThroughput ? 8U : 4U;
            }
            if (deviceBytes >= 16ULL * kGiB)
            {
                return request.preferMaxThroughput ? 4U : 2U;
            }
            return 1U;
        }

        std::uint32_t RecommendPrefillChunk(const SessionRequest& request,
                                            const std::size_t deviceBytes)
        {
            if (request.mode == RuntimeMode::kUltraLow || request.mode == RuntimeMode::kMicro ||
                request.mode == RuntimeMode::kNano)
            {
                return 64U;
            }
            return deviceBytes >= 24ULL * kGiB ? 512U : 256U;
        }

        std::size_t EstimateKvBytes(const SessionRequest& request, const std::size_t deviceBytes)
        {
            const std::size_t perTokenBytes = deviceBytes >= 24ULL * kGiB ? 4096ULL : 2048ULL;
            return static_cast<std::size_t>(request.maxContextTokens) * perTokenBytes;
        }

        std::vector<std::int32_t> BuildReferenceTokens(const CudaExecutionPlan& plan,
                                                       const std::uint32_t count,
                                                       const std::uint32_t salt)
        {
            const std::uint32_t seed = plan.request.seed == 0U ? 17U : plan.request.seed;
            const std::size_t hashedModel = std::hash<std::string>{}(plan.request.modelId);
            std::vector<std::int32_t> tokens;
            tokens.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const std::uint32_t mixed =
                    static_cast<std::uint32_t>((hashedModel & 0xFFFFU) + seed + salt + index * 13U +
                                               plan.recommendedBatchSize * 7U);
                tokens.push_back(static_cast<std::int32_t>(1000U + (mixed % 2048U)));
            }
            return tokens;
        }
    } // namespace

    const char* ToString(const CudaArchitectureTier value)
    {
        switch (value)
        {
        case CudaArchitectureTier::kUnknown:
            return "unknown";
        case CudaArchitectureTier::kTuring:
            return "turing";
        case CudaArchitectureTier::kAmpere:
            return "ampere";
        case CudaArchitectureTier::kAda:
            return "ada";
        case CudaArchitectureTier::kHopper:
            return "hopper";
        }
        return "unknown";
    }

    const char* ToString(const CudaExecutionFlavor value)
    {
        switch (value)
        {
        case CudaExecutionFlavor::kReference:
            return "reference";
        case CudaExecutionFlavor::kStreamed:
            return "streamed";
        case CudaExecutionFlavor::kGraphCaptured:
            return "graph-captured";
        }
        return "reference";
    }

    const char* ToString(const CudaResidencyMode value)
    {
        switch (value)
        {
        case CudaResidencyMode::kDeviceOnly:
            return "device-only";
        case CudaResidencyMode::kUnifiedMemory:
            return "unified-memory";
        case CudaResidencyMode::kTieredKv:
            return "tiered-kv";
        case CudaResidencyMode::kHostAssist:
            return "host-assist";
        }
        return "device-only";
    }

    CudaAvailabilityProbe CudaBackend::ProbeSupport(const HardwareCapabilities& capabilities)
    {
        CudaAvailabilityProbe probe;
        probe.enabledByBuild = US4_ENABLE_CUDA != 0;
        probe.detectedOnHost = capabilities.hasCuda;
        probe.hasDiscreteGpu = capabilities.primaryAdapterClass == DeviceClass::kDiscreteGpu ||
                               capabilities.discreteGpuCount > 0;
        probe.requiresDriverUpgrade =
            probe.detectedOnHost && capabilities.budget.deviceBytes < 4ULL * kGiB;
        probe.issues = BuildAvailabilityIssues(capabilities);

        if (!probe.enabledByBuild)
        {
            probe.availability = BackendAvailability::kStubbed;
        }
        else if (probe.detectedOnHost && probe.hasDiscreteGpu && !probe.requiresDriverUpgrade)
        {
            probe.availability = BackendAvailability::kAvailable;
        }
        else
        {
            probe.availability = BackendAvailability::kUnavailable;
        }

        if (probe.requiresDriverUpgrade)
        {
            probe.issues.push_back({"cuda.low_vram",
                                    "The detected CUDA adapter has less than 4 GiB of device "
                                    "memory; keep the runtime on the reference path.",
                                    true});
        }

        return probe;
    }

    CudaDeviceProperties CudaBackend::DescribeDevice(const HardwareCapabilities& capabilities)
    {
        CudaDeviceProperties properties;
        properties.adapterName = capabilities.primaryAdapterName;
        properties.architecture = DetectArchitecture(capabilities.primaryAdapterName);
        const auto capability = DetectComputeCapability(properties.architecture);
        properties.computeCapabilityMajor = capability.first;
        properties.computeCapabilityMinor = capability.second;
        properties.totalGlobalMemoryBytes = capabilities.budget.deviceBytes;
        properties.streamingMultiprocessors =
            EstimateSmCount(properties.architecture, capabilities.budget.deviceBytes);
        properties.asyncEngineCount = capabilities.budget.deviceBytes >= 24ULL * kGiB ? 3U : 2U;
        properties.copyEngineCount = properties.asyncEngineCount;
        properties.maxSharedMemoryPerBlockKb =
            properties.architecture == CudaArchitectureTier::kHopper ? 228U : 100U;
        properties.supportsGraphs = properties.computeCapabilityMajor >= 8U;
        properties.supportsManagedMemory = capabilities.hasUnifiedMemory;
        properties.supportsPeerAccess = capabilities.discreteGpuCount > 1U;
        return properties;
    }

    BackendDescriptor CudaBackend::BuildDescriptor(const HardwareCapabilities& capabilities)
    {
        const CudaAvailabilityProbe probe = ProbeSupport(capabilities);
        const CudaDeviceProperties device = DescribeDevice(capabilities);

        BackendDescriptor descriptor;
        descriptor.kind = BackendKind::kCuda;
        descriptor.name = "cuda";
        descriptor.displayName = "CUDA";
        descriptor.deviceClass = DeviceClass::kDiscreteGpu;
        descriptor.vendor = BackendVendor::kNvidia;
        descriptor.availability = probe.availability;
        descriptor.defaultPrecision =
            device.computeCapabilityMajor >= 8U ? PrecisionMode::kFp16 : PrecisionMode::kFp32;
        descriptor.selectionRank = device.totalGlobalMemoryBytes >= 16ULL * kGiB ? 80U : 120U;
        descriptor.maxContextTokensHint =
            device.totalGlobalMemoryBytes >= 24ULL * kGiB ? 16384U : 8192U;
        descriptor.supportsGraphCapture = device.supportsGraphs;
        descriptor.supportsPagedKv = true;
        descriptor.supportsMoE = true;
        descriptor.supportsSpeculative = true;
        descriptor.supportsUnifiedMemory = capabilities.hasUnifiedMemory;
        descriptor.budget = capabilities.budget;
        return descriptor;
    }

    CudaExecutionPlan CudaBackend::BuildExecutionPlan(const SessionRequest& request,
                                                      const HardwareCapabilities& capabilities)
    {
        CudaExecutionPlan plan;
        plan.request = request;
        plan.descriptor = BuildDescriptor(capabilities);
        plan.device = DescribeDevice(capabilities);
        plan.flavor = ChooseFlavor(request, plan.device);
        plan.streams.prefillStreams =
            request.preferMaxThroughput && capabilities.budget.deviceBytes >= 24ULL * kGiB ? 2U
                                                                                           : 1U;
        plan.streams.decodeStreams = request.preferLowLatency ? 1U : plan.streams.prefillStreams;
        plan.streams.useDedicatedTransferStream =
            plan.device.asyncEngineCount >= 2U && request.mode != RuntimeMode::kNano;
        plan.streams.usePriorityStreams = request.preferLowLatency;
        plan.graph.enableGraphCapture = plan.flavor == CudaExecutionFlavor::kGraphCaptured;
        plan.graph.preferPersistentGraph = request.preferMaxThroughput;
        plan.graph.warmupIterations = plan.graph.enableGraphCapture ? 2U : 0U;
        plan.memory.residency = ChooseResidency(request, capabilities);
        plan.memory.weightsDeviceBytes =
            capabilities.budget.deviceBytes >= 24ULL * kGiB ? 10ULL * kGiB : 4ULL * kGiB;
        plan.memory.kvDeviceBytes = EstimateKvBytes(request, capabilities.budget.deviceBytes);
        plan.memory.hostStagingBytes = request.mode == RuntimeMode::kNano
                                           ? 64ULL * 1024ULL * 1024ULL
                                           : 256ULL * 1024ULL * 1024ULL;
        plan.memory.scratchBytes =
            static_cast<std::size_t>(request.maxGenerationTokens) * 1024ULL * 1024ULL;
        plan.memory.enablePoolAllocator = plan.device.supportsGraphs;
        plan.memory.enableAsyncPrefetch =
            plan.memory.residency == CudaResidencyMode::kUnifiedMemory;
        plan.recommendedBatchSize = RecommendBatchSize(request, capabilities.budget.deviceBytes);
        plan.recommendedPrefillChunkTokens =
            RecommendPrefillChunk(request, capabilities.budget.deviceBytes);
        plan.status = plan.descriptor.availability == BackendAvailability::kAvailable
                          ? RuntimeStatus::kReady
                          : RuntimeStatus::kDegraded;
        plan.issues = ProbeSupport(capabilities).issues;
        plan.issues.push_back({"cuda.reference_path",
                               "This sprint-level CUDA backend keeps execution on a deterministic "
                               "reference path until kernels are wired into the shared runtime.",
                               true});
        if (plan.graph.enableGraphCapture)
        {
            plan.issues.push_back({"cuda.graph_warmup",
                                   "Graph capture is planned but still requires the integration "
                                   "layer to provide stable tensor addresses.",
                                   true});
        }
        return plan;
    }

    CudaStepResult CudaBackend::PreparePrefill(const CudaExecutionPlan& plan,
                                               const std::string_view prompt)
    {
        CudaStepResult result;
        result.status = plan.status;
        result.stage = "prefill";
        result.usedGraphCapture = plan.graph.enableGraphCapture;
        result.activeStreams = plan.streams.prefillStreams;
        result.deviceBytesTouched =
            std::min(plan.memory.weightsDeviceBytes + plan.memory.kvDeviceBytes / 8U,
                     plan.descriptor.budget.deviceBytes);
        const std::uint32_t tokenCount =
            std::max<std::uint32_t>(1U, static_cast<std::uint32_t>(prompt.size() / 4U));
        result.chunk.tokens = BuildReferenceTokens(plan, tokenCount, 31U);
        result.chunk.isTerminal = false;
        result.issues = plan.issues;
        return result;
    }

    CudaStepResult CudaBackend::PrepareDecode(const CudaExecutionPlan& plan,
                                              const TokenChunk& prefillChunk,
                                              const std::uint32_t stepIndex)
    {
        CudaStepResult result;
        result.status = RuntimeStatus::kGenerating;
        result.stage = "decode";
        result.usedGraphCapture = plan.graph.enableGraphCapture;
        result.activeStreams = plan.streams.decodeStreams;
        result.deviceBytesTouched =
            std::min(plan.memory.kvDeviceBytes, plan.descriptor.budget.deviceBytes);
        const std::uint32_t decodeCount = std::min<std::uint32_t>(
            plan.request.maxGenerationTokens == 0U ? 1U : plan.request.maxGenerationTokens, 8U);
        result.chunk.tokens = BuildReferenceTokens(
            plan, decodeCount,
            static_cast<std::uint32_t>(prefillChunk.tokens.size() * 17U + stepIndex));
        result.chunk.isTerminal = stepIndex + decodeCount >= plan.request.maxGenerationTokens ||
                                  plan.request.maxGenerationTokens <= prefillChunk.tokens.size();
        result.issues = plan.issues;
        return result;
    }

} // namespace us4::runtime::backends::cuda
