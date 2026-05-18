#include "us4/runtime/backends/hardware_probe.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <dxgi1_6.h>
#include <intrin.h>
#include <windows.h>
#include <wrl/client.h>
#endif

namespace us4::runtime::backends
{

    namespace
    {
        constexpr std::size_t kOneGiB = 1024ULL * 1024ULL * 1024ULL;

        std::optional<std::string> ReadEnv(const char* key)
        {
#if defined(_WIN32)
            char* value = nullptr;
            std::size_t size = 0;
            if (_dupenv_s(&value, &size, key) != 0 || value == nullptr || value[0] == '\0')
            {
                if (value != nullptr)
                {
                    std::free(value);
                }
                return std::nullopt;
            }

            const std::string result(value);
            std::free(value);
            return result;
#else
            const char* value = std::getenv(key);
            if (value == nullptr || value[0] == '\0')
            {
                return std::nullopt;
            }
            return std::string(value);
#endif
        }

        bool ParseBoolString(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](const unsigned char character)
                           { return static_cast<char>(std::tolower(character)); });
            return value == "1" || value == "true" || value == "on" || value == "yes";
        }

        std::string ReadEnvOrDefault(const char* key, std::string_view fallback)
        {
            const auto value = ReadEnv(key);
            if (!value.has_value())
            {
                return std::string(fallback);
            }
            return *value;
        }

        bool ReadBoolEnv(const char* key)
        {
            const auto value = ReadEnv(key);
            return value.has_value() && ParseBoolString(*value);
        }

        std::optional<std::uint32_t> ReadUintEnv(const char* key)
        {
            const auto value = ReadEnv(key);
            if (!value.has_value())
            {
                return std::nullopt;
            }

            char* end = nullptr;
            const unsigned long parsed = std::strtoul(value->c_str(), &end, 10);
            if (end == value->c_str())
            {
                return std::nullopt;
            }

            return static_cast<std::uint32_t>(parsed);
        }

        std::size_t ReadSizeEnvGiB(const char* key, const std::size_t fallbackGiB)
        {
            const auto value = ReadEnv(key);
            if (!value.has_value())
            {
                return fallbackGiB * kOneGiB;
            }

            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(value->c_str(), &end, 10);
            if (end == value->c_str())
            {
                return fallbackGiB * kOneGiB;
            }

            return static_cast<std::size_t>(parsed) * kOneGiB;
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

        BackendVendor VendorFromPciId(const std::uint32_t vendorId)
        {
            switch (vendorId)
            {
            case 0x10DE:
                return BackendVendor::kNvidia;
            case 0x1002:
            case 0x1022:
                return BackendVendor::kAmd;
            case 0x8086:
                return BackendVendor::kIntel;
            case 0x17CB:
                return BackendVendor::kQualcomm;
            case 0x1414:
                return BackendVendor::kMicrosoft;
            default:
                return BackendVendor::kUnknown;
            }
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

        bool ShouldUseSyntheticProbe()
        {
            if (ReadBoolEnv("US4_USE_SYNTHETIC_PROBE"))
            {
                return true;
            }

            constexpr std::array<const char*, 16> kSyntheticScenarioKeys = {
                "US4_HAS_CUDA",   "US4_HAS_DIRECTML", "US4_HAS_VULKAN", "US4_HAS_AVX2",
                "US4_HAS_AVX512", "US4_HAS_AMX",      "US4_HAS_NPU",    "US4_CPU_NAME",
                "US4_GPU_NAME",   "US4_NPU_NAME",     "US4_GPU_VENDOR", "US4_NPU_VENDOR",
                "US4_GPU_CLASS",  "US4_HOST_GIB",     "US4_DEVICE_GIB", "US4_STORAGE_GIB",
            };

            return std::any_of(kSyntheticScenarioKeys.begin(), kSyntheticScenarioKeys.end(),
                               [](const char* key) { return ReadEnv(key).has_value(); });
        }

#if defined(_WIN32)
        std::string NarrowWide(const wchar_t* value)
        {
            if (value == nullptr || value[0] == L'\0')
            {
                return {};
            }

            const int required =
                WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
            if (required <= 1)
            {
                return {};
            }

            std::string output(static_cast<std::size_t>(required - 1), '\0');
            static_cast<void>(WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), required,
                                                  nullptr, nullptr));
            return output;
        }

        struct CpuProbeResult
        {
            bool hasAvx2 = false;
            bool hasAvx512 = false;
            bool hasAmx = false;
            std::string cpuName = "Generic x86-64 CPU";
        };

        CpuProbeResult DetectCpuProbe()
        {
            CpuProbeResult result{};

            std::array<int, 4> registers = {0, 0, 0, 0};
            __cpuidex(registers.data(), 0, 0);
            const int maxLeaf = registers[0];

            if (maxLeaf >= 7)
            {
                __cpuidex(registers.data(), 7, 0);
                result.hasAvx2 = (registers[1] & (1 << 5)) != 0;
                result.hasAvx512 = (registers[1] & (1 << 16)) != 0;
                result.hasAmx = (registers[3] & (1 << 24)) != 0;
            }

            __cpuidex(registers.data(), 0x80000000U, 0);
            const unsigned int maxExtendedLeaf = static_cast<unsigned int>(registers[0]);
            if (maxExtendedLeaf >= 0x80000004U)
            {
                std::array<int, 12> brandRegisters{};
                __cpuidex(&brandRegisters[0], 0x80000002U, 0);
                __cpuidex(&brandRegisters[4], 0x80000003U, 0);
                __cpuidex(&brandRegisters[8], 0x80000004U, 0);

                std::string brand(reinterpret_cast<const char*>(brandRegisters.data()),
                                  brandRegisters.size() * sizeof(int));
                brand.erase(std::find(brand.begin(), brand.end(), '\0'), brand.end());
                const auto firstNonSpace =
                    std::find_if_not(brand.begin(), brand.end(), [](unsigned char character)
                                     { return std::isspace(character) != 0; });
                const auto lastNonSpace =
                    std::find_if_not(brand.rbegin(), brand.rend(), [](unsigned char character)
                                     { return std::isspace(character) != 0; })
                        .base();
                if (firstNonSpace < lastNonSpace)
                {
                    result.cpuName.assign(firstNonSpace, lastNonSpace);
                }
            }

            return result;
        }

        struct AdapterProbeResult
        {
            bool foundGpu = false;
            bool hasUnifiedMemory = false;
            bool prefersIntegratedPath = false;
            std::uint32_t discreteGpuCount = 0;
            std::uint32_t integratedGpuCount = 0;
            std::string primaryAdapterName = "Unknown GPU";
            BackendVendor primaryAdapterVendor = BackendVendor::kUnknown;
            DeviceClass primaryAdapterClass = DeviceClass::kCpuOnly;
            std::size_t deviceBytes = 0;
        };

        AdapterProbeResult DetectDxgiAdapters()
        {
            AdapterProbeResult result{};

            Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
            if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            {
                return result;
            }

            std::size_t bestBudget = 0;

            for (UINT adapterIndex = 0;; ++adapterIndex)
            {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
                if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND)
                {
                    break;
                }

                DXGI_ADAPTER_DESC1 description{};
                if (FAILED(adapter->GetDesc1(&description)))
                {
                    continue;
                }

                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
                {
                    continue;
                }

                const bool integrated = description.DedicatedVideoMemory == 0;
                if (integrated)
                {
                    ++result.integratedGpuCount;
                }
                else
                {
                    ++result.discreteGpuCount;
                }

                const std::size_t adapterBudget =
                    description.DedicatedVideoMemory > 0
                        ? static_cast<std::size_t>(description.DedicatedVideoMemory)
                        : static_cast<std::size_t>(description.SharedSystemMemory);
                if (!result.foundGpu || adapterBudget >= bestBudget)
                {
                    result.foundGpu = true;
                    result.primaryAdapterName = NarrowWide(description.Description);
                    result.primaryAdapterVendor = VendorFromPciId(description.VendorId);
                    result.primaryAdapterClass =
                        integrated ? DeviceClass::kIntegratedGpu : DeviceClass::kDiscreteGpu;
                    result.deviceBytes = adapterBudget;
                    result.hasUnifiedMemory = integrated;
                    bestBudget = adapterBudget;
                }
            }

            result.prefersIntegratedPath =
                result.discreteGpuCount == 0U && result.integratedGpuCount > 0U;
            return result;
        }

        bool HasLibrary(const wchar_t* libraryName)
        {
            HMODULE library = LoadLibraryW(libraryName);
            if (library == nullptr)
            {
                return false;
            }

            FreeLibrary(library);
            return true;
        }

        bool DetectVulkanSupport()
        {
            HMODULE library = LoadLibraryW(L"vulkan-1.dll");
            if (library == nullptr)
            {
                return false;
            }

            const bool available = GetProcAddress(library, "vkGetInstanceProcAddr") != nullptr ||
                                   GetProcAddress(library, "vkEnumerateInstanceVersion") != nullptr;
            FreeLibrary(library);
            return available;
        }

        bool DetectDirectMlSupport(const AdapterProbeResult& adapterProbe)
        {
            return adapterProbe.foundGpu && HasLibrary(L"DirectML.dll");
        }

        bool DetectWindowsMlRuntimeSupport()
        {
            return HasLibrary(L"Windows.AI.MachineLearning.dll") ||
                   HasLibrary(L"Windows.AI.MachineLearning.Native.dll");
        }

        bool DetectCudaSupport()
        {
            HMODULE library = LoadLibraryW(L"nvcuda.dll");
            if (library == nullptr)
            {
                return false;
            }

            using CuInitFn = int(__stdcall*)(unsigned int);
            using CuDeviceGetCountFn = int(__stdcall*)(int*);

            const auto cuInit = reinterpret_cast<CuInitFn>(GetProcAddress(library, "cuInit"));
            const auto cuDeviceGetCount =
                reinterpret_cast<CuDeviceGetCountFn>(GetProcAddress(library, "cuDeviceGetCount"));

            bool available = false;
            if (cuInit != nullptr && cuDeviceGetCount != nullptr && cuInit(0U) == 0)
            {
                int deviceCount = 0;
                available = cuDeviceGetCount(&deviceCount) == 0 && deviceCount > 0;
            }

            FreeLibrary(library);
            return available;
        }
