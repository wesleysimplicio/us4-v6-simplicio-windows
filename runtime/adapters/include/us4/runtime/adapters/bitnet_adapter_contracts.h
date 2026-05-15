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

    enum class BitNetKernelStyle
    {
        kReferenceScalar,
        kPackedPopcount,
        kTMacTiles,
    };

    enum class BitNetWeightEncoding
    {
        kBinarySign,
        kOnePointFiveBit,
        kTernaryResidual,
    };

    struct BitNetTileShape
    {
        std::size_t rows = 128;
        std::size_t columns = 128;
        std::size_t depth = 64;
    };

    struct BitNetExecutionHints
    {
        BitNetKernelStyle preferredKernel = BitNetKernelStyle::kReferenceScalar;
        BitNetWeightEncoding weightEncoding = BitNetWeightEncoding::kOnePointFiveBit;
        BitNetTileShape tile{};
        std::size_t groupSize = 128;
        std::size_t packAlignmentBytes = 64;
        float activationClamp = 6.0F;
        bool preferTMac = true;
        bool allowHostDequantFallback = true;
        bool allowDenseFallback = true;
    };

    struct BitNetModelDescriptor
    {
        ModelAssetDescriptor asset;
        BitNetExecutionHints hints;
        std::size_t hiddenSize = 2048;
        std::size_t intermediateSize = 5632;
        std::size_t attentionHeads = 16;
        std::size_t kvHeads = 8;
        std::size_t blockCount = 24;
        std::size_t maxContextTokens = 8192;
        float quantScale = 1.0F;
        bool hasGateBranch = false;
    };

    [[nodiscard]] DenseAdapterConfig MakeBitNetDenseAdapterConfig();
    [[nodiscard]] std::unique_ptr<IUS4WindowsAdapter> CreateBitNetScalarAdapter();

} // namespace us4::runtime::adapters
