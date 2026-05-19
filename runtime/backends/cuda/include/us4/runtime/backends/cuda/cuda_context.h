#pragma once

#include "us4/runtime/backends/cuda/cuda_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::backends::cuda
{

    struct CudaStreamHandle
    {
        std::uint32_t id = 0U;
        std::string label;
        bool highPriority = false;
        bool dedicatedTransfer = false;
    };

    struct CudaPoolAllocation
    {
        std::uint64_t id = 0U;
        std::size_t bytes = 0U;
        std::size_t poolOffsetBytes = 0U;
        bool reusedFromPool = false;
        std::string tag;
    };

    struct CudaGraphExecutable
    {
        std::uint32_t id = 0U;
        std::string name;
        std::string cacheKey;
        bool persistent = false;
        std::uint32_t warmupIterations = 0U;
        std::vector<std::string> operations;
    };

    struct CudaGraphLease
    {
        CudaGraphExecutable executable;
        bool reusedExistingGraph = false;
        std::uint32_t reuseCount = 0U;
    };

    struct CudaContextStatus
    {
        bool ok = true;
        std::string code;
        std::string detail;
    };

    class CudaContext
    {
      public:
        [[nodiscard]] static CudaContext Create(const CudaExecutionPlan& plan);

        [[nodiscard]] const CudaDeviceProperties& Device() const noexcept;
        [[nodiscard]] const CudaExecutionPlan& Plan() const noexcept;

        [[nodiscard]] CudaStreamHandle AcquirePrefillStream();
        [[nodiscard]] CudaStreamHandle AcquireDecodeStream();
        [[nodiscard]] CudaStreamHandle AcquireTransferStream();
        [[nodiscard]] CudaContextStatus ReleaseStream(const CudaStreamHandle& stream);

        [[nodiscard]] CudaPoolAllocation AllocateAsync(std::size_t bytes, std::string_view tag);
        [[nodiscard]] CudaContextStatus ReleaseAllocation(const CudaPoolAllocation& allocation);

        [[nodiscard]] std::optional<CudaGraphExecutable>
        CaptureGraph(std::string_view name, const std::vector<std::string>& operations);
        [[nodiscard]] std::optional<CudaGraphLease>
        AcquireSpeculativeGraph(std::string_view cacheKey, std::string_view name,
                                const std::vector<std::string>& operations);
        [[nodiscard]] TokenChunk ReplayGraph(const CudaGraphExecutable& executable,
                                             std::uint32_t tokenCount, std::uint32_t seedSalt);
        void ClearGraphCache();

        [[nodiscard]] std::size_t PoolCapacityBytes() const noexcept;
        [[nodiscard]] std::size_t PoolReservedBytes() const noexcept;
        [[nodiscard]] std::uint32_t ActiveStreamCount() const noexcept;
        [[nodiscard]] std::uint32_t CachedGraphCount() const noexcept;
        [[nodiscard]] std::uint32_t GraphCaptureCount() const noexcept;
        [[nodiscard]] std::uint32_t GraphCacheHitCount() const noexcept;
        [[nodiscard]] std::uint32_t GraphReplayCount() const noexcept;
        [[nodiscard]] const std::vector<std::string>& ExecutionTrace() const noexcept;

      private:
        struct PoolBlock
        {
            std::uint64_t id = 0U;
            std::size_t capacityBytes = 0U;
            std::size_t offsetBytes = 0U;
            bool inUse = false;
        };

        struct CachedGraphEntry
        {
            std::string cacheKey;
            CudaGraphExecutable executable;
            std::uint32_t reuseCount = 0U;
        };

        explicit CudaContext(CudaExecutionPlan plan);

        [[nodiscard]] CudaStreamHandle AcquireStream(std::string_view lane, std::uint32_t modulo,
                                                     bool highPriority, bool dedicatedTransfer);
        [[nodiscard]] std::vector<std::int32_t> BuildReferenceTokens(std::uint32_t tokenCount,
                                                                     std::uint32_t seedSalt) const;

        CudaExecutionPlan plan_;
        std::uint32_t nextStreamId_ = 1U;
        std::uint64_t nextAllocationId_ = 1U;
        std::uint32_t nextGraphId_ = 1U;
        std::uint32_t prefillCursor_ = 0U;
        std::uint32_t decodeCursor_ = 0U;
        std::uint32_t graphCaptureCount_ = 0U;
        std::uint32_t graphCacheHitCount_ = 0U;
        std::uint32_t graphReplayCount_ = 0U;
        std::vector<CudaStreamHandle> activeStreams_;
        std::vector<PoolBlock> pool_;
        std::vector<CachedGraphEntry> graphCache_;
        std::vector<std::string> executionTrace_;
    };

} // namespace us4::runtime::backends::cuda
