#pragma once

#include "runtime/core/tensor.h"

#include <string>

namespace us4::runtime::backends::cpu_avx
{

    struct AttentionValidation
    {
        bool ok = false;
        std::string message;
    };

    [[nodiscard]] AttentionValidation ValidateScalarAttention(const us4::core::Tensor& query,
                                                              const us4::core::Tensor& key,
                                                              const us4::core::Tensor& value);
    [[nodiscard]] us4::core::Tensor ScalarAttention(const us4::core::Tensor& query,
                                                    const us4::core::Tensor& key,
                                                    const us4::core::Tensor& value,
                                                    float scale = 0.0F);

} // namespace us4::runtime::backends::cpu_avx
