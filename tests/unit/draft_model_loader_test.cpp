#include "us4/runtime/speculative/draft_model_loader.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(DraftModelLoaderTest, LoadAcceptsValidDescriptor)
    {
        speculative::DraftModelLoader loader;
        speculative::DraftModelDescriptor desc{};
        desc.modelId = "draft-1.5B";
        desc.parameterCount = 1500000000U;
        desc.hiddenSize = 2048U;
        desc.numLayers = 24U;
        desc.vocabSize = 128000U;
        const auto handle = loader.Load(desc);
        ASSERT_TRUE(handle.has_value());
        EXPECT_TRUE(handle->loaded);
    }

    TEST(DraftModelLoaderTest, RejectsInvalidDescriptor)
    {
        speculative::DraftModelLoader loader;
        speculative::DraftModelDescriptor desc{};
        desc.modelId.clear();
        desc.vocabSize = 0U;
        EXPECT_FALSE(loader.Load(desc).has_value());
    }

    TEST(DraftModelLoaderTest, UnloadDecrementsLoadedCount)
    {
        speculative::DraftModelLoader loader;
        speculative::DraftModelDescriptor desc{};
        desc.modelId = "draft";
        desc.vocabSize = 32000U;
        ASSERT_TRUE(loader.Load(desc).has_value());
        EXPECT_TRUE(loader.IsLoaded("draft"));
        EXPECT_TRUE(loader.Unload("draft"));
        EXPECT_FALSE(loader.IsLoaded("draft"));
    }

} // namespace us4::runtime::tests
