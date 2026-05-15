#pragma once

#include "us4/runtime/backends/runtime_types.h"

namespace us4::runtime::backends {

class HardwareProbe {
 public:
  [[nodiscard]] static HardwareCapabilities DetectHost();
};

}  // namespace us4::runtime::backends
