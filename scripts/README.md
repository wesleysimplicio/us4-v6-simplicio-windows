# Scripts

Os scripts deste repositorio cobrem dois cenarios:

- validar e atualizar a camada do `agentic-starter` que hoje esta versionada aqui
- preparar e validar o caminho do runtime C++/CLI sem mascarar bloqueios de toolchain

## Scripts atuais

- `start.ps1` / `start.sh`
  - validam toolchain, configuram o build local e falham com mensagem direta quando `cmake`, `ninja` ou compilador nao estao expostos no shell
- `test.ps1` / `test.sh`
  - validam a camada do starter com `npm run test:cli`, `npm run pack:dry`, parse do `bootstrap.ps1`, reconfiguram/rebuildam `build/` e rodam `ctest` + smoke Playwright quando `us4-cli` ja existe
- `evidence.ps1` / `evidence.sh`
  - rodam Playwright para o `us4-cli`, exigem binario existente, verificam os artefatos gerados e falham se o resultado for so `skip`
- `update-starter.ps1` / `update-starter.sh`
  - atualizam a estrutura instalada do starter de forma segura
- `install-completions.ps1`
  - instala o script de completions do `us4-cli` no profile atual do PowerShell
- `build-portable-zip.ps1`
  - monta um zip portatil minimo com `us4-cli.exe`, `README` e `CHANGELOG`
- `build-msix.ps1`
  - tenta montar um MSIX nao assinado usando `MakeAppx.exe` quando as tooling de packaging do Windows SDK estiverem disponiveis
- `generate-checksums.ps1`
  - gera `SHA256SUMS.txt` para os artefatos de release em `dist/`
- `install-msix-smoke.ps1`
  - valida assinatura/trust e tenta instalar um `.msix` em host Windows apto, falhando claramente quando o pacote ainda nao e instalavel
- `post-publish-smoke.ps1`
  - executa um smoke local de artefato publicado; hoje cobre o portable zip e falha claramente para MSIX nao automatizado
- `preflight-release.ps1`
  - valida readiness de release cruzando versao, changelog, build, ferramentas de packaging e configuracao de assinatura
- `render-winget-manifests.ps1`
  - renderiza manifests de `winget` a partir dos templates versionados em `packaging/winget/templates/`
- `sign-msix.ps1`
  - assina um `.msix` usando `signtool.exe` e configuracao de certificado via variaveis de ambiente ou caminho explicito

## Regras

- falhar com exit code diferente de zero em erro real
- imprimir proximo passo claro quando um pre-requisito estiver ausente
- tratar `skip` por falta de toolchain como bloqueio, nao como sucesso
- nunca embutir segredo
- preferir variaveis de ambiente para customizacoes

## Update command

```powershell
.\scripts\update-starter.ps1
```

Usar clone local do starter:

```powershell
$env:AGENTIC_STARTER_SOURCE="C:\Users\you\source\repos\agentic-starter"
.\scripts\update-starter.ps1
```
