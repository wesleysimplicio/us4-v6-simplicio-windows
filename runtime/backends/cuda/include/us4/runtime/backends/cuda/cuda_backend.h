#pragma once

#include "us4/runtime/backends/cuda/cuda_types.h"

#include <string_view>

namespace us4::runtime::backends::cuda
{

    class CudaBackend
    {
      public:
        [[nodiscard]] static CudaAvailabilityProbe
        ProbeSupport(const HardwareCapabilities& capabilities);
        [[nodiscard]] static CudaDeviceProperties
        DescribeDevice(const HardwareCapabilities& capabilities);
        [[nodiscard]] static BackendDescriptor
        BuildDescriptor(const HardwareCapabilities& capabilities);
        [[nodiscard]] static CudaExecutionPlan
        BuildExecutionPlan(const SessionRequest& request, const HardwareCapabilities& capabilities);
        [[nodiscard]] static CudaStepResult PreparePrefill(const CudaExecutionPlan& plan,
                                                           std::string_view prompt);
        [[nodiscard]] static CudaStepResult PrepareDecode(const CudaExecutionPlan& plan,
                                                          const TokenChunk& prefillChunk,
                                                          std::uint32_t stepIndex);
    };

} // namespace us4::runtime::backends::cuda
