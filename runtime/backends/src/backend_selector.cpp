#include "us4/runtime/backends/backend_selector.h"

#include "us4/runtime/backends/cuda/cuda_backend.h"
#include "us4/runtime/backends/directml/dml_device.h"

#include <algorithm>
#include <cctype>

namespace us4::runtime::backends
{

    namespace
    {
        std::string NormalizeBackendName(std::string_view value)
        {
            std::string normalized;
            normalized.reserve(value.size());
            for (const char character : value)
            {
                if (character == '_' || character == ' ')
                {
                    normalized.push_back('-');
                    continue;
                }
                normalized.push_back(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
            return normalized;
        }

        bool MatchesRequestedBackend(const BackendDescriptor& descriptor,
                                     std::string_view preferredBackend)
        {
            const std::string normalized = NormalizeBackendName(preferredBackend);
            if (normalized.empty() || normalized == "auto")
            {
                return true;
            }

            if (normalized == "cpu")
            {
                return descriptor.kind == BackendKind::kCpu;
            }

            if (normalized == "npu")
            {
                return descriptor.kind == BackendKind::kWindowsMl;
            }

            return NormalizeBackendName(descriptor.name) == normalized;
        }

        bool IsExplicitWindowsMlRequest(std::string_view preferredBackend)
        {
            const std::string normalized = NormalizeBackendName(preferredBackend);
            return normalized == "windows-ml" || normalized == "npu";
        }

        constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;

        std::uint32_t ComputeContextHint(const HardwareCapabilities& capabilities,
                                         const BackendKind kind)
        {
            if (kind == BackendKind::kCuda)
            {
                return capabilities.budget.deviceBytes >= 24ULL * kGiB ? 16384U : 8192U;
            }
            if (kind == BackendKind::kDirectML || kind == BackendKind::kVulkan)
            {
                return capabilities.budget.deviceBytes >= 12ULL * kGiB ? 8192U : 4096U;
            }
            if (kind == BackendKind::kWindowsMl)
            {
                return 4096U;
            }
            return capabilities.hasAvx512 ? 4096U : 2048U;
        }

        BackendVendor InferCpuVendor(const HardwareCapabilities& capabilities)
        {
            std::string cpuName = capabilities.cpuName;
            std::transform(cpuName.begin(), cpuName.end(), cpuName.begin(),
                           [](unsigned char character)
                           { return static_cast<char>(std::tolower(character)); });

            if (cpuName.find("intel") != std::string::npos)
            {
                return BackendVendor::kIntel;
            }
            if (cpuName.find("amd") != std::string::npos ||
                cpuName.find("ryzen") != std::string::npos ||
                cpuName.find("epyc") != std::string::npos)
            {
                return BackendVendor::kAmd;
            }
            if (cpuName.find("qualcomm") != std::string::npos ||
                cpuName.find("snapdragon") != std::string::npos)
            {
                return BackendVendor::kQualcomm;
            }
            return BackendVendor::kUnknown;
        }

        BackendDescriptor MakeCpuBackend(const HardwareCapabilities& capabilities)
        {
            BackendDescriptor descriptor;
            descriptor.kind = BackendKind::kCpu;
            descriptor.name = capabilities.hasAmx
                                  ? "cpu-amx"
                                  : (capabilities.hasAvx512 ? "cpu-avx512" : "cpu-avx2");
            descriptor.displayName =
                capabilities.hasAmx ? "CPU (AMX)"
                                    : (capabilities.hasAvx512 ? "CPU (AVX-512)" : "CPU (AVX2)");
            descriptor.deviceClass = DeviceClass::kCpuOnly;
            descriptor.vendor = InferCpuVendor(capabilities);
            descriptor.availability = BackendAvailability::kAvailable;
            descriptor.defaultPrecision =
                capabilities.hasAmx ? PrecisionMode::kBf16 : PrecisionMode::kFp32;
            descriptor.selectionRank = 500;
            descriptor.maxContextTokensHint = ComputeContextHint(capabilities, descriptor.kind);
            descriptor.supportsPagedKv = true;
            descriptor.supportsMoE = true;
            descriptor.supportsUnifiedMemory = true;
            descriptor.budget = capabilities.budget;
            return descriptor;
        }

        BackendDescriptor MakeCudaBackend(const HardwareCapabilities& capabilities)
        {
            return cuda::CudaBackend::BuildDescriptor(capabilities);
        }

        BackendDescriptor MakeDirectMlBackend(const HardwareCapabilities& capabilities)
        {
            return directml::DmlDevice::BuildDescriptor(capabilities);
        }

        BackendDescriptor MakeVulkanBackend(const HardwareCapabilities& capabilities)
        {
            BackendDescriptor descriptor;
            descriptor.kind = BackendKind::kVulkan;
            descriptor.name = "vulkan";
            descriptor.displayName = "Vulkan Compute";
            descriptor.deviceClass = capabilities.primaryAdapterClass == DeviceClass::kDiscreteGpu
                                         ? DeviceClass::kDiscreteGpu
                                         : DeviceClass::kIntegratedGpu;
            descriptor.vendor = capabilities.primaryAdapterVendor;
            descriptor.availability = BackendAvailability::kAvailable;
            descriptor.defaultPrecision = PrecisionMode::kFp16;
            descriptor.selectionRank =
                descriptor.deviceClass == DeviceClass::kIntegratedGpu ? 330 : 300;
            descriptor.maxContextTokensHint = ComputeContextHint(capabilities, descriptor.kind);
            descriptor.supportsPagedKv = true;
            descriptor.supportsSpeculative = true;
            descriptor.supportsUnifiedMemory =
                capabilities.hasUnifiedMemory ||
                descriptor.deviceClass == DeviceClass::kIntegratedGpu;
            descriptor.budget = capabilities.budget;
            return descriptor;
        }

        BackendDescriptor MakeWindowsMlBackend(const HardwareCapabilities& capabilities)
        {
            BackendDescriptor descriptor;
            descriptor.kind = BackendKind::kWindowsMl;
            descriptor.name = "windows-ml";
            descriptor.displayName = "Windows ML";
            descriptor.deviceClass = DeviceClass::kNpu;
            descriptor.vendor = capabilities.npuVendor == BackendVendor::kUnknown
                                    ? BackendVendor::kMicrosoft
                                    : capabilities.npuVendor;
            descriptor.availability = BackendAvailability::kOptIn;
            descriptor.defaultPrecision = PrecisionMode::kInt8;
            descriptor.selectionRank = 250;
            descriptor.maxContextTokensHint = ComputeContextHint(capabilities, descriptor.kind);
            descriptor.supportsNpuOffload = true;
            descriptor.requiresOptIn = true;
            descriptor.budget = capabilities.budget;
            return descriptor;
        }

        bool IsAllowedByRequest(const BackendDescriptor& descriptor, const SessionRequest& request)
        {
            if (!MatchesRequestedBackend(descriptor, request.preferredBackend))
            {
                return false;
            }

            if (request.mode == RuntimeMode::kCpuOnly)
            {
                return descriptor.kind == BackendKind::kCpu;
            }

            if (descriptor.kind == BackendKind::kWindowsMl && !request.allowNpu)
            {
                return IsExplicitWindowsMlRequest(request.preferredBackend);
            }

            if (request.requireDeterministic && descriptor.kind == BackendKind::kVulkan)
            {
                return false;
            }

            if (request.enableSpeculative && !descriptor.supportsSpeculative &&
                descriptor.kind != BackendKind::kCpu)
            {
                return false;
            }

            return true;
        }

        std::uint32_t ComputeRequestAdjustedRank(const BackendDescriptor& descriptor,
                                                 const SessionRequest& request)
        {
            std::uint32_t rank = descriptor.selectionRank;

            if (request.preferIntegratedGpu &&
                descriptor.deviceClass == DeviceClass::kIntegratedGpu)
            {
                rank = rank > 40 ? rank - 40 : 0;
            }

            if (request.preferLowLatency && descriptor.kind == BackendKind::kWindowsMl)
            {
                rank = rank > 120 ? rank - 120 : 0;
            }

            if (request.preferMaxThroughput && descriptor.kind == BackendKind::kCuda)
            {
                rank = rank > 30 ? rank - 30 : 0;
            }

            if (request.precision == PrecisionMode::kInt8 &&
                descriptor.kind == BackendKind::kWindowsMl)
            {
                rank = rank > 60 ? rank - 60 : 0;
            }

            if (request.precision == PrecisionMode::kFp32 && descriptor.kind != BackendKind::kCpu)
            {
                rank += 40;
            }

            return rank;
        }

        void SortCatalog(BackendCatalog* catalog, const SessionRequest* request)
        {
            std::stable_sort(
                catalog->begin(), catalog->end(),
                [request](const BackendDescriptor& left, const BackendDescriptor& right)
                {
                    const std::uint32_t leftRank = request == nullptr
                                                       ? left.selectionRank
                                                       : ComputeRequestAdjustedRank(left, *request);
                    const std::uint32_t rightRank =
                        request == nullptr ? right.selectionRank
                                           : ComputeRequestAdjustedRank(right, *request);
                    return leftRank < rightRank;
                });
        }

    } // namespace

    BackendDescriptor BackendSelector::SelectCpuFallback(const HardwareCapabilities& capabilities)
    {
        return MakeCpuBackend(capabilities);
    }

    BackendCatalog BackendSelector::BuildCatalog(const HardwareCapabilities& capabilities)
    {
        BackendCatalog catalog;

        if (capabilities.hasCuda)
        {
            catalog.push_back(MakeCudaBackend(capabilities));
        }

        if (capabilities.hasDirectMl)
        {
            catalog.push_back(MakeDirectMlBackend(capabilities));
        }

        if (capabilities.hasVulkan)
        {
            catalog.push_back(MakeVulkanBackend(capabilities));
        }

        if (capabilities.hasNpu)
        {
            catalog.push_back(MakeWindowsMlBackend(capabilities));
        }

        catalog.push_back(SelectCpuFallback(capabilities));
        SortCatalog(&catalog, nullptr);
        return catalog;
    }

    std::optional<BackendDescriptor>
    BackendSelector::SelectPrimary(const SessionRequest& request,
                                   const HardwareCapabilities& capabilities)
    {
        BackendCatalog catalog = BuildCatalog(capabilities);
        if (!capabilities.hasNpu && IsExplicitWindowsMlRequest(request.preferredBackend))
        {
            catalog.push_back(MakeWindowsMlBackend(capabilities));
        }
        SortCatalog(&catalog, &request);

        for (const BackendDescriptor& descriptor : catalog)
        {
            if (!IsAllowedByRequest(descriptor, request))
            {
                continue;
            }
            return descriptor;
        }

        return std::nullopt;
    }

    BackendCatalog BackendSelector::SelectFallbacks(const SessionRequest& request,
                                                    const HardwareCapabilities& capabilities)
    {
        BackendCatalog catalog = BuildCatalog(capabilities);
        if (!capabilities.hasNpu && IsExplicitWindowsMlRequest(request.preferredBackend))
        {
            catalog.push_back(MakeWindowsMlBackend(capabilities));
        }
        SortCatalog(&catalog, &request);
        catalog.erase(std::remove_if(catalog.begin(), catalog.end(),
                                     [&request](const BackendDescriptor& descriptor)
                                     { return !IsAllowedByRequest(descriptor, request); }),
                      catalog.end());
        return catalog;
    }

} // namespace us4::runtime::backends
