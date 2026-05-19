#pragma once

#include <cstddef>
#include <string>

namespace us4::runtime::backends::cuda
{

    enum class CublasPath
    {
        kCustomKernel,
        kCublasLt,
        kCublasLegacy,
        kHostFallback,
    };

    [[nodiscard]] std::string ToString(CublasPath path);

    struct CublasDispatchRequest
    {
        std::size_t rowsLeft = 0;
        std::size_t colsLeft = 0;
        std::size_t colsRight = 0;
        std::size_t batchSize = 1;
        bool transposeLeft = false;
        bool transposeRight = false;
        bool useFp16Accumulate = false;
    };

    struct CublasDispatchDecision
    {
        CublasPath path = CublasPath::kCustomKernel;
        std::string rationale;
    };

    [[nodiscard]] CublasDispatchDecision SelectCublasPath(const CublasDispatchRequest& request);

} // namespace us4::runtime::backends::cuda
