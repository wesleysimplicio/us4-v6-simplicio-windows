#pragma once

#include "us4/runtime/backends/backend_descriptor.h"
#include "us4/runtime/backends/runtime_types.h"
#include "us4/runtime/telemetry/telemetry_sink.h"

#include <cstddef>
#include <string>
#include <vector>

namespace us4::core
{

    struct ProbeAdvisory
    {
        std::string severity;
        std::string code;
        std::string message;
    };

    struct ProbeSummary
    {
        struct MoeTelemetryPreview
        {
            std::size_t routeCount = 0;
            float hotHitRate = 0.0F;
            float warmHitRate = 0.0F;
            float coldHitRate = 0.0F;
            std::size_t evictionCount = 0;
            std::size_t coldOffloadCount = 0;
            std::size_t reloadCount = 0;
            float routerEntropy = 0.0F;
            std::vector<us4::runtime::telemetry::TelemetryEvent> events;
        };

        std::string osName;
        std::string cpuName;
        std::string gpuName;
        bool hasNpu = false;
        std::string selectedBackend;
        std::string mode;
        us4::runtime::backends::HardwareCapabilities capabilities;
        us4::runtime::backends::BackendCatalog backends;
        std::vector<std::string> fallbackBackends;
        std::vector<ProbeAdvisory> advisories;
        MoeTelemetryPreview moeTelemetry;

        [[nodiscard]] bool HasAccelerator() const;
        [[nodiscard]] bool IsDegraded() const;
    };

    ProbeSummary ProbeHardware();
    std::string FormatProbeSummary(const ProbeSummary& summary);
    std::string BuildLaunchHint(const ProbeSummary& summary);

} // namespace us4::core
