#pragma once

#include "us4/runtime/backends/backend_descriptor.h"
#include "us4/runtime/backends/runtime_types.h"

#include <optional>
#include <string>
#include <string_view>

namespace us4::core
{

    [[nodiscard]] std::string ToString(us4::runtime::backends::RuntimeMode mode);
    [[nodiscard]] std::optional<us4::runtime::backends::RuntimeMode>
    ParseRuntimeMode(std::string_view value);
    [[nodiscard]] us4::runtime::backends::RuntimeMode
    SelectRuntimeMode(const us4::runtime::backends::BackendDescriptor& backend,
                      const us4::runtime::backends::HardwareCapabilities& capabilities,
                      const us4::runtime::backends::SessionRequest& request);

} // namespace us4::core
