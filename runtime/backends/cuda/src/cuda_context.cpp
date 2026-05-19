#include "us4/runtime/backends/cuda/cuda_context.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <utility>

namespace us4::runtime::backends::cuda
{

    namespace
    {
        [[nodiscard]] std::string MakeTraceLine(std::string_view verb, std::string_view detail)
        {
            std::ostringstream output;
            output << verb << ": " << detail;
            return output.str();
        }
    } // namespace

    CudaContext::CudaContext(CudaExecutionPlan plan) : plan_(std::move(plan))
    {
        executionTrace_.push_back(MakeTraceLine("init", plan_.device.adapterName));

        std::size_t offsetBytes = 0U;
        if (plan_.memory.enablePoolAllocator)
        {
            const std::size_t blockSize =
                std::max<std::size_t>(4ULL * 1024ULL * 1024ULL, plan_.memory.scratchBytes / 2U);
            for (std::uint32_t index = 0U;
                 index < std::max<std::uint32_t>(2U, plan_.streams.prefillStreams); ++index)
            {
                pool_.push_back(PoolBlock{
                    .id = nextAllocationId_++,
                    .capacityBytes = blockSize,
                    .offsetBytes = offsetBytes,
                    .inUse = false,
                });
                offsetBytes += blockSize;
            }
        }
    }

    CudaContext CudaContext::Create(const CudaExecutionPlan& plan)
    {
        return CudaContext(plan);
    }

    const CudaDeviceProperties& CudaContext::Device() const noexcept
    {
        return plan_.device;
    }

    const CudaExecutionPlan& CudaContext::Plan() const noexcept
    {
        return plan_;
    }

    CudaStreamHandle CudaContext::AcquireStream(const std::string_view lane,
                                                const std::uint32_t modulo, const bool highPriority,
                                                const bool dedicatedTransfer)
    {
        const std::uint32_t laneIndex =
            modulo == 0U
                ? 0U
                : (lane == "prefill" ? prefillCursor_++ % modulo : decodeCursor_++ % modulo);

        CudaStreamHandle stream{
            .id = nextStreamId_++,
            .label = std::string(lane) + "-" + std::to_string(laneIndex),
            .highPriority = highPriority,
            .dedicatedTransfer = dedicatedTransfer,
        };
        activeStreams_.push_back(stream);
        executionTrace_.push_back(MakeTraceLine("stream.acquire", stream.label));
        return stream;
    }

    CudaStreamHandle CudaContext::AcquirePrefillStream()
    {
        return AcquireStream("prefill", std::max<std::uint32_t>(1U, plan_.streams.prefillStreams),
                             plan_.streams.usePriorityStreams, false);
    }

    CudaStreamHandle CudaContext::AcquireDecodeStream()
    {
        return AcquireStream("decode", std::max<std::uint32_t>(1U, plan_.streams.decodeStreams),
                             plan_.streams.usePriorityStreams, false);
    }

    CudaStreamHandle CudaContext::AcquireTransferStream()
    {
        const bool dedicated = plan_.streams.useDedicatedTransferStream;
        return AcquireStream(dedicated ? "transfer" : "prefill",
                             dedicated ? 1U
                                       : std::max<std::uint32_t>(1U, plan_.streams.prefillStreams),
                             false, dedicated);
    }

    CudaContextStatus CudaContext::ReleaseStream(const CudaStreamHandle& stream)
    {
        const auto originalSize = activeStreams_.size();
        activeStreams_.erase(std::remove_if(activeStreams_.begin(), activeStreams_.end(),
                                            [&](const CudaStreamHandle& candidate)
                                            { return candidate.id == stream.id; }),
                             activeStreams_.end());
        if (activeStreams_.size() == originalSize)
        {
            return {.ok = false,
                    .code = "cuda.stream.unknown",
                    .detail = "Attempted to release a CUDA stream handle that is not active."};
        }
        executionTrace_.push_back(MakeTraceLine("stream.release", stream.label));
        return {};
    }

    CudaPoolAllocation CudaContext::AllocateAsync(const std::size_t bytes,
                                                  const std::string_view tag)
    {
        for (auto& block : pool_)
        {
            if (!block.inUse && block.capacityBytes >= bytes)
            {
                block.inUse = true;
                executionTrace_.push_back(MakeTraceLine("pool.reuse", std::string(tag)));
                return CudaPoolAllocation{
                    .id = block.id,
                    .bytes = bytes,
                    .poolOffsetBytes = block.offsetBytes,
                    .reusedFromPool = true,
                    .tag = std::string(tag),
                };
            }
        }

        const std::size_t offsetBytes = PoolReservedBytes();
        const std::uint64_t id = nextAllocationId_++;
        pool_.push_back(PoolBlock{
            .id = id,
            .capacityBytes = bytes,
            .offsetBytes = offsetBytes,
            .inUse = true,
        });
        executionTrace_.push_back(MakeTraceLine("pool.allocate", std::string(tag)));
        return CudaPoolAllocation{
            .id = id,
            .bytes = bytes,
            .poolOffsetBytes = offsetBytes,
            .reusedFromPool = false,
            .tag = std::string(tag),
        };
    }

