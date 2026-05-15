#include "us4/runtime/backends/windows_ml/power_thermal_monitor.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace us4::runtime::backends::windows_ml
{

    namespace
    {
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

        std::string Normalize(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch)
                           { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        bool ReadBoolEnv(const char* key, const bool fallback = false)
        {
            const auto value = ReadEnv(key);
            if (!value.has_value())
            {
                return fallback;
            }

            const std::string normalized = Normalize(*value);
            return normalized == "1" || normalized == "true" || normalized == "on" ||
                   normalized == "yes";
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

        PowerSource ParsePowerSource(const std::string& value)
        {
            const std::string normalized = Normalize(value);
            if (normalized == "ac" || normalized == "plugged")
            {
                return PowerSource::kAc;
            }
            if (normalized == "battery" || normalized == "dc")
            {
                return PowerSource::kBattery;
            }
            return PowerSource::kUnknown;
        }

        ThermalState ParseThermalState(const std::string& value)
        {
            const std::string normalized = Normalize(value);
            if (normalized == "nominal" || normalized == "cool")
            {
                return ThermalState::kNominal;
            }
            if (normalized == "warm")
            {
                return ThermalState::kWarm;
            }
            if (normalized == "throttled" || normalized == "throttle")
            {
                return ThermalState::kThrottled;
            }
            if (normalized == "critical" || normalized == "hot")
            {
                return ThermalState::kCritical;
            }
            return ThermalState::kUnknown;
        }

        bool HasSyntheticOverride()
        {
            return ReadEnv("US4_POWER_SOURCE").has_value() ||
                   ReadEnv("US4_BATTERY_PERCENT").has_value() ||
                   ReadEnv("US4_BATTERY_SAVER").has_value() ||
                   ReadEnv("US4_THERMAL_STATE").has_value() ||
                   ReadEnv("US4_ETW_THROTTLED").has_value();
        }
    } // namespace

    std::string ToString(const PowerSource source)
    {
        switch (source)
        {
        case PowerSource::kAc:
            return "ac";
        case PowerSource::kBattery:
            return "battery";
        case PowerSource::kUnknown:
            return "unknown";
        }

        return "unknown";
    }

    std::string ToString(const ThermalState state)
    {
        switch (state)
        {
        case ThermalState::kNominal:
            return "nominal";
        case ThermalState::kWarm:
            return "warm";
        case ThermalState::kThrottled:
            return "throttled";
        case ThermalState::kCritical:
            return "critical";
        case ThermalState::kUnknown:
            return "unknown";
        }

        return "unknown";
    }

    std::string ToString(const PowerDispatchPolicy policy)
    {
        switch (policy)
        {
        case PowerDispatchPolicy::kNominal:
            return "nominal";
        case PowerDispatchPolicy::kPreferEfficiency:
            return "prefer-efficiency";
        case PowerDispatchPolicy::kThermalThrottle:
            return "thermal-throttle";
        case PowerDispatchPolicy::kCriticalFallback:
            return "critical-fallback";
        }

        return "nominal";
    }

    PowerThermalSnapshot PowerThermalMonitor::Sample()
    {
        PowerThermalSnapshot snapshot{};
        snapshot.usingSyntheticTelemetry = HasSyntheticOverride();

#if defined(_WIN32)
        SYSTEM_POWER_STATUS powerStatus{};
        if (GetSystemPowerStatus(&powerStatus) != 0)
        {
            if (powerStatus.ACLineStatus == 1)
            {
                snapshot.powerSource = PowerSource::kAc;
            }
            else if (powerStatus.ACLineStatus == 0)
            {
                snapshot.powerSource = PowerSource::kBattery;
            }

            if (powerStatus.BatteryLifePercent != 255)
            {
                snapshot.batteryPercent = powerStatus.BatteryLifePercent;
            }

            snapshot.batterySaverActive = (powerStatus.SystemStatusFlag & 0x01) != 0;
            snapshot.thermalState = ThermalState::kNominal;
        }
#endif

        if (const auto value = ReadEnv("US4_POWER_SOURCE"); value.has_value())
        {
            snapshot.powerSource = ParsePowerSource(*value);
        }
        if (const auto value = ReadUintEnv("US4_BATTERY_PERCENT"); value.has_value())
        {
            snapshot.batteryPercent = std::min<std::uint32_t>(*value, 100U);
        }
        if (ReadEnv("US4_BATTERY_SAVER").has_value())
        {
            snapshot.batterySaverActive = ReadBoolEnv("US4_BATTERY_SAVER");
        }
        if (const auto value = ReadEnv("US4_THERMAL_STATE"); value.has_value())
        {
            snapshot.thermalState = ParseThermalState(*value);
        }
        if (ReadEnv("US4_ETW_THROTTLED").has_value())
        {
            snapshot.etwThrottleActive = ReadBoolEnv("US4_ETW_THROTTLED");
        }

        return snapshot;
    }

    PowerDispatchPolicy PowerThermalMonitor::SelectPolicy(const PowerThermalSnapshot& snapshot)
    {
        if (snapshot.thermalState == ThermalState::kCritical)
        {
            return PowerDispatchPolicy::kCriticalFallback;
        }

        if (snapshot.etwThrottleActive || snapshot.thermalState == ThermalState::kThrottled)
        {
            return PowerDispatchPolicy::kThermalThrottle;
        }

        if (snapshot.batterySaverActive ||
            (snapshot.powerSource == PowerSource::kBattery && snapshot.batteryPercent <= 25U))
        {
            return PowerDispatchPolicy::kPreferEfficiency;
        }

        return PowerDispatchPolicy::kNominal;
    }

    std::vector<RuntimeIssue> PowerThermalMonitor::BuildIssues(const PowerThermalSnapshot& snapshot)
    {
        std::vector<RuntimeIssue> issues;
        const auto policy = SelectPolicy(snapshot);

        if (policy == PowerDispatchPolicy::kPreferEfficiency)
        {
            issues.push_back({"windows_ml.power.battery_saver",
                              "Battery saver or low battery requested an efficiency-biased mixed "
                              "dispatch plan.",
                              true});
        }

        if (policy == PowerDispatchPolicy::kThermalThrottle)
        {
            issues.push_back({"windows_ml.power.thermal_throttle",
                              "Thermal or ETW throttle signals demoted dense NPU slices to the "
                              "GPU/CPU fallback path.",
                              true});
        }

        if (policy == PowerDispatchPolicy::kCriticalFallback)
        {
            issues.push_back({"windows_ml.power.critical_fallback",
                              "Critical thermal pressure forced Windows ML dense slices off the "
                              "NPU path.",
                              true});
        }

        if (snapshot.etwThrottleActive)
        {
            issues.push_back({"windows_ml.power.etw_signal",
                              "Synthetic or ETW throttle signal was observed while building the "
                              "Windows ML dispatch policy.",
                              true});
        }

        return issues;
    }

} // namespace us4::runtime::backends::windows_ml
