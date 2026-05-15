#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace us4::runtime::memory
{

    struct BufferView
    {
        std::size_t offset = 0;
        std::size_t size = 0;
    };

    struct ArenaReservation
    {
        BufferView view;
        std::size_t requestedBytes = 0;
        std::size_t grantedBytes = 0;
        std::size_t alignment = 1;
        bool exact = false;
    };

    class BufferArena
    {
      public:
        explicit BufferArena(std::size_t capacityBytes);

        [[nodiscard]] std::size_t Capacity() const;
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::size_t Available() const;
        [[nodiscard]] std::size_t HighWatermark() const;
        [[nodiscard]] BufferView Reserve(std::size_t bytes);
        [[nodiscard]] ArenaReservation ReserveAligned(std::size_t bytes, std::size_t alignment);
        void Reset();

      private:
        std::vector<std::uint8_t> storage_;
        std::size_t size_ = 0;
        std::size_t highWatermark_ = 0;
    };

} // namespace us4::runtime::memory