    CudaContextStatus CudaContext::ReleaseAllocation(const CudaPoolAllocation& allocation)
    {
        for (auto& block : pool_)
        {
            if (block.id == allocation.id)
            {
                block.inUse = false;
                executionTrace_.push_back(MakeTraceLine("pool.release", allocation.tag));
                return {};
            }
        }
        return {.ok = false,
                .code = "cuda.pool.unknown_allocation",
                .detail = "ReleaseAllocation received an unknown pool allocation."};
    }

    std::optional<CudaGraphExecutable>
    CudaContext::CaptureGraph(const std::string_view name,
                              const std::vector<std::string>& operations)
    {
        if (!plan_.graph.enableGraphCapture)
        {
            executionTrace_.push_back(MakeTraceLine("graph.capture.skipped", std::string(name)));
            return std::nullopt;
        }

        CudaGraphExecutable executable{
            .id = nextGraphId_++,
            .name = std::string(name),
            .cacheKey = "",
            .persistent = plan_.graph.preferPersistentGraph,
            .warmupIterations = plan_.graph.warmupIterations,
            .operations = operations,
        };
        ++graphCaptureCount_;
        executionTrace_.push_back(MakeTraceLine("graph.capture", executable.name));
        return executable;
    }

    std::optional<CudaGraphLease>
    CudaContext::AcquireSpeculativeGraph(const std::string_view cacheKey,
                                         const std::string_view name,
                                         const std::vector<std::string>& operations)
    {
        if (!plan_.graph.enableGraphCapture)
        {
            executionTrace_.push_back(
                MakeTraceLine("graph.acquire.skipped", std::string(cacheKey)));
            return std::nullopt;
        }

        const auto cacheKeyString = std::string(cacheKey);
        for (auto& entry : graphCache_)
        {
            if (entry.cacheKey == cacheKeyString)
            {
                ++entry.reuseCount;
                ++graphCacheHitCount_;
                executionTrace_.push_back(MakeTraceLine("graph.cache.hit", cacheKeyString));
                return CudaGraphLease{
                    .executable = entry.executable,
                    .reusedExistingGraph = true,
                    .reuseCount = entry.reuseCount,
                };
            }
        }

        const auto executable = CaptureGraph(name, operations);
        if (!executable.has_value())
        {
            return std::nullopt;
        }

        CachedGraphEntry entry{
            .cacheKey = cacheKeyString,
            .executable = *executable,
            .reuseCount = 0U,
        };
        entry.executable.cacheKey = cacheKeyString;
        graphCache_.push_back(entry);
        executionTrace_.push_back(MakeTraceLine("graph.cache.store", cacheKeyString));
        return CudaGraphLease{
            .executable = graphCache_.back().executable,
            .reusedExistingGraph = false,
            .reuseCount = 0U,
        };
    }

    std::vector<std::int32_t> CudaContext::BuildReferenceTokens(const std::uint32_t tokenCount,
                                                                const std::uint32_t seedSalt) const
    {
        const std::uint32_t seed = plan_.request.seed == 0U ? 17U : plan_.request.seed;
        const std::size_t hashedModel = std::hash<std::string>{}(plan_.request.modelId);
        std::vector<std::int32_t> tokens;
        tokens.reserve(tokenCount);

        for (std::uint32_t index = 0U; index < tokenCount; ++index)
        {
            const std::uint32_t mixed =
                static_cast<std::uint32_t>((hashedModel & 0xFFFFU) + seed + seedSalt + index * 13U +
                                           plan_.recommendedBatchSize * 7U);
            tokens.push_back(static_cast<std::int32_t>(1000U + (mixed % 2048U)));
        }

        return tokens;
    }

    TokenChunk CudaContext::ReplayGraph(const CudaGraphExecutable& executable,
                                        const std::uint32_t tokenCount,
                                        const std::uint32_t seedSalt)
    {
        ++graphReplayCount_;
        executionTrace_.push_back(MakeTraceLine("graph.replay", executable.name));

        TokenChunk chunk;
        chunk.tokens = BuildReferenceTokens(tokenCount, seedSalt);
        chunk.isTerminal = false;
        return chunk;
    }

    void CudaContext::ClearGraphCache()
    {
        graphCache_.clear();
        executionTrace_.push_back(MakeTraceLine("graph.cache.clear", "all"));
    }

    std::size_t CudaContext::PoolCapacityBytes() const noexcept
    {
        std::size_t total = 0U;
        for (const auto& block : pool_)
        {
            total += block.capacityBytes;
        }
        return total;
    }

    std::size_t CudaContext::PoolReservedBytes() const noexcept
    {
        std::size_t total = 0U;
        for (const auto& block : pool_)
        {
            total = std::max(total, block.offsetBytes + block.capacityBytes);
        }
        return total;
    }

    std::uint32_t CudaContext::ActiveStreamCount() const noexcept
    {
        return static_cast<std::uint32_t>(activeStreams_.size());
    }

    std::uint32_t CudaContext::CachedGraphCount() const noexcept
    {
        return static_cast<std::uint32_t>(graphCache_.size());
    }

    std::uint32_t CudaContext::GraphCaptureCount() const noexcept
    {
        return graphCaptureCount_;
    }

    std::uint32_t CudaContext::GraphCacheHitCount() const noexcept
    {
        return graphCacheHitCount_;
    }

    std::uint32_t CudaContext::GraphReplayCount() const noexcept
    {
        return graphReplayCount_;
    }

    const std::vector<std::string>& CudaContext::ExecutionTrace() const noexcept
    {
        return executionTrace_;
    }

} // namespace us4::runtime::backends::cuda