#endif

        HardwareCapabilities BuildSyntheticCapabilities()
        {
            HardwareCapabilities capabilities{};
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
            capabilities.budget.deviceBytes =
                ReadSizeEnvGiB("US4_DEVICE_GIB",
                               capabilities.hasCuda
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
            return capabilities;
        }

        void ApplyEnvOverrides(HardwareCapabilities& capabilities)
        {
            if (const auto value = ReadEnv("US4_HAS_CUDA"); value.has_value())
            {
                capabilities.hasCuda = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_DIRECTML"); value.has_value())
            {
                capabilities.hasDirectMl = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_VULKAN"); value.has_value())
            {
                capabilities.hasVulkan = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_AVX2"); value.has_value())
            {
                capabilities.hasAvx2 = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_AVX512"); value.has_value())
            {
                capabilities.hasAvx512 = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_AMX"); value.has_value())
            {
                capabilities.hasAmx = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_NPU"); value.has_value())
            {
                capabilities.hasNpu = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_HAS_UNIFIED_MEMORY"); value.has_value())
            {
                capabilities.hasUnifiedMemory = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_PREFER_INTEGRATED"); value.has_value())
            {
                capabilities.prefersIntegratedPath = ParseBoolString(*value);
            }
            if (const auto value = ReadEnv("US4_CPU_NAME"); value.has_value())
            {
                capabilities.cpuName = *value;
            }
            if (const auto value = ReadEnv("US4_GPU_NAME"); value.has_value())
            {
                capabilities.primaryAdapterName = *value;
            }
            if (const auto value = ReadEnv("US4_NPU_NAME"); value.has_value())
            {
                capabilities.npuName = *value;
            }
            if (const auto value = ReadEnv("US4_GPU_VENDOR"); value.has_value())
            {
                capabilities.primaryAdapterVendor = ParseVendor(*value);
            }
            if (const auto value = ReadEnv("US4_NPU_VENDOR"); value.has_value())
            {
                capabilities.npuVendor = ParseVendor(*value);
            }
            if (const auto value = ReadEnv("US4_GPU_CLASS"); value.has_value())
            {
                capabilities.primaryAdapterClass = ParseDeviceClass(*value);
            }
            if (const auto value = ReadUintEnv("US4_DISCRETE_GPU_COUNT"); value.has_value())
            {
                capabilities.discreteGpuCount = *value;
            }
            if (const auto value = ReadUintEnv("US4_INTEGRATED_GPU_COUNT"); value.has_value())
            {
                capabilities.integratedGpuCount = *value;
            }
            if (const auto value = ReadUintEnv("US4_NPU_COUNT"); value.has_value())
            {
                capabilities.npuCount = *value;
            }

            capabilities.budget.hostBytes =
                ReadSizeEnvGiB("US4_HOST_GIB", capabilities.budget.hostBytes / kOneGiB);
            capabilities.budget.deviceBytes =
                ReadSizeEnvGiB("US4_DEVICE_GIB", capabilities.budget.deviceBytes / kOneGiB);
            capabilities.budget.storageBytes =
                ReadSizeEnvGiB("US4_STORAGE_GIB", capabilities.budget.storageBytes / kOneGiB);
        }

        HardwareCapabilities BuildDetectedCapabilities()
        {
            HardwareCapabilities capabilities{};

#if defined(_WIN32)
            const CpuProbeResult cpuProbe = DetectCpuProbe();
            const AdapterProbeResult adapterProbe = DetectDxgiAdapters();

            capabilities.hasAvx2 = cpuProbe.hasAvx2;
            capabilities.hasAvx512 = cpuProbe.hasAvx512;
            capabilities.hasAmx = cpuProbe.hasAmx;
            capabilities.cpuName = cpuProbe.cpuName;

            capabilities.hasCuda = DetectCudaSupport();
            capabilities.hasDirectMl = DetectDirectMlSupport(adapterProbe);
            capabilities.hasVulkan = DetectVulkanSupport();
            capabilities.hasUnifiedMemory = adapterProbe.hasUnifiedMemory;
            capabilities.prefersIntegratedPath = adapterProbe.prefersIntegratedPath;
            capabilities.discreteGpuCount = adapterProbe.discreteGpuCount;
            capabilities.integratedGpuCount = adapterProbe.integratedGpuCount;
            capabilities.primaryAdapterName = adapterProbe.primaryAdapterName;
            capabilities.primaryAdapterVendor = adapterProbe.primaryAdapterVendor;
            capabilities.primaryAdapterClass = adapterProbe.primaryAdapterClass;
            capabilities.budget.deviceBytes = adapterProbe.deviceBytes;

            const bool hasWindowsMlRuntime = DetectWindowsMlRuntimeSupport();
            capabilities.hasNpu = false;
            capabilities.npuName = hasWindowsMlRuntime ? "Windows ML runtime available" : "Absent";
            capabilities.npuVendor = BackendVendor::kUnknown;
            capabilities.npuCount = 0;
#else
            capabilities.hasAvx2 = true;
            capabilities.cpuName = "Generic x86-64 CPU";
            capabilities.primaryAdapterName = "Unknown GPU";
            capabilities.npuName = "Absent";
#endif

            capabilities.budget.hostBytes = 32 * kOneGiB;
            capabilities.budget.storageBytes = 256 * kOneGiB;
            return capabilities;
        }

    } // namespace

    HardwareCapabilities HardwareProbe::DetectHost()
    {
        HardwareCapabilities capabilities =
            ShouldUseSyntheticProbe() ? BuildSyntheticCapabilities() : BuildDetectedCapabilities();
        if (!ShouldUseSyntheticProbe())
        {
            ApplyEnvOverrides(capabilities);
        }

        if (capabilities.primaryAdapterVendor == BackendVendor::kUnknown)
        {
            capabilities.primaryAdapterVendor = InferGpuVendor(capabilities);
        }

        if (capabilities.primaryAdapterClass == DeviceClass::kCpuOnly)
        {
            capabilities.primaryAdapterClass = InferPrimaryDeviceClass(capabilities);
        }

        if (capabilities.npuName.empty())
        {
            capabilities.npuName = capabilities.hasNpu ? "Generic NPU" : "Absent";
        }
        if (capabilities.primaryAdapterName.empty())
        {
            capabilities.primaryAdapterName = "Unknown GPU";
        }
        if (capabilities.cpuName.empty())
        {
            capabilities.cpuName = "Generic x86-64 CPU";
        }

        return capabilities;
    }

} // namespace us4::runtime::backends
