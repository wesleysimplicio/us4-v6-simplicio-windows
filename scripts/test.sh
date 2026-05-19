#!/usr/bin/env bash
set -euo pipefail

echo "Running current repository validation"

npm run test:cli
npm run pack:dry

if [[ -f CMakeLists.txt && -d build ]]; then
  if command -v cmake >/dev/null 2>&1 && command -v ctest >/dev/null 2>&1; then
    echo "Detected runtime scaffold/build. Refreshing CMake configuration and rebuilding before CTest."
    cmake -S . -B build
    cmake --build build -j 8
    echo "Running CTest on the rebuilt runtime artifacts."
    ctest --test-dir build --output-on-failure
  else
    echo "Build directory found, but cmake/ctest is not available in PATH. Skipping runtime rebuild and test invocation."
  fi
fi

if [[ -f build/runtime/benchmarks/correctness_diff.exe ]]; then
  echo "Detected correctness gate binary. Running CPU scalar correctness validation."
  build/runtime/benchmarks/correctness_diff.exe
fi

if [[ -f build/runtime/benchmarks/hybrid_planner_gate.exe ]]; then
  echo "Detected hybrid planner gate binary. Running Vulkan/Windows ML correctness validation."
  build/runtime/benchmarks/hybrid_planner_gate.exe
fi

if [[ -f build/runtime/benchmarks/cpu_block_gemm_bench.exe ]]; then
  echo "Detected CPU block GEMM benchmark binary. Running oneDNN vs scalar local benchmark evidence."
  build/runtime/benchmarks/cpu_block_gemm_bench.exe
fi

if [[ -f build/runtime/benchmarks/cpu_attention_bench.exe ]]; then
  echo "Detected CPU attention benchmark binary. Running AVX fused-attention local benchmark evidence."
  build/runtime/benchmarks/cpu_attention_bench.exe
fi

if [[ -f build/runtime/benchmarks/cpu_bitnet_bench.exe ]]; then
  echo "Detected CPU BitNet benchmark binary. Running AVX2 BitNet local benchmark evidence."
  build/runtime/benchmarks/cpu_bitnet_bench.exe
fi

if [[ -f build/runtime/benchmarks/cuda_fallback_policy_bench.exe ]]; then
  echo "Detected CUDA fallback policy benchmark binary. Running custom-vs-library policy evidence."
  build/runtime/benchmarks/cuda_fallback_policy_bench.exe
fi

if [[ -f build/runtime/benchmarks/cuda_graph_reuse_bench.exe ]]; then
  echo "Detected CUDA graph reuse benchmark binary. Running reset-vs-reuse speculative evidence."
  build/runtime/benchmarks/cuda_graph_reuse_bench.exe
fi

if [[ -f build/runtime/benchmarks/cuda_kernel_correctness_bench.exe ]]; then
  echo "Detected CUDA kernel correctness benchmark binary. Running FP16/BF16 diff evidence."
  build/runtime/benchmarks/cuda_kernel_correctness_bench.exe
fi

if [[ -f build/runtime/benchmarks/cuda_bitnet_bench.exe ]]; then
  echo "Detected CUDA BitNet benchmark binary. Running packed 1.58-bit local benchmark evidence."
  build/runtime/benchmarks/cuda_bitnet_bench.exe
fi

if [[ -d build ]] && command -v npx >/dev/null 2>&1; then
  if [[ -n "${US4_CLI_PATH:-}" ]]; then
    CLI_PATH="$US4_CLI_PATH"
  elif [[ -f "$(pwd)/build/us4-cli.exe" ]]; then
    CLI_PATH="$(pwd)/build/us4-cli.exe"
  else
    CLI_PATH="$(pwd)/build/runtime/cli/us4-cli.exe"
  fi
  if [[ -f "$CLI_PATH" ]]; then
    echo "Detected us4-cli binary. Running CLI Playwright smoke validation."
    npx playwright test --project=cli --reporter=list
  else
    echo "Build directory found, but us4-cli binary is missing. Skipping CLI Playwright smoke."
  fi
fi
