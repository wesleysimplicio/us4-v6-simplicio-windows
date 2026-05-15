# API Examples

`US4 V6 Windows Edition` ainda nao expoe API HTTP.
O produto, neste estado, e uma CLI nativa + bibliotecas/scaffolds de runtime.

Este diretorio deve guardar exemplos pequenos e sanitizados dos artefatos observaveis da CLI.

## O Que Guardar Aqui

Use este diretorio para exemplos de:

- `probe` output textual
- `run --format json`
- `bench --format json`
- `tune` output textual
- `profiles.json` persistido pelo `ProfileStore`
- correctness reports em JSON
- traces ou logs sanitizados usados em troubleshooting

## Suggested Layout

```text
docs/api-examples/
  bench/
    bench-matrix-qwen-cpu-only.json
  tune/
    tune-qwen-cpu-only.txt
    profiles-qwen-cpu-only.json
  probe/
    probe-directml-host.txt
  correctness/
    hybrid-planner-gate.json
```

## CLI Examples

```powershell
.\build\us4-cli.exe probe
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi" --backend cpu
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi" --backend cpu --format json
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only --format json
.\build\us4-cli.exe tune --model qwen-0.5b --backend cpu --mode cpu-only
```

## Run JSON Example

`run --format json` agora expõe um envelope estruturado para o baseline CPU scalar e para os dry-runs.
O payload esperado inclui pelo menos:

- `execution`
- `status`
- `plan_execution`
- `backend`
- `mode`
- `profile`
- `prompt_chars`
- `prompt_tokens_estimate`
- `report_text`

Em CPU scalar, o payload tambem inclui:

- `generated_tokens`
- `generated_text`
- `generation`
- `kv`
- `prefix_cache`
- `moe`
- `telemetry`
- `checksums`

## Bench JSON Example

`bench --format json` e o contrato JSON mais completo da CLI hoje.
O payload esperado inclui pelo menos:

- `execution`
- `requestedProfileId`
- `recommendedProfileId`
- `selectedProfileId`
- `selectedBackend`
- `selectedScore`
- `persisted`
- `storePath`
- `decisions`
- `samples`

Use arquivos reais gerados pela CLI; nao sintetize exemplos que o binario ainda nao emite.

## Tune Persistence Example

Quando `tune` for exercitado com `US4_PROFILE_STORE_PATH`, guarde tambem o store resultante:

```powershell
$env:US4_PROFILE_STORE_PATH="$PWD\\docs\\api-examples\\tune\\profiles-qwen-cpu-only.json"
.\build\us4-cli.exe tune --model qwen-0.5b --backend cpu --mode cpu-only
```

O arquivo persistido deve seguir o formato atual:

```json
{
  "entries": [
    {
      "fingerprint": "cpu=...;gpu=...;...",
      "profile_id": "cpu-only"
    }
  ]
}
```

## Nao Colocar Aqui

- segredos
- caminhos sensiveis da maquina
- dumps gigantes de benchmark nao sanitizados
- exemplos de flags obsoletas como `--benchmark` ou `--metrics`
