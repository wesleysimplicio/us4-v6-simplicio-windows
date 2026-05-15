#pragma once

#include "us4/runtime/backends/backend_descriptor.h"

#include <optional>

namespace us4::runtime::backends
{

    class BackendSelector
    {
      public:
        [[nodiscard]] static BackendCatalog BuildCatalog(const HardwareCapabilities& capabilities);
        [[nodiscard]] static std::optional<BackendDescriptor>
        SelectPrimary(const SessionRequest& request, const HardwareCapabilities& capabilities);
        [[nodiscard]] static BackendCatalog
        SelectFallbacks(const SessionRequest& request, const HardwareCapabilities& capabilities);
    };

} // namespace us4::runtime::backends
