#include "us4/runtime/adapters/bitnet_adapter_contracts.h"
#include "us4/runtime/adapters/bitnet_model_loader.h"
#include "us4/runtime/adapters/bitnet_reference_hooks.h"
#include "us4/runtime/adapters/ternary_adapter_contracts.h"
#include "us4/runtime/adapters/ternary_model_loader.h"
#include "us4/runtime/adapters/ternary_reference_hooks.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {

        TEST(BitNetTernaryTest, BitNetReferencePackProducesExpectedSigns)
        {
            const auto packed = us4::runtime::adapters::PackBitNetReferenceWeights(
                {1.0F, -0.9F, 0.0F, 0.2F}, 2U, 2U, us4::runtime::adapters::BitNetExecutionHints{});

            ASSERT_EQ(packed.values.size(), 4U);
            EXPECT_EQ(packed.values[0], 1);
            EXPECT_EQ(packed.values[1], -1);
            EXPECT_EQ(packed.values[2], 0);
        }

        TEST(BitNetTernaryTest, TernaryReferenceMatVecAcceptsSignedZeroLayout)
        {
            const auto packed = us4::runtime::adapters::PackTernaryReferenceWeights(
                {1.0F, -1.0F, 0.0F, 0.0F}, 2U, 2U, us4::runtime::adapters::TernaryExecutionHints{});
            const auto output =
                us4::runtime::adapters::RunTernaryReferenceMatVec(packed, {0.5F, 2.0F});

            ASSERT_EQ(output.size(), 2U);
            EXPECT_FLOAT_EQ(output[0], -1.5F);
        }

        TEST(BitNetTernaryTest, SyntheticBitNetLoaderInfersOnePointFiveBitConfig)
        {
            const auto loadResult = us4::runtime::adapters::LoadBitNetModelAsset(
                "builtin://bitnet-b1.58-tmac", "bitnet-b1.58-tmac-3b");

            ASSERT_TRUE(loadResult.ok);
            EXPECT_EQ(loadResult.descriptor.hints.preferredKernel,
                      us4::runtime::adapters::BitNetKernelStyle::kTMacTiles);
            EXPECT_EQ(loadResult.descriptor.quantScale, 1.58F);
        }

        TEST(BitNetTernaryTest, SyntheticTernaryLoaderInfersActivationProbe)
        {
            const auto loadResult = us4::runtime::adapters::LoadTernaryModelAsset(
                "builtin://ternary-int4", "ternary-int4-7b-gqa");

            ASSERT_TRUE(loadResult.ok);
            EXPECT_EQ(loadResult.descriptor.hints.activationMode,
                      us4::runtime::adapters::TernaryActivationMode::kInt4Probe);
            EXPECT_EQ(loadResult.descriptor.kvHeads, 8U);
        }

    } // namespace
} // namespace us4::runtime::tests
