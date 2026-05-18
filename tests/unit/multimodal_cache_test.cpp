#include "us4/runtime/cache/multimodal_cache.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(MultimodalCacheTest, ChannelToStringMapsAllVariants)
    {
        EXPECT_EQ(cache::ToString(cache::MultimodalChannel::kImage), "image");
        EXPECT_EQ(cache::ToString(cache::MultimodalChannel::kAudio), "audio");
        EXPECT_EQ(cache::ToString(cache::MultimodalChannel::kVideo), "video");
    }

    TEST(MultimodalCacheTest, UpsertCountsPerChannel)
    {
        cache::MultimodalCache cache;
        cache.Configure(8192U);
        cache.Upsert({.key = "img-1", .channel = cache::MultimodalChannel::kImage, .residentBytes = 2048U});
        cache.Upsert({.key = "aud-1", .channel = cache::MultimodalChannel::kAudio, .residentBytes = 1024U});
        cache.Upsert({.key = "vid-1", .channel = cache::MultimodalChannel::kVideo, .residentBytes = 4096U});

        const auto stats = cache.Stats();
        EXPECT_EQ(stats.entryCount, 3U);
        EXPECT_EQ(stats.imageEntries, 1U);
        EXPECT_EQ(stats.audioEntries, 1U);
        EXPECT_EQ(stats.videoEntries, 1U);
        EXPECT_EQ(stats.residentBytes, 7168U);
    }

    TEST(MultimodalCacheTest, EraseRemovesEntry)
    {
        cache::MultimodalCache cache;
        cache.Configure(4096U);
        cache.Upsert({.key = "img-1", .channel = cache::MultimodalChannel::kImage, .residentBytes = 1024U});
        EXPECT_TRUE(cache.Erase("img-1"));
        EXPECT_FALSE(cache.Contains("img-1"));
    }

} // namespace us4::runtime::tests
