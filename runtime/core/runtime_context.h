#pragma once

#include "runtime/core/model_registry.h"
#include "us4/profiles/runtime_profile.h"
#include "us4/runtime/adapters/adapter_contracts.h"
#include "us4/runtime/backends/backend_descriptor.h"
#include "us4/runtime/backends/runtime_types.h"

#include <string>
#include <vector>

namespace us4::core
{

    struct RuntimePlan
    {
        us4::runtime::backends::SessionRequest request;
        ModelDescriptor model;
        us4::profiles::RuntimeProfile profile;
        us4::runtime::backends::BackendDescriptor backend;
        us4::runtime::backends::BackendCatalog fallbacks;
        us4::runtime::adapters::RuntimeBinding binding;
        us4::runtime::adapters::ModelResidencyPlan residency;
        us4::runtime::backends::RuntimeMode mode = us4::runtime::backends::RuntimeMode::kBalanced;
        std::vector<us4::runtime::backends::RuntimeIssue> issues;
    };

    class RuntimeContext
    {
      public:
        [[nodiscard]] static RuntimePlan
        BuildPlan(const us4::runtime::backends::SessionRequest& request,
                  const us4::runtime::backends::HardwareCapabilities& capabilities);
    };

    [[nodiscard]] std::string FormatRuntimePlan(const RuntimePlan& plan);

} // namespace us4::core
