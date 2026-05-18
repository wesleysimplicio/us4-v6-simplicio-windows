#include "us4/runtime/adapters/llama_config.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(LlamaConfigTest, DefaultsValidate)
    {
        const adapters::LlamaConfig config{};
        EXPECT_TRUE(adapters::Validate(config));
    }

    TEST(LlamaConfigTest, RejectsZeroHeads)
    {
        adapters::LlamaConfig config{};
        config.numAttentionHeads = 0;
        EXPECT_FALSE(adapters::Validate(config));
    }

    TEST(LlamaConfigTest, RejectsNonDivisibleGqaGrouping)
    {
        adapters::LlamaConfig config{};
        config.numAttentionHeads = 32;
        config.numKeyValueHeads = 5;
        EXPECT_FALSE(adapters::Validate(config));
    }

    TEST(LlamaConfigTest, RopeScalingToStringCoversAllVariants)
    {
        EXPECT_EQ(adapters::ToString(adapters::LlamaRopeScaling::kNone), "none");
        EXPECT_EQ(adapters::ToString(adapters::LlamaRopeScaling::kLinear), "linear");
        EXPECT_EQ(adapters::ToString(adapters::LlamaRopeScaling::kDynamic), "dynamic");
        EXPECT_EQ(adapters::ToString(adapters::LlamaRopeScaling::kYarn), "yarn");
    }

} // namespace us4::runtime::tests
