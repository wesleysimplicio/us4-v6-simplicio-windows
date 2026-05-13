# RELEASE — `US4 V6 Windows Edition`

Processo para cortar release de **US4 V6 Windows Edition** (`us4-v6-simplicio-windows`). Releases são tagueadas, automatizadas via GitHub Actions, assinadas (Authenticode EV) e reversíveis. Dono do processo: `us4-core`. Stack: C++17/20 + CMake/Ninja + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + GoogleTest + Playwright + Ralph Loop.

---

## 1. Princípios

- **SemVer estrito.** `MAJOR.MINOR.PATCH`. Quebra contrato = MAJOR, feature compatível = MINOR, fix compatível = PATCH.
- **Tag é fonte de verdade.** Sem tag, sem deploy de produção.
- **CHANGELOG é contrato com o usuário.** Toda release tem entrada lida e revisada.
- **Rollback em minutos.** Toda release tem caminho documentado de volta (winget manifest reverte, GitHub Release marca anterior como `latest`, MSIX velho fica disponível).
- **Correctness é gate de release.** Logit diff regressivo bloqueia release até resolver ou aprovar via ADR.
- **Multi-backend é gate.** Release valida CUDA + DirectML + Vulkan + CPU AVX num smoke. Falha em um backend = bloqueia.

---

## 2. Bump de versão (SemVer)

Critério rápido:

| Mudança | Bump |
|---------|------|
| Bug fix interno, sem mudar API/CLI/correctness | PATCH (`1.4.2` -> `1.4.3`) |
| Backend novo atrás de flag (GA), adapter novo, CLI flag retrocompatível | MINOR (`1.4.2` -> `1.5.0`) |
| Quebra de API C++ exportada, mudança em `.us4` format, CLI breaking, mudança em seletor de backend | MAJOR (`1.4.2` -> `2.0.0`) |
| Pre-release, RC | sufixo (`1.5.0-rc.1`) |

Local do número de versão:
- `CMakeLists.txt`: `project(us4 VERSION 1.5.0 LANGUAGES CXX CUDA)`.
- `runtime/version.h`: `constexpr auto kUs4Version = "1.5.0";` (gerado por CMake).
- `packaging/msix/AppxManifest.xml`: `<Identity Version="1.5.0.0" .../>` (Windows requer 4 dígitos).
- `packaging/winget/wesleysimplicio.us4.yaml`: `PackageVersion: 1.5.0`.

Bump:

```powershell
git add CMakeLists.txt packaging/msix/AppxManifest.xml packaging/winget/wesleysimplicio.us4.yaml CHANGELOG.md
git commit -m "chore(release): bump to 1.5.0"
```

---

## 3. Atualizar `CHANGELOG.md`

Formato Keep a Changelog. Toda release tem bloco com seções abaixo (omita as vazias):

```markdown
## [1.5.0] - 2026-05-07

### Added
- CUDA FP16 GEMM kernel with epilogue fusion (sprint-04 / T04.3).
- NPU opt-in path via Windows ML (`--npu`) for Copilot+ PCs.
- DirectML backend now supports Intel Arc + AMD RDNA3.

### Changed
- BackendSelector priority on Intel iGPU now prefers DirectML over Vulkan (ADR-009).
- AVX-512 path enabled by default on Sapphire Rapids+.

### Fixed
- VRAM leak on cuStream destruction (#187).
- Logit diff regression on Vulkan FP16 attention for prompt > 4096 tokens (#192).
- MSIX installer silent-fail on Windows 11 22H2 missing VC++ redist (#195).

### Removed
- Legacy `--gpu nvidia` flag (deprecated em v1.3, use `--backend cuda`).

### Security
- Bump `safetensors-cpp` from 0.2.1 to 0.2.4 (CVE-2026-0142, OOB read no GGUF loader).
- Authenticode timestamp server migrado para `timestamp.digicert.com` (SHA-1 deprecated).
```

