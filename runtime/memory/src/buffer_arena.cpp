#include "us4/runtime/memory/buffer_arena.h"

#include <algorithm>

namespace us4::runtime::memory
{

    namespace
    {

        std::size_t AlignUp(std::size_t value, std::size_t alignment)
        {
            if (alignment <= 1)
            {
                return value;
            }
            const std::size_t remainder = value % alignment;
            return remainder == 0 ? value : (value + alignment - remainder);
        }

    } // namespace

    BufferArena::BufferArena(std::size_t capacityBytes) : storage_(capacityBytes, 0U) {}

    std::size_t BufferArena::Capacity() const
    {
        return storage_.size();
    }

    std::size_t BufferArena::Size() const
    {
        return size_;
    }

    std::size_t BufferArena::Available() const
    {
        return storage_.size() - std::min(size_, storage_.size());
    }

    std::size_t BufferArena::HighWatermark() const
    {
        return highWatermark_;
    }

    BufferView BufferArena::Reserve(std::size_t bytes)
    {
        const std::size_t granted = std::min(bytes, Available());
        BufferView view{size_, granted};
        size_ += granted;
        highWatermark_ = std::max(highWatermark_, size_);
        return view;
    }

    ArenaReservation BufferArena::ReserveAligned(std::size_t bytes, std::size_t alignment)
    {
        const std::size_t normalizedAlignment = alignment == 0 ? 1 : alignment;
        const std::size_t alignedOffset = AlignUp(size_, normalizedAlignment);
        const std::size_t remaining = storage_.size() - std::min(alignedOffset, storage_.size());
        const std::size_t granted = std::min(bytes, remaining);
        size_ = alignedOffset + granted;
        highWatermark_ = std::max(highWatermark_, size_);
        return ArenaReservation{
            .view =
                BufferView{
                    .offset = alignedOffset,
                    .size = granted,
                },
            .requestedBytes = bytes,
            .grantedBytes = granted,
            .alignment = normalizedAlignment,
            .exact = granted == bytes,
        };
    }

    void BufferArena::Reset()
    {
        size_ = 0;
    }

} // namespace us4::runtime::memory
