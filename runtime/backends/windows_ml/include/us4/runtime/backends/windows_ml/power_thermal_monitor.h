#pragma once

#include "us4/runtime/backends/runtime_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace us4::runtime::backends::windows_ml
{

    enum class PowerSource
    {
        kUnknown,
        kAc,
        kBattery,
    };

    enum class ThermalState
    {
        kUnknown,
        kNominal,
        kWarm,
        kThrottled,
        kCritical,
    };

    enum class PowerDispatchPolicy
    {
        kNominal,
        kPreferEfficiency,
        kThermalThrottle,
        kCriticalFallback,
    };

    struct PowerThermalSnapshot
    {
        PowerSource powerSource = PowerSource::kUnknown;
        ThermalState thermalState = ThermalState::kUnknown;
        std::uint32_t batteryPercent = 100;
        bool batterySaverActive = false;
        bool etwThrottleActive = false;
        bool usingSyntheticTelemetry = false;
    };

    [[nodiscard]] std::string ToString(PowerSource source);
    [[nodiscard]] std::string ToString(ThermalState state);
    [[nodiscard]] std::string ToString(PowerDispatchPolicy policy);

    class PowerThermalMonitor
    {
      public:
        [[nodiscard]] static PowerThermalSnapshot Sample();
        [[nodiscard]] static PowerDispatchPolicy SelectPolicy(const PowerThermalSnapshot& snapshot);
        [[nodiscard]] static std::vector<RuntimeIssue>
        BuildIssues(const PowerThermalSnapshot& snapshot);
    };

} // namespace us4::runtime::backends::windows_ml
