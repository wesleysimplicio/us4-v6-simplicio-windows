#pragma once

#include "us4/runtime/backends/cpu_avx/scalar_attention.h"

namespace us4::runtime::backends::cpu_avx
{

    [[nodiscard]] us4::core::Tensor AvxAttention(const us4::core::Tensor& query,
                                                 const us4::core::Tensor& key,
                                                 const us4::core::Tensor& value,
                                                 const AttentionOptions& options);

} // namespace us4::runtime::backends::cpu_avx
