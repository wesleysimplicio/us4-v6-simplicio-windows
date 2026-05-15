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

    struct AttentionOptions
    {
        float scale = 0.0F;
        bool causalMask = true;
        const us4::core::Tensor* cachedKey = nullptr;
        const us4::core::Tensor* cachedValue = nullptr;
    };

    [[nodiscard]] AttentionValidation ValidateScalarAttention(const us4::core::Tensor& query,
                                                              const us4::core::Tensor& key,
                                                              const us4::core::Tensor& value);
    [[nodiscard]] AttentionValidation ValidateScalarAttention(const us4::core::Tensor& query,
                                                              const us4::core::Tensor& key,
                                                              const us4::core::Tensor& value,
                                                              const AttentionOptions& options);
    [[nodiscard]] us4::core::Tensor ScalarAttention(const us4::core::Tensor& query,
                                                    const us4::core::Tensor& key,
                                                    const us4::core::Tensor& value,
                                                    const AttentionOptions& options);
    [[nodiscard]] inline us4::core::Tensor ScalarAttention(const us4::core::Tensor& query,
                                                           const us4::core::Tensor& key,
                                                           const us4::core::Tensor& value,
                                                           const float scale)
    {
        return ScalarAttention(query, key, value,
                               AttentionOptions{
                                   .scale = scale,
                               });
    }

} // namespace us4::runtime::backends::cpu_avx
