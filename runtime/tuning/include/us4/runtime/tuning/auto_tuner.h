#pragma once

#include "us4/runtime/backends/runtime_types.h"

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

    class AutoTuner
    {
      public:
        [[nodiscard]] std::vector<TuningDecision>
        BuildPlan(const backends::SessionRequest& request,
                  const backends::HardwareCapabilities& capabilities) const;
    };

} // namespace us4::runtime::tuning
