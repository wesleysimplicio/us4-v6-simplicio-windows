#include "runtime/core/runtime_mode.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace us4::core
{
    namespace
    {
        constexpr std::uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;
        constexpr std::uint64_t kLowHostMemoryThresholdBytes = 8ULL * kGiB;

        std::string Normalize(std::string_view value)
        {
            std::string normalized(value);
            std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                           [](unsigned char character)
                           {
                               if (character == '-' || character == ' ')
                               {
                                   return '_';
                               }
                               return static_cast<char>(std::toupper(character));
                           });
            return normalized;
        }

        bool HasLowHostMemory(const us4::runtime::backends::HardwareCapabilities& capabilities)
        {
            return capabilities.budget.hostBytes > 0 &&
                   capabilities.budget.hostBytes <= kLowHostMemoryThresholdBytes;
        }

    } // namespace

    std::string ToString(us4::runtime::backends::RuntimeMode mode)
    {
        using us4::runtime::backends::RuntimeMode;

        switch (mode)
        {
        case RuntimeMode::kFull:
            return "FULL";
        case RuntimeMode::kBalanced:
            return "BALANCED";
        case RuntimeMode::kDegraded:
            return "DEGRADED";
        case RuntimeMode::kUltraLow:
            return "ULTRA_LOW";
        case RuntimeMode::kMicro:
            return "MICRO";
        case RuntimeMode::kNano:
            return "NANO";
        case RuntimeMode::kCpuOnly:
            return "CPU_ONLY";
        }

        return "BALANCED";
    }

    std::optional<us4::runtime::backends::RuntimeMode> ParseRuntimeMode(std::string_view value)
    {
        using us4::runtime::backends::RuntimeMode;

        const std::string normalized = Normalize(value);
        if (normalized.empty() || normalized == "AUTO")
        {
            return std::nullopt;
        }
        if (normalized == "FULL")
        {
            return RuntimeMode::kFull;
        }
        if (normalized == "BALANCED")
        {
            return RuntimeMode::kBalanced;
        }
        if (normalized == "DEGRADED")
        {
            return RuntimeMode::kDegraded;
        }
        if (normalized == "ULTRA_LOW")
        {
            return RuntimeMode::kUltraLow;
        }
        if (normalized == "MICRO")
        {
            return RuntimeMode::kMicro;
        }
        if (normalized == "NANO")
        {
            return RuntimeMode::kNano;
        }
        if (normalized == "CPU_ONLY")
        {
            return RuntimeMode::kCpuOnly;
        }

        return std::nullopt;
    }

    us4::runtime::backends::RuntimeMode
    SelectRuntimeMode(const us4::runtime::backends::BackendDescriptor& backend,
                      const us4::runtime::backends::HardwareCapabilities& capabilities,
                      const us4::runtime::backends::SessionRequest& request)
    {
        using us4::runtime::backends::BackendKind;
        using us4::runtime::backends::RuntimeMode;

        if (request.mode == RuntimeMode::kCpuOnly)
        {
            return RuntimeMode::kCpuOnly;
        }

        if (HasLowHostMemory(capabilities))
        {
            return backend.kind == BackendKind::kCpu ? RuntimeMode::kNano : RuntimeMode::kMicro;
        }

        if (backend.kind == BackendKind::kWindowsMl)
        {
            return RuntimeMode::kMicro;
        }

        if (backend.kind == BackendKind::kCpu)
        {
            if (capabilities.hasAmx || capabilities.hasAvx512)
            {
                return RuntimeMode::kDegraded;
            }
            return RuntimeMode::kCpuOnly;
        }

        if (backend.kind == BackendKind::kCuda)
        {
            if (request.preferLowLatency)
            {
                return RuntimeMode::kBalanced;
            }
            if (capabilities.budget.deviceBytes >= 24ULL * 1024ULL * 1024ULL * 1024ULL)
            {
                return RuntimeMode::kFull;
            }
            if (capabilities.budget.deviceBytes >= 12ULL * 1024ULL * 1024ULL * 1024ULL)
            {
                return RuntimeMode::kBalanced;
            }
            return RuntimeMode::kDegraded;
        }

        if (backend.kind == BackendKind::kDirectML || backend.kind == BackendKind::kVulkan)
        {
            if (capabilities.budget.deviceBytes >= 8ULL * 1024ULL * 1024ULL * 1024ULL)
            {
                return RuntimeMode::kUltraLow;
            }
            return RuntimeMode::kMicro;
        }

        return RuntimeMode::kBalanced;
    }

} // namespace us4::core
