#pragma once

#include "runtime/core/tensor.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace us4::runtime::backends::cpu_avx
{

    [[nodiscard]] us4::core::Tensor DequantizeInt8GroupWise(const std::vector<std::int8_t>& values,
                                                            std::size_t rows, std::size_t cols,
                                                            std::size_t groupSize,
                                                            const std::vector<float>& scales);

} // namespace us4::runtime::backends::cpu_avx
