#pragma once

#include "us4/runtime/backends/runtime_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace us4::runtime::tuning
{

    struct TuningDecision
    {
        std::string key;
        std::string value;
        std::string rationale;
    };

    struct TuningProbe
    {
        std::string benchmarkName;
        std::string backend;
        std::string profileId;
        std::size_t promptTokens = 0;
        std::size_t generationTokens = 0;
        bool regressionCritical = false;
        std::string rationale;
    };

    struct TuningPlan
    {
        std::vector<TuningDecision> decisions;
        std::vector<TuningProbe> probes;
    };

    class AutoTuner
    {
      public:
        [[nodiscard]] TuningPlan
        BuildPlan(const backends::SessionRequest& request,
                  const backends::HardwareCapabilities& capabilities) const;
    };

} // namespace us4::runtime::tuning
