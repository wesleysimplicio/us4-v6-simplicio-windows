#include "us4/runtime/adapters/i_us4_windows_adapter.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(MiniMaxAdapterTest, FactoryReturnsAdapter)
    {
        const auto adapter = adapters::CreateMiniMaxScalarAdapter();
        ASSERT_NE(adapter, nullptr);
        EXPECT_FALSE(adapter->Describe().adapterId.empty());
    }

    TEST(MiniMaxAdapterTest, FamilyIsMiniMax)
    {
        const auto adapter = adapters::CreateMiniMaxScalarAdapter();
        ASSERT_NE(adapter, nullptr);
        EXPECT_EQ(adapter->Describe().family, "minimax");
    }

} // namespace us4::runtime::tests
