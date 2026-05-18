#pragma once

#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"

namespace us4::runtime::backends::cpu_avx
{

    [[nodiscard]] us4::core::Tensor Avx2BlockedMatMul(const us4::core::Tensor& left,
                                                      const us4::core::Tensor& right,
                                                      const BlockedMatMulPlan& plan);

} // namespace us4::runtime::backends::cpu_avx
