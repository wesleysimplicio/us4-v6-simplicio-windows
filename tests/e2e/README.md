# E2E Tests

Os testes Playwright deste repositorio sao voltados ao `us4-cli`.

Fluxo atual:

- build do executavel `us4-cli`
- execucao de `--help`
- execucao de `--probe`
- execucao real do `run --backend cpu`
- dry-run de `DirectML`
- dry-run de `CUDA`
- dry-run de `Vulkan`
- dry-run de `Windows ML / NPU`
- dry-run de fallback de `Windows ML` sem opt-in
- anexos de stdout/stderr em `test-results/`
- anexo `cli-diagnostics` com caminho resolvido do binario e overrides relevantes
- relatorio HTML em `playwright-report/`

## Como rodar

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target us4-cli
npx playwright test --project=cli
```

Override opcional do binario:

```powershell
$env:US4_CLI_PATH="C:\abs\path\to\us4-cli.exe"
npx playwright test --project=cli
```

Overrides uteis para o probe scaffold:

- `US4_HAS_CUDA=1`
- `US4_HAS_DIRECTML=1`
- `US4_HAS_VULKAN=1`
- `US4_HAS_NPU=1`
- `US4_CPU_NAME=<nome>`
- `US4_GPU_NAME=<nome>`

## Regra de evidencia

`skip` por falta de binario nao conta como evidencia valida de CLI/UX.

Para fechar uma task que toca CLI:

- `build/us4-cli(.exe)` precisa existir
- `npx playwright test --project=cli` precisa executar os specs, nao so pular
- `playwright-report/index.html` precisa ser gerado
- `test-results/results.json` precisa registrar pelo menos um teste executado
- `test-results/` precisa conter anexos com stdout/stderr e `cli-diagnostics`

Se o binario ainda nao existir, os testes fazem `skip` explicito com instrucao de build, e `scripts/evidence.*` falham para deixar claro que a evidencia ainda nao esta completa.
