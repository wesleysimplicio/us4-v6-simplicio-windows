#pragma once

#include "us4/runtime/adapters/dense_adapter_base.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::adapters
{

    enum class TernaryKernelStyle
    {
        kReferenceScalar,
        kNibblePack,
        kTMacTiles,
    };

    enum class TernaryActivationMode
    {
        kFp16Reference,
        kInt8Symmetric,
        kInt4Probe,
    };

    struct TernaryTileShape
    {
        std::size_t rows = 128;
        std::size_t columns = 256;
        std::size_t depth = 64;
    };

    struct TernaryExecutionHints
    {
        TernaryKernelStyle preferredKernel = TernaryKernelStyle::kReferenceScalar;
        TernaryActivationMode activationMode = TernaryActivationMode::kFp16Reference;
        TernaryTileShape tile{};
        std::size_t groupSize = 128;
        std::size_t packAlignmentBytes = 64;
        float activationClamp = 8.0F;
        bool useSignedZeroEncoding = false;
        bool preferTMac = true;
        bool allowDenseFallback = true;
    };

    struct TernaryModelDescriptor
    {
        ModelAssetDescriptor asset;
        TernaryExecutionHints hints;
        std::size_t hiddenSize = 4096;
        std::size_t intermediateSize = 11008;
        std::size_t attentionHeads = 32;
        std::size_t kvHeads = 8;
        std::size_t blockCount = 32;
        std::size_t maxContextTokens = 16384;
        float quantScale = 1.0F;
        bool usesGroupedExperts = false;
    };

    [[nodiscard]] DenseAdapterConfig MakeTernaryDenseAdapterConfig();
    [[nodiscard]] std::unique_ptr<IUS4WindowsAdapter> CreateTernaryScalarAdapter();

} // namespace us4::runtime::adapters
