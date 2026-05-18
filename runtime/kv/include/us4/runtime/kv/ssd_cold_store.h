#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace us4::runtime::kv
{

    struct SsdColdStoreEntry
    {
        std::string sequenceId;
        std::filesystem::path backingFile;
        std::size_t offsetBytes = 0;
        std::size_t lengthBytes = 0;
        std::uint64_t checksum = 0;
        bool flushed = false;
    };

    struct SsdColdStoreStats
    {
        std::size_t entryCount = 0;
        std::size_t residentBytes = 0;
        std::size_t writeCount = 0;
        std::size_t readCount = 0;
        std::size_t flushCount = 0;
        std::size_t evictionCount = 0;
    };

    class SsdColdStore
    {
      public:
        void ConfigureRoot(std::filesystem::path rootDir);
        [[nodiscard]] bool Write(const std::string& sequenceId, std::vector<std::uint8_t> payload);
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> Read(const std::string& sequenceId);
        [[nodiscard]] bool Flush(const std::string& sequenceId);
        [[nodiscard]] std::size_t FlushAll();
        [[nodiscard]] bool Evict(const std::string& sequenceId);
        [[nodiscard]] std::optional<SsdColdStoreEntry> Find(const std::string& sequenceId) const;
        [[nodiscard]] SsdColdStoreStats Stats() const;
        [[nodiscard]] std::vector<SsdColdStoreEntry> Snapshot() const;

      private:
        [[nodiscard]] static std::uint64_t Checksum(const std::vector<std::uint8_t>& payload);

        std::filesystem::path rootDir_;
        std::unordered_map<std::string, SsdColdStoreEntry> entries_;
        std::unordered_map<std::string, std::vector<std::uint8_t>> buffer_;
        std::size_t writeCount_ = 0;
        std::size_t readCount_ = 0;
        std::size_t flushCount_ = 0;
        std::size_t evictionCount_ = 0;
    };

} // namespace us4::runtime::kv
