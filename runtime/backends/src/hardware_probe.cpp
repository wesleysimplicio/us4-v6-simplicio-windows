#include "us4/runtime/backends/hardware_probe.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace us4::runtime::backends
{

    namespace
    {

        std::string ReadEnvOrDefault(const char* key, std::string_view fallback)
        {
            const char* value = std::getenv(key);
            if (value == nullptr || value[0] == '\0')
            {
                return std::string(fallback);
            }
            return std::string(value);
        }

        bool ReadBoolEnv(const char* key)
        {
            const std::string value = ReadEnvOrDefault(key, "");
            return value == "1" || value == "true" || value == "TRUE" || value == "on" ||
                   value == "ON";
        }

        std::optional<std::uint32_t> ReadUintEnv(const char* key)
        {
            const char* value = std::getenv(key);
            if (value == nullptr || value[0] == '\0')
            {
                return std::nullopt;
            }

            char* end = nullptr;
            const unsigned long parsed = std::strtoul(value, &end, 10);
            if (end == value)
            {
                return std::nullopt;
            }

            return static_cast<std::uint32_t>(parsed);
        }

        std::size_t ReadSizeEnvGiB(const char* key, std::size_t fallbackGiB)
        {
            const char* value = std::getenv(key);
            if (value == nullptr || value[0] == '\0')
            {
                return fallbackGiB * 1024ULL * 1024ULL * 1024ULL;
            }

            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(value, &end, 10);
            if (end == value)
            {
                return fallbackGiB * 1024ULL * 1024ULL * 1024ULL;
            }

            return static_cast<std::size_t>(parsed) * 1024ULL * 1024ULL * 1024ULL;
        }

        BackendVendor ParseVendor(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            if (value == "nvidia")
            {
                return BackendVendor::kNvidia;
            }
            if (value == "amd")
            {
                return BackendVendor::kAmd;
            }
            if (value == "intel")
            {
                return BackendVendor::kIntel;
            }
            if (value == "qualcomm")
            {
                return BackendVendor::kQualcomm;
            }
            if (value == "microsoft")
            {
                return BackendVendor::kMicrosoft;
            }
            return BackendVendor::kUnknown;
        }

        DeviceClass ParseDeviceClass(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            if (value == "discrete" || value == "dgpu")
            {
                return DeviceClass::kDiscreteGpu;
            }
            if (value == "integrated" || value == "igpu")
            {
                return DeviceClass::kIntegratedGpu;
            }
            if (value == "npu")
            {
                return DeviceClass::kNpu;
            }
            return DeviceClass::kCpuOnly;
        }

        BackendVendor InferGpuVendor(const HardwareCapabilities& capabilities)
        {
            if (capabilities.hasCuda)
            {
                return BackendVendor::kNvidia;
            }
            if (capabilities.hasDirectMl || capabilities.hasVulkan)
            {
                return BackendVendor::kUnknown;
            }
            return BackendVendor::kUnknown;
        }

        DeviceClass InferPrimaryDeviceClass(const HardwareCapabilities& capabilities)
        {
            if (capabilities.hasNpu && !capabilities.hasCuda && !capabilities.hasDirectMl &&
                !capabilities.hasVulkan)
            {
                return DeviceClass::kNpu;
            }
            if (capabilities.hasCuda)
            {
                return DeviceClass::kDiscreteGpu;
            }
            if (capabilities.hasDirectMl || capabilities.hasVulkan)
            {
                return capabilities.prefersIntegratedPath ? DeviceClass::kIntegratedGpu
                                                          : DeviceClass::kDiscreteGpu;
            }
            return DeviceClass::kCpuOnly;
        }

    } // namespace

    HardwareCapabilities HardwareProbe::DetectHost()
    {
        HardwareCapabilities capabilities;
        capabilities.hasCuda = ReadBoolEnv("US4_HAS_CUDA");
        capabilities.hasDirectMl = ReadBoolEnv("US4_HAS_DIRECTML");
        capabilities.hasVulkan = ReadBoolEnv("US4_HAS_VULKAN");
        capabilities.hasAvx2 = true;
        capabilities.hasAvx512 = ReadBoolEnv("US4_HAS_AVX512");
        capabilities.hasAmx = ReadBoolEnv("US4_HAS_AMX");
        capabilities.hasNpu = ReadBoolEnv("US4_HAS_NPU");
        capabilities.hasUnifiedMemory = ReadBoolEnv("US4_HAS_UNIFIED_MEMORY");
        capabilities.prefersIntegratedPath = ReadBoolEnv("US4_PREFER_INTEGRATED");
        capabilities.cpuName = ReadEnvOrDefault("US4_CPU_NAME", "Generic x86-64 CPU");
        capabilities.primaryAdapterName = ReadEnvOrDefault("US4_GPU_NAME", "Unknown GPU");
        capabilities.npuName =
            ReadEnvOrDefault("US4_NPU_NAME", capabilities.hasNpu ? "Generic NPU" : "Absent");
        capabilities.primaryAdapterVendor = ParseVendor(ReadEnvOrDefault("US4_GPU_VENDOR", ""));
        capabilities.npuVendor = ParseVendor(ReadEnvOrDefault("US4_NPU_VENDOR", ""));
        capabilities.budget.hostBytes = ReadSizeEnvGiB("US4_HOST_GIB", 32);
        capabilities.budget.deviceBytes = ReadSizeEnvGiB(
            "US4_DEVICE_GIB", capabilities.hasCuda
                                  ? 24
                                  : (capabilities.hasDirectMl || capabilities.hasVulkan ? 12 : 0));
        capabilities.budget.storageBytes = ReadSizeEnvGiB("US4_STORAGE_GIB", 256);
        capabilities.discreteGpuCount =
            ReadUintEnv("US4_DISCRETE_GPU_COUNT").value_or(capabilities.hasCuda ? 1U : 0U);
        capabilities.integratedGpuCount =
            ReadUintEnv("US4_INTEGRATED_GPU_COUNT")
                .value_or(capabilities.hasDirectMl || capabilities.hasVulkan ? 1U : 0U);
        capabilities.npuCount =
            ReadUintEnv("US4_NPU_COUNT").value_or(capabilities.hasNpu ? 1U : 0U);

        if (capabilities.primaryAdapterVendor == BackendVendor::kUnknown)
        {
            capabilities.primaryAdapterVendor = InferGpuVendor(capabilities);
        }

        const DeviceClass envClass = ParseDeviceClass(ReadEnvOrDefault("US4_GPU_CLASS", ""));
        capabilities.primaryAdapterClass =
            envClass == DeviceClass::kCpuOnly ? InferPrimaryDeviceClass(capabilities) : envClass;

        return capabilities;
    }

} // namespace us4::runtime::backends