Regras:
- PT-BR no chat, **CHANGELOG sempre em inglês** (face pública do repo).
- Sem entrada genérica tipo "various improvements". Específico ou nada.
- `Security` ganha destaque, com CVE/advisory linkado.
- Entrada referencia task (sprint-XX/TXX.Y) ou PR (#numero).
- Logit-diff regressions resolvidas vão em `Fixed` com link pro ADR/issue.
- Mudanças que afetam backend específico mencionam o backend explicitamente.

---

## 4. Criar tag

Após bump e CHANGELOG mergeados em `main`:

```powershell
git checkout main
git pull --rebase origin main

Select-String -Path CMakeLists.txt -Pattern 'VERSION '
Get-Content CHANGELOG.md -TotalCount 20

git tag -a v1.5.0 -m "Release 1.5.0"
git push origin v1.5.0
```

Tag deve apontar pro commit em que CHANGELOG e version foram atualizados. Não tag em commit antigo.

> Tag é imutável. Errou? Cria nova patch (`v1.5.1`) com correção. Nunca delete e re-cria tag publicada.

---

## 5. Deploy automático via GitHub Actions

Push da tag dispara `.github/workflows/release.yml`:

```yaml
on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  release:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
      - name: Setup MSVC toolchain
        uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x64
      - name: Install CUDA Toolkit
        uses: Jimver/cuda-toolkit@v0.2.15
        with:
          cuda: '12.5.0'
      - name: Install Vulkan SDK
        uses: humbletim/install-vulkan-sdk@v1.1.1
        with:
          version: 1.3.275.0
      - name: Build Release (x64)
        shell: pwsh
        run: |
          cmake -S . -B build -G Ninja `
            -DCMAKE_BUILD_TYPE=Release `
            -DUS4_ENABLE_CUDA=ON `
            -DUS4_ENABLE_DIRECTML=ON `
            -DUS4_ENABLE_VULKAN=ON `
            -DUS4_ENABLE_ONEDNN=ON `
            -DUS4_ENABLE_NPU=ON
          cmake --build build -j $env:NUMBER_OF_PROCESSORS
      - name: Run DoD gate (unit + regression + correctness + e2e)
        shell: pwsh
        run: |
          ctest --test-dir build --output-on-failure
          npx playwright install --with-deps
          npx playwright test
      - name: Sign binaries (Authenticode)
        shell: pwsh
        env:
          AUTHENTICODE_THUMBPRINT: ${{ secrets.AUTHENTICODE_THUMBPRINT }}
        run: |
          signtool sign /fd SHA256 /sha1 $env:AUTHENTICODE_THUMBPRINT `
            /tr http://timestamp.digicert.com /td SHA256 `
            build\us4-cli.exe build\runtime\us4-runtime.dll
      - name: Build MSIX installer
        shell: pwsh
        run: .\packaging\msix\build-msix.ps1 -Version ${{ github.ref_name }}
      - name: Sign MSIX
        shell: pwsh
        env:
          AUTHENTICODE_THUMBPRINT: ${{ secrets.AUTHENTICODE_THUMBPRINT }}
        run: |
          signtool sign /fd SHA256 /sha1 $env:AUTHENTICODE_THUMBPRINT `
            /tr http://timestamp.digicert.com /td SHA256 `
            us4-${{ github.ref_name }}.msix
      - name: Build portable zip
        shell: pwsh
        run: |
          Compress-Archive -Path build\us4-cli.exe, build\runtime\us4-runtime.dll `
            -DestinationPath us4-v6-windows-${{ github.ref_name }}-portable.zip
          Get-FileHash us4-${{ github.ref_name }}.msix -Algorithm SHA256 > checksum.txt
          Get-FileHash us4-v6-windows-${{ github.ref_name }}-portable.zip -Algorithm SHA256 >> checksum.txt
      - name: Publish GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          files: |
            us4-${{ github.ref_name }}.msix
            us4-v6-windows-${{ github.ref_name }}-portable.zip
            checksum.txt
          body_path: CHANGELOG.md
          draft: false
          prerelease: ${{ contains(github.ref_name, '-rc.') }}
      - name: Submit winget manifest
        shell: pwsh
        env:
          WINGET_TOKEN: ${{ secrets.WINGET_PAT }}
        run: .\packaging\winget\submit-manifest.ps1 -Version ${{ github.ref_name }} -Token $env:WINGET_TOKEN
      - name: Smoke test pos-publish (multi-backend)
        shell: pwsh
        run: |
          winget install --id wesleysimplicio.us4 --version ${{ github.ref_name }}
          us4-cli --version | Select-String ${{ github.ref_name }}
          us4-cli --probe
          us4-cli run --model qwen-0.5b --prompt hi --backend cuda
          us4-cli run --model qwen-0.5b --prompt hi --backend directml
          us4-cli run --model qwen-0.5b --prompt hi --backend cpu
      - name: Notify
        shell: pwsh
        run: .\.github\scripts\notify-release.ps1 ${{ github.ref_name }}
```

Acompanhar o run:

```powershell
gh run watch
gh run list --workflow=release.yml --limit 5
```

Falhou? Workflow é idempotente, pode re-rodar. Se assinatura falhou, é certificado/thumbprint errado — checa `gh secret list` antes de re-rodar. Se um backend specific falhou no smoke (CUDA ok, DirectML não), bloqueia release até resolver.

---

## 6. Smoke tests pós-deploy

Cenários críticos rodando contra a release publicada. Detectar regressão grande em < 5min.

Cobertura mínima por backend:
- `us4-cli --version` retorna a versão nova.
- `us4-cli --probe` detecta hardware (CPU vendor + AVX nível, GPU NVIDIA/AMD/Intel, NPU se Copilot+ PC).
- `us4-cli run --model qwen-0.5b --prompt "hi" --backend <X>` gera >= 5 tokens em <= 60s para cada backend disponível (cuda/directml/vulkan/cpu/npu).
- Correctness diff em modelo de smoke (`qwen-0.5b`, 16 tokens) dentro de 1e-3 **por backend**.
- MSIX install limpo em Windows 11 22H2 + Windows 11 24H2 funciona.
- winget install + uninstall + reinstall ciclo limpo.

Smoke roda dentro do workflow `release.yml` em runner `windows-2022` + self-hosted GPU runner (matriz de hardware). Falha = não publica como `latest`, mantém release anterior.

---

## 7. Rollback

Quando: smoke falhou, usuário reportou crash/correctness regressivo, métrica spikou.

### Estratégia A — Marcar release anterior como `latest`

```powershell
gh release edit v1.5.0 --prerelease
gh release edit v1.4.2 --latest
```

### Estratégia B — Reverter winget manifest

```powershell
cd C:\dev\winget-pkgs
git revert <commit-do-manifest-v1.5.0>
git push origin master
# abre PR no microsoft/winget-pkgs para revert
```

Usuários que ainda não atualizaram caem no MSIX velho via `winget upgrade`.

### Marca a release ruim

```powershell
gh release edit v1.5.0 --notes "ROLLED BACK - see incident #INC-2026-05-07"
```

CHANGELOG ganha nota:

```markdown
## [1.5.0] - 2026-05-07 [ROLLED BACK]
> Rolled back at 14:32 UTC. See incident report INC-2026-05-07.
```

### Pós-rollback

- Postmortem em `.specs/incidents/INC-YYYY-MM-DD.md` em até 48h.
- Fix vai em PR normal (com teste regressivo + correctness diff **no backend afetado**) e tagueia próxima patch (`v1.5.1`).
- Adiciona regression test ou correctness fixture que teria pego o bug.
- Se causa veio de gap arquitetural (ex: BackendSelector decisão errada), abre ADR.
- Se causa veio de gap em matriz de hardware (ex: testou só em RTX, quebrou em RDNA3), adiciona o hardware na CI self-hosted runner pool.
- Atualiza skill/playbook se causa-raiz era processo, não código.

---

## 8. Pre-releases e RCs

Para mudanças grandes (MAJOR) ou backend novo atingindo GA, considere RC:

```powershell
git tag v2.0.0-rc.1
git push origin v2.0.0-rc.1
```

- Workflow `release.yml` detecta sufixo `-rc.` e publica como `prerelease: true`.
- Não submete pro winget-pkgs principal. Manifest separado `wesleysimplicio.us4.RC` opcional.
- Beta testers usam por 3-7 dias antes do tag final `v2.0.0`.
- Bugs em RC viram patch no RC (`v2.0.0-rc.2`), não em PATCH SemVer ainda.

---

## 9. Checklist do release manager

- [ ] `main` verde (build, format, lint, unit, regression POR BACKEND, correctness POR BACKEND, e2e).
- [ ] Versão bumpada em `CMakeLists.txt` + `AppxManifest.xml` + winget manifest conforme SemVer.
- [ ] `CHANGELOG.md` atualizado, revisado, em inglês, com tasks/PRs linkados, mencionando backends afetados.
- [ ] Tag criada apontando pro commit certo.
- [ ] Workflow `release.yml` completou verde.
- [ ] Binário + MSIX assinados (Authenticode EV + timestamp).
- [ ] Smoke tests passaram em TODOS os backends disponíveis (cuda/directml/vulkan/cpu, + npu se Copilot+ PC).
- [ ] winget manifest submetido e merged em `microsoft/winget-pkgs`.
- [ ] Métricas estáveis nos primeiros 30min (issues abertos, winget install reports, Sentry).
- [ ] Notificação pra `us4-core` enviada.
- [ ] Release notes publicadas (`gh release create v1.5.0 -F CHANGELOG.md`).

Em incidente, congelar releases até postmortem fechar com ação concreta no roadmap.
