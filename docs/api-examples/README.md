# API Examples

`US4 V6 Windows Edition` não expõe API HTTP neste estágio.
O produto é planejado como CLI nativa + DLL Windows.

## O que guardar aqui

Use este diretório para exemplos pequenos e sanitizados de:

- payloads JSON emitidos por `--metrics`
- traces JSONL gerados por `--trace-file`
- configs de hardware/model profiles
- exemplos de saída de benchmark

## Suggested layout

```text
docs/api-examples/
  metrics/
    probe-output.json
    benchmark-summary.json
  profiles/
    hardware-profile.json
    model-profile.json
```

## CLI examples

```powershell
.\build\us4-cli.exe --probe
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi"
.\build\us4-cli.exe --benchmark --model models\qwen --backend auto --metrics
```
