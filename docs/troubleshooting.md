# Troubleshooting

## C++ compiler not found

- Symptom: `cmake` falha com `No CMAKE_CXX_COMPILER could be found` ou `scripts/start.ps1` informa que nenhum compilador foi encontrado.
- Diagnose: confirme se o shell atual expoe `cl`, `clang-cl` ou `clang++`.
- Diagnose adicional: `scripts/evidence.ps1` e `scripts/evidence.sh` imprimem um snapshot com `cmake`, `ninja`, `cl`, `clang-cl` e `clang++` quando o binario do CLI nao existe.
- Fix: instale o workload `Desktop development with C++` no Visual Studio e abra um `Developer PowerShell for VS`.

## CMake or Ninja not found

- Symptom: `scripts/start.ps1` falha antes do configure.
- Diagnose: rode `cmake --version` e `ninja --version`.
- Fix: abra uma shell do Visual Studio ou adicione CMake/Ninja ao `PATH`.

## CUDA not detected

- Symptom: o probe escolhe `directml`, `vulkan` ou `cpu-avx2` em uma maquina NVIDIA.
- Diagnose: confira `nvidia-smi`, driver e toolkit instalado.
- Fix: atualize driver/toolkit ou use overrides como `US4_HAS_CUDA=1` enquanto o probe estiver em scaffold.

## DirectML unavailable on AMD or Intel

- Symptom: o probe cai para Vulkan ou CPU em hardware que deveria priorizar DirectML.
- Diagnose: valide Windows atualizado, driver WDDM e suporte D3D12.
- Fix: atualize o driver grafico; se a stack nao suportar DirectML no host, mantenha o fallback seguro.

## Vulkan SDK missing

- Symptom: paths Vulkan nao configuram ou falham ao expandir alem do scaffold.
- Diagnose: verifique headers, libs e variaveis do Vulkan SDK.
- Fix: instale o Vulkan SDK e rode o configure novamente.

## Playwright skips CLI smoke

- Symptom: `npx playwright test --project=cli` retorna `skip` nos testes.
- Diagnose: o binario `build/us4-cli.exe` ainda nao existe ou `US4_CLI_PATH` aponta para caminho invalido.
- Fix: compile `us4-cli` antes do E2E ou defina `US4_CLI_PATH` com um executavel valido.
- Important: `skip` nao conta como evidencia valida de CLI/UX. Use `scripts/evidence.ps1` ou `scripts/evidence.sh`; eles falham se o resultado for so teste pulado.

## PowerShell completions do not load

- Symptom: o tab completion de `us4-cli` nao funciona mesmo apos rodar `scripts/install-completions.ps1`.
- Diagnose: inspecione o arquivo atual de `$PROFILE` e confirme se ele contem uma linha que faz dot-source de `scripts/completions/us4-cli.ps1`.
- Diagnose adicional: reabra o shell ou execute manualmente o profile atual depois da instalacao.
- Fix: rode `.\scripts\install-completions.ps1` novamente; o instalador e idempotente e nao deve duplicar a entrada.

## MakeAppx not found for MSIX packaging

- Symptom: `scripts/build-msix.ps1` falha com `MakeAppx.exe not found`.
- Diagnose: rode `Get-Command MakeAppx.exe`.
- Fix: instale as ferramentas de App Packaging do Windows SDK e rode novamente `.\scripts\build-msix.ps1 -BuildDir build -OutputDir dist`.
- Important: o empacotamento portable zip nao depende de `MakeAppx.exe`; use `scripts/build-portable-zip.ps1` enquanto esse prerequisito nao estiver disponivel.

## MSIX signing prerequisites missing

- Symptom: `scripts/sign-msix.ps1` falha dizendo que a configuracao do certificado esta ausente.
- Diagnose: confirme se `US4_SIGN_CERT_PASSWORD` esta definido e se voce forneceu `US4_SIGN_CERT_PATH` ou `US4_SIGN_CERT_BASE64`.
- Fix: injete esses valores no ambiente local ou no CI antes de rodar a etapa de assinatura.
- Important: sem assinatura, o scaffold de release continua util para zip portatil, checksums e manifests, mas nao fecha distribuicao instalada.

## Release preflight reports blocked status

- Symptom: `scripts/preflight-release.ps1` retorna `blocked`.
- Diagnose: leia `issue_codes` no JSON ou no output textual.
- Fix: corrija o item indicado, como mismatch de versao, entrada ausente no `CHANGELOG`, build faltando ou configuracao de assinatura ausente.

## MSIX install smoke fails before installation

- Symptom: `scripts/install-msix-smoke.ps1` falha antes de chamar `Add-AppxPackage`.
- Diagnose: verifique se o pacote esta assinado e se o certificado e confiavel no host atual.
- Fix: assine o `.msix`, importe a trust chain necessaria no host de validacao e rode o smoke novamente.
- Important: esse smoke foi feito para host apto a instalacao; ele deve falhar explicitamente quando o pacote ainda esta em estado de scaffold.

## winget manifests still contain placeholder URLs

- Symptom: os manifests renderizados em `packaging/winget/manifests/` ainda apontam para `example.invalid`.
- Diagnose: confira os argumentos usados em `scripts/render-winget-manifests.ps1`.
- Fix: rerenderize com os URLs reais do zip e do MSIX publicados.
- Important: o scaffold de `winget` e versionado no repo, mas a publicacao real ainda depende dos artefatos assinados e dos URLs finais.

## Build artifacts locked

- Symptom: compilador nao consegue sobrescrever artefatos em `build/`.
- Diagnose: `us4-cli`, `ctest` ou outra ferramenta pode estar segurando o arquivo.
- Fix: encerre processos em execucao, limpe `build/` se necessario e recompilhe.

## Correctness drift by backend

- Symptom: unit tests passam, mas benchmark/correctness diverge entre CUDA, DirectML, Vulkan ou CPU.
- Diagnose: compare seed, prompt, budget e modo de precisao.
- Fix: desative a otimizacao nova, isole a regressao por backend e reintroduza com evidencia de benchmark e logit diff.
