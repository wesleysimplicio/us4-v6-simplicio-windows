#pragma once

#include "us4/runtime/backends/cpu_avx/bitnet_reference.h"

#include <vector>

namespace us4::runtime::backends::cpu_avx
{

    [[nodiscard]] float DotPackedBitNetAvx2(const std::vector<float>& activations,
                                            const PackedBitNetRow& packed);

} // namespace us4::runtime::backends::cpu_avx
