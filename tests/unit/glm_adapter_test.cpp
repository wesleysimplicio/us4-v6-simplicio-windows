#include "us4/runtime/adapters/i_us4_windows_adapter.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(GlmAdapterTest, FactoryReturnsAdapter)
    {
        const auto adapter = adapters::CreateGlmScalarAdapter();
        ASSERT_NE(adapter, nullptr);
        EXPECT_FALSE(adapter->Describe().adapterId.empty());
    }

    TEST(GlmAdapterTest, FamilyIsGlm)
    {
        const auto adapter = adapters::CreateGlmScalarAdapter();
        ASSERT_NE(adapter, nullptr);
        EXPECT_EQ(adapter->Describe().family, "glm");
    }

} // namespace us4::runtime::tests
