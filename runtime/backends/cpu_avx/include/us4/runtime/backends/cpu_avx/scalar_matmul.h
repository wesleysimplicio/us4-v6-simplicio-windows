#pragma once

#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"

#include <string>

namespace us4::runtime::backends::cpu_avx
{

    struct MatMulValidation
    {
        bool ok = false;
        std::string message;
    };

    [[nodiscard]] MatMulValidation ValidateScalarMatMul(const us4::core::Tensor& left,
                                                        const us4::core::Tensor& right);
    [[nodiscard]] BlockedMatMulPlan ExplainScalarMatMulPlan(const us4::core::Tensor& left,
                                                            const us4::core::Tensor& right);
    [[nodiscard]] us4::core::Tensor ScalarMatMul(const us4::core::Tensor& left,
                                                 const us4::core::Tensor& right);

} // namespace us4::runtime::backends::cpu_avx
