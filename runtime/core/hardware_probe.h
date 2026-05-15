#pragma once

#include "us4/runtime/backends/backend_descriptor.h"
#include "us4/runtime/backends/runtime_types.h"

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

        [[nodiscard]] bool HasAccelerator() const;
        [[nodiscard]] bool IsDegraded() const;
    };

    ProbeSummary ProbeHardware();
    std::string FormatProbeSummary(const ProbeSummary& summary);
    std::string BuildLaunchHint(const ProbeSummary& summary);

} // namespace us4::core
