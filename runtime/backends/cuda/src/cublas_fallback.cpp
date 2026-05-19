#include "us4/runtime/backends/cuda/cublas_fallback.h"

#include <sstream>

namespace us4::runtime::backends::cuda
{

    std::string ToString(const CublasPath path)
    {
        switch (path)
        {
            case CublasPath::kCustomKernel:
                return "custom_kernel";
            case CublasPath::kCublasLt:
                return "cublasLt";
            case CublasPath::kCublasLegacy:
                return "cublas_legacy";
            case CublasPath::kHostFallback:
                return "host_fallback";
        }
        return "unknown";
    }

    CublasDispatchDecision SelectCublasPath(const CublasDispatchRequest& request)
    {
        CublasDispatchDecision decision{};
        std::ostringstream reason;
        if (request.batchSize > 1U)
        {
            decision.path = CublasPath::kCublasLt;
            reason << "batch=" << request.batchSize << ";prefer=cublasLt";
        }
        else if (request.colsLeft == 0U || request.colsRight == 0U)
        {
            decision.path = CublasPath::kHostFallback;
            reason << "empty_shape;fallback=host";
        }
        else if (request.useFp16Accumulate && request.rowsLeft >= 256U && request.colsRight >= 256U)
        {
            decision.path = CublasPath::kCublasLt;
            reason << "large_fp16=true;prefer=cublasLt";
        }
        else if (request.rowsLeft < 32U || request.colsRight < 32U)
        {
            decision.path = CublasPath::kCublasLegacy;
            reason << "small_shape;prefer=cublas_legacy";
        }
        else
        {
            decision.path = CublasPath::kCustomKernel;
            reason << "rows=" << request.rowsLeft << ";cols=" << request.colsRight
                   << ";prefer=custom_kernel";
        }
        decision.rationale = reason.str();
        return decision;
    }

} // namespace us4::runtime::backends::cuda
