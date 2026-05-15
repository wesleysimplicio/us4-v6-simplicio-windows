#pragma once

#include "us4/runtime/backends/runtime_types.h"

#include <string>
#include <vector>

namespace us4::runtime::backends
{

    struct BackendDescriptor
    {
        BackendKind kind = BackendKind::kCpu;
        std::string name;
        std::string displayName;
        DeviceClass deviceClass = DeviceClass::kCpuOnly;
        BackendVendor vendor = BackendVendor::kUnknown;
        BackendAvailability availability = BackendAvailability::kStubbed;
        PrecisionMode defaultPrecision = PrecisionMode::kFp16;
        std::uint32_t selectionRank = 0;
        std::uint32_t maxContextTokensHint = 0;
        bool supportsGraphCapture = false;
        bool supportsPagedKv = false;
        bool supportsMoE = false;
        bool supportsSpeculative = false;
        bool supportsNpuOffload = false;
        bool supportsUnifiedMemory = false;
        bool requiresOptIn = false;
        MemoryBudget budget;
    };

    using BackendCatalog = std::vector<BackendDescriptor>;

} // namespace us4::runtime::backends
