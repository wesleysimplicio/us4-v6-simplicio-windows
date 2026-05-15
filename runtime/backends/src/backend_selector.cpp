#include "us4/runtime/backends/backend_selector.h"

#include <algorithm>

namespace us4::runtime::backends
{

    namespace
    {

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
            descriptor.vendor = BackendVendor::kIntel;
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
            BackendDescriptor descriptor;
            descriptor.kind = BackendKind::kCuda;
            descriptor.name = "cuda";
            descriptor.displayName = "CUDA";
            descriptor.deviceClass = DeviceClass::kDiscreteGpu;
            descriptor.vendor = BackendVendor::kNvidia;
            descriptor.availability = BackendAvailability::kAvailable;
            descriptor.defaultPrecision = PrecisionMode::kFp16;
            descriptor.selectionRank = 100;
            descriptor.maxContextTokensHint = ComputeContextHint(capabilities, descriptor.kind);
            descriptor.supportsGraphCapture = true;
            descriptor.supportsPagedKv = true;
            descriptor.supportsMoE = true;
            descriptor.supportsSpeculative = true;
            descriptor.budget = capabilities.budget;
            return descriptor;
        }

        BackendDescriptor MakeDirectMlBackend(const HardwareCapabilities& capabilities)
        {
            BackendDescriptor descriptor;
            descriptor.kind = BackendKind::kDirectML;
            descriptor.name = "directml";
            descriptor.displayName = "DirectML";
            descriptor.deviceClass = capabilities.primaryAdapterClass == DeviceClass::kDiscreteGpu
                                         ? DeviceClass::kDiscreteGpu
                                         : DeviceClass::kIntegratedGpu;
            descriptor.vendor = capabilities.primaryAdapterVendor;
            descriptor.availability = BackendAvailability::kAvailable;
            descriptor.defaultPrecision = PrecisionMode::kFp16;
            descriptor.selectionRank =
                descriptor.deviceClass == DeviceClass::kIntegratedGpu ? 230 : 200;
            descriptor.maxContextTokensHint = ComputeContextHint(capabilities, descriptor.kind);
            descriptor.supportsPagedKv = true;
            descriptor.supportsMoE = true;
            descriptor.supportsUnifiedMemory =
                capabilities.hasUnifiedMemory ||
                descriptor.deviceClass == DeviceClass::kIntegratedGpu;
            descriptor.budget = capabilities.budget;
            return descriptor;
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
            if (request.mode == RuntimeMode::kCpuOnly)
            {
                return descriptor.kind == BackendKind::kCpu;
            }

            if (descriptor.kind == BackendKind::kWindowsMl && !request.allowNpu)
            {
                return false;
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

        catalog.push_back(MakeCpuBackend(capabilities));
        SortCatalog(&catalog, nullptr);
        return catalog;
    }

    std::optional<BackendDescriptor>
    BackendSelector::SelectPrimary(const SessionRequest& request,
                                   const HardwareCapabilities& capabilities)
    {
        BackendCatalog catalog = BuildCatalog(capabilities);
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
        SortCatalog(&catalog, &request);
        catalog.erase(std::remove_if(catalog.begin(), catalog.end(),
                                     [&request](const BackendDescriptor& descriptor)
                                     { return !IsAllowedByRequest(descriptor, request); }),
                      catalog.end());
        return catalog;
    }

} // namespace us4::runtime::backends
