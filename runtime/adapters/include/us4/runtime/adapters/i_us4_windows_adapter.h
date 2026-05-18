#pragma once

#include "us4/runtime/adapters/adapter_contracts.h"
#include "us4/runtime/backends/runtime_types.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::adapters
{

    class IUS4WindowsAdapter
    {
      public:
        virtual ~IUS4WindowsAdapter() = default;

        [[nodiscard]] virtual AdapterDescriptor Describe() const = 0;
        [[nodiscard]] virtual backends::RuntimeStatus Status() const = 0;
        [[nodiscard]] virtual AdapterHealth Health() const = 0;
        [[nodiscard]] virtual std::optional<RuntimeBinding> Binding() const = 0;
        [[nodiscard]] virtual bool CanServe(const backends::SessionRequest& request) const = 0;
        [[nodiscard]] virtual ModelResidencyPlan
        BuildResidencyPlan(const backends::SessionRequest& request) const = 0;
        virtual bool Attach(RuntimeBinding binding) = 0;
        virtual bool LoadModel(const std::string& modelPath) = 0;
        [[nodiscard]] virtual backends::TokenChunk Prefill(const std::string& prompt) = 0;
        [[nodiscard]] virtual backends::TokenChunk
        Generate(const backends::SessionRequest& request) = 0;
        [[nodiscard]] virtual bool AppendKvCache(const std::string& sequenceId,
                                                 std::size_t appendedTokens,
                                                 std::size_t appendedBytes,
                                                 KvCacheTier preferredTier,
                                                 bool pinned = false) = 0;
        [[nodiscard]] virtual std::optional<KvCacheEntry>
        LookupKvCache(const std::string& sequenceId) = 0;
        [[nodiscard]] virtual bool EvictKvCache(const std::string& sequenceId,
                                                KvCacheTier targetTier) = 0;
        [[nodiscard]] virtual bool SummarizeKvCache(const std::string& sequenceId,
                                                    std::size_t retainedTokens,
                                                    std::size_t retainedBytes) = 0;
        [[nodiscard]] virtual KvHookSnapshot KvHooks() const = 0;
        virtual void ResetKvCache() = 0;
        [[nodiscard]] virtual GenerationFrame LastGenerationFrame() const = 0;
        virtual void Reset() = 0;
    };

    std::unique_ptr<IUS4WindowsAdapter> CreateNullAdapter();

} // namespace us4::runtime::adapters
