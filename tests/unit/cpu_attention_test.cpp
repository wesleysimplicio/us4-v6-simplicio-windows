#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/avx_attention.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"
#include "us4/runtime/backends/cpu_avx/scalar_attention.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    TEST(CpuAttentionTest, AvxAttentionMatchesScalarReferenceWithCausalMaskAndCache)
    {
        if (!backends::cpu_avx::HostSupportsAvx2MatMul())
        {
            GTEST_SKIP() << "Host CPU does not expose AVX2.";
        }

        us4::core::Tensor query({3U, 16U});
        us4::core::Tensor key({3U, 16U});
        us4::core::Tensor value({3U, 16U});
        us4::core::Tensor cachedKey({2U, 16U});
        us4::core::Tensor cachedValue({2U, 16U});

        for (std::size_t index = 0U; index < query.ElementCount(); ++index)
        {
            query[index] = static_cast<float>((index % 11U) - 5U) * 0.0625F;
            key[index] = static_cast<float>((index % 7U) - 3U) * 0.125F;
            value[index] = static_cast<float>((index % 13U) - 6U) * 0.09375F;
        }
        for (std::size_t index = 0U; index < cachedKey.ElementCount(); ++index)
        {
            cachedKey[index] = static_cast<float>((index % 5U) - 2U) * 0.25F;
            cachedValue[index] = static_cast<float>((index % 9U) - 4U) * 0.1875F;
        }

        const backends::cpu_avx::AttentionOptions options{
            .scale = 1.0F / 4.0F,
            .causalMask = true,
            .cachedKey = &cachedKey,
            .cachedValue = &cachedValue,
        };

        const auto reference = backends::cpu_avx::ScalarAttention(query, key, value, options);
        const auto accelerated = backends::cpu_avx::AvxAttention(query, key, value, options);

        ASSERT_EQ(reference.ElementCount(), accelerated.ElementCount());
        for (std::size_t index = 0U; index < reference.ElementCount(); ++index)
        {
            EXPECT_NEAR(reference[index], accelerated[index], 1e-3F);
        }
    }
} // namespace us4::runtime::tests
