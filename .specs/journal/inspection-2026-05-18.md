# Inspection - 2026-05-18

## Stack real

- Product: US4 V6 Windows Edition
- Stack: C++20 + CMake + Ninja + CUDA + DirectML + Vulkan + oneDNN + Windows ML + Playwright
- Package manager: npm
- Project mode: root

## Commands uteis

- Install: `npm install`
- Dev: `cmake --build build --target us4-cli`
- Lint: `clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h')`
- Test: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1`
- Build: `cmake --build build -j 8`
- Evidence: `npx playwright test --project=cli --reporter=list,html`

## Estrutura de pastas

- `.agents`
- `.claude`
- `.codex`
- `.github`
- `.skills`
- `.specs`
- `bin`
- `docs`
- `packaging`
- `profiles`
- `runtime`
- `scripts`
- `tests`

## Entidades detectadas

- `HardwareProbe`
- `BackendSelector`
- `RuntimeContext`
- `MatrixRunner`
- `AutoTuner`
- `KvPager`
- `PrefixCache`
- `ExpertPager`
- `WinMlAdapter`
- `VulkanContext`
- `IUS4WindowsAdapter`

## Integracoes

- CUDA Toolkit / NVIDIA driver runtime
- DirectML / D3D12
- Vulkan compute
- oneDNN
- Windows ML / NPU opt-in
- GoogleTest / CTest
- Playwright CLI smoke
- GitHub Actions
- LLM Project Mapper overlay tooling

## TODOs encontrados

- `.specs/architecture/PATTERNS.md` ainda reforca a regra de nao deixar TODO sem dono
- Sprint 03 ainda tem `CudaContext`, kernels CUDA e wrappers `cuBLAS/cuBLASLt` em aberto
- Sprint 04 ainda depende de host `AVX-512` e `AMX` reais para fechar as issues restantes
- Sprint 11 ainda precisa aprofundar execucao device-side real em Vulkan e Windows ML
- Sprint 12 ainda depende de certificado de producao e publicacao real da release
