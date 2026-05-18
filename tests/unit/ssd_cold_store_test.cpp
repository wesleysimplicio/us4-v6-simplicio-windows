#include "us4/runtime/kv/ssd_cold_store.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(SsdColdStoreTest, WriteThenReadReturnsPayload)
    {
        kv::SsdColdStore store;
        store.ConfigureRoot("/tmp/us4-test");
        std::vector<std::uint8_t> payload{1U, 2U, 3U, 4U, 5U};
        EXPECT_TRUE(store.Write("seq-1", payload));
        const auto read = store.Read("seq-1");
        ASSERT_TRUE(read.has_value());
        EXPECT_EQ(read->size(), 5U);
    }

    TEST(SsdColdStoreTest, FlushMarksEntryFlushed)
    {
        kv::SsdColdStore store;
        store.ConfigureRoot("/tmp/us4-test");
        EXPECT_TRUE(store.Write("seq-1", {0U, 1U, 2U}));
        EXPECT_TRUE(store.Flush("seq-1"));
        const auto entry = store.Find("seq-1");
        ASSERT_TRUE(entry.has_value());
        EXPECT_TRUE(entry->flushed);
    }

    TEST(SsdColdStoreTest, EvictRemovesEntry)
    {
        kv::SsdColdStore store;
        store.ConfigureRoot("/tmp/us4-test");
        EXPECT_TRUE(store.Write("seq-1", {0U}));
        EXPECT_TRUE(store.Evict("seq-1"));
        EXPECT_FALSE(store.Find("seq-1").has_value());
        EXPECT_EQ(store.Stats().evictionCount, 1U);
    }

    TEST(SsdColdStoreTest, ChecksumIsStableForSamePayload)
    {
        kv::SsdColdStore store;
        store.ConfigureRoot("/tmp/us4-test");
        EXPECT_TRUE(store.Write("seq-a", {7U, 8U, 9U}));
        EXPECT_TRUE(store.Write("seq-b", {7U, 8U, 9U}));
        const auto a = store.Find("seq-a");
        const auto b = store.Find("seq-b");
        ASSERT_TRUE(a.has_value());
        ASSERT_TRUE(b.has_value());
        EXPECT_EQ(a->checksum, b->checksum);
    }

} // namespace us4::runtime::tests
