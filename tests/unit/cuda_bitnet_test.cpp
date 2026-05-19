#include "us4/runtime/backends/cpu_avx/bitnet_reference.h"
#include "us4/runtime/backends/cuda/cuda_kernels.h"

#include <cmath>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        std::vector<float> MakeWeights(const std::size_t width)
        {
            std::vector<float> weights(width, 0.0F);
            for (std::size_t index = 0U; index < width; ++index)
            {
                const std::size_t mod = index % 7U;
                weights[index] = mod < 2U ? -0.85F : (mod < 4U ? 0.0F : 0.9F);
            }
            return weights;
        }

        std::vector<float> MakeActivations(const std::size_t width)
        {
            std::vector<float> activations(width, 0.0F);
            for (std::size_t index = 0U; index < width; ++index)
            {
                activations[index] = static_cast<float>((index % 19U) - 9U) * 0.125F;
            }
            return activations;
        }

        TEST(CudaBitNetTest, PackedCudaPathMatchesScalarBitNetReference)
        {
            const auto weights = MakeWeights(1024U);
            const auto activations = MakeActivations(weights.size());

            const auto scalarPacked = us4::runtime::backends::cpu_avx::PackBitNetRow(weights);
            const auto cudaPacked = us4::runtime::backends::cuda::PackCudaBitNetRow(weights);

            const float scalar =
                us4::runtime::backends::cpu_avx::DotPackedBitNet(activations, scalarPacked);
            const float cuda =
                us4::runtime::backends::cuda::CudaBitNetMatMul(activations, cudaPacked);

            EXPECT_NEAR(cuda, scalar, 5.0e-3F);
        }
    } // namespace
} // namespace us4::runtime::tests
