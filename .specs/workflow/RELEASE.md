# RELEASE - `US4 V6 Windows Edition`

Estado atual do processo de release do repo `us4-v6-simplicio-windows`.

Este documento descreve o que ja existe, o que falta para a primeira release distribuivel e como registrar esse gap sem fingir que a automacao esta pronta.

---

## 1. Estado atual

Hoje o repositorio ja tem um scaffold funcional de release do runtime Windows.

Ainda faltam estes blocos basicos para chamar o processo de distribuicao de completo:

- assinatura efetiva com certificado real
- smoke de pos-publicacao em host de instalacao

O que existe hoje e suficiente para validar base de engenharia:

- build CMake em Windows via [`.github/workflows/ci.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/ci.yml)
- `ctest`
- smoke `us4-cli --probe`
- CLI com `version`, `probe`, `run`, `bench` e `tune`
- `release.yml` para tags `v*`
- `CHANGELOG.md`
- `scripts/build-portable-zip.ps1`
- scaffold de `packaging/msix/` + `scripts/build-msix.ps1`
- scaffold de `packaging/winget/` + `scripts/render-winget-manifests.ps1`
- validacao de manifests de `winget` via `scripts/validate-winget-manifests.ps1`
- validacao cruzada de artefatos + checksums + manifests via `scripts/validate-release-assets.ps1`
- `release-manifest.json` gerado via `scripts/render-release-manifest.ps1`
- validacao de tag de release via `scripts/validate-release-tag.ps1`
- `release-notes.md` gerado via `scripts/render-release-notes.ps1`
- scaffold de assinatura via `scripts/sign-msix.ps1`
- scaffold de certificado dev via `scripts/create-dev-signing-cert.ps1`
- `scripts/generate-checksums.ps1`
- smoke local de portable zip via `scripts/post-publish-smoke.ps1`
- smoke local dev-only de `.msix` assinado temporariamente via `scripts/post-publish-smoke.ps1 -EnableDevMsixSmoke`
- preflight de release via `scripts/preflight-release.ps1`
- dry-run local encadeado via `scripts/release-dry-run.ps1`
- snapshot consolidado de projeto/release via `scripts/render-project-status.ps1`
- smoke de instalacao MSIX via `scripts/install-msix-smoke.ps1`
- smoke local dev-only de MSIX autoassinado via `scripts/dev-msix-smoke.ps1`
- validacao de layout publicavel via `scripts/validate-publish-layout.ps1`
- evidencia Playwright quando a PR toca CLI/UX
- gate de corpo de PR e ADR via [`.github/workflows/dod.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/dod.yml)

Conclusao pratica: o repo esta em fase avancada de fechamento da Sprint 12, com release scaffold local e CI de release inicial, mas ainda nao deve prometer MSIX assinado ou publicacao via winget.

---

## 2. O que precisa existir antes da primeira release real

Antes de cortar uma release publica do runtime, o repositorio precisa ganhar pelo menos:

1. um workflow de release dedicado
2. versionamento canonico do runtime
3. `CHANGELOG.md`
4. pipeline de empacotamento
5. estrategia de assinatura
6. smoke de pos-publicacao
7. criterio de rollback documentado

Itens recomendados:

- `release.yml` para tags `v*`
- artefatos `portable zip` e `MSIX`
- checksum SHA256
- publicacao em GitHub Releases
- submissao de manifest quando `winget` entrar de fato no escopo

---

## 3. Estado real da CLI e implicacoes para release

### 3.1 O que a CLI publica hoje

Hoje a superficie publica real do `us4-cli` inclui:

- `help`
- `version`
- `probe`
- `run`
- `bench`
- `tune`

Implicacoes:

- `bench` e `tune` ja sao comandos release-relevant e precisam entrar no smoke de release quando a pipeline existir.
- `bench --format json` ja constitui contrato de saida publica e precisa ser tratado como surface versionada.
- `run --format json` agora tambem constitui contrato de saida publica e precisa ser tratado como surface versionada.
- `tune` escreve estado persistido em `runtime/tuning/profiles.json` ou em `US4_PROFILE_STORE_PATH`; a release precisa documentar esse comportamento.

### 3.2 O que ainda nao esta pronto como contrato final

- a versao do binario ja e derivada da versao do projeto no CMake, mas a trilha de release ainda nao fecha so com isso.
- `serve` ainda e scaffold-only.
- os caminhos `Vulkan` e `Windows ML` ainda tem trechos fortes de planner/dry-run em vez de execucao device-side completa.

### 3.3 Package version x runtime version

Hoje `package.json` existe por causa do tooling herdado do starter e do ecossistema JS do repo. Esse numero nao deve ser tratado sozinho como a versao canonica do runtime C++.

Enquanto a versao do runtime ainda nao estiver conectada a changelog, tag e artefatos finais:

- `package.json` pode acompanhar a trilha do scaffold/tooling
- `us4-cli version` continua util para smoke
- a release do runtime ainda nao esta operacionalmente fechada

---

## 4. Processo interim

Enquanto a pipeline de release nao existe, use este processo como preparacao:

### 4.1 Preparar a base

- manter `CI` e `DoD Gate` verdes
- manter README, setup, evidence e workflow alinhados com o estado real da CLI
- registrar gaps de release nas tasks e ADRs quando o desenho mudar

### 4.2 Definir versao

O runtime ja usa a versao do `project(...)` do CMake para alimentar `us4-cli version`.
O que ainda falta e ligar isso a:

- changelog
- tags de release
- artefatos distribuidos
- assinatura e publicacao

### 4.3 Changelog

`CHANGELOG.md` ja existe e deve continuar sendo atualizado em todo change release-relevant. Nao use linhas vagas como "various improvements".

### 4.4 Empacotamento

Hoje o repo ja inclui:

- `packaging/msix/`
- `packaging/winget/`
- `scripts/build-portable-zip.ps1`
- `scripts/build-msix.ps1`
- `scripts/generate-checksums.ps1`
- `scripts/post-publish-smoke.ps1`
  - cobre smoke do zip publicado e tambem um caminho opt-in dev-only para `.msix`, sem substituir a validacao de instalacao em host real para release publica
- `scripts/release-dry-run.ps1`
  - valida `vX.Y.Z` vs `package.json`, empacota artefatos locais, renderiza manifests/notas e pode expor readiness dev-only de MSIX sem esconder blockers externos de producao
- `scripts/validate-publish-layout.ps1`

O que ainda falta para fechar distribuicao:

- assinatura com certificado real em CI
- smoke de pos-publicacao em host real para MSIX
- politica de rollback operacional

Importante: a trilha `create-dev-signing-cert.ps1` + `dev-msix-smoke.ps1` reduz o gap local de `T12.6`, mas nao muda o blocker de producao. Certificado autoassinado continua insuficiente para release publica.

---

## 5. SemVer

SemVer continua sendo a regra alvo do produto:

- `PATCH`: bug fix compativel
- `MINOR`: feature compativel
- `MAJOR`: quebra de contrato

Mas esse versionamento so passa a valer como processo operacional quando ele estiver ligado aos artefatos finais e ao processo de publicacao.

Mudancas que devem ser tratadas com cuidado de contrato desde ja:

- formato JSON de `bench`
- formato JSON de `run`
- formato textual de `probe`, `run` e `tune`
- local e estrutura do `profiles.json`
- nomes de comandos e flags publicas

---

## 6. Checklist para habilitar release automation

- [x] `CHANGELOG.md` criado
- [x] versao do runtime definida no build C++
- [x] `us4-cli version` alinhado a essa fonte canonica
- [x] smoke de release cobrindo `probe`, `run`, `bench` e `tune`
- [x] `release.yml` criado
- [x] `packaging/msix/` criado
- [x] `packaging/winget/` criado
- [x] estrategia de `portable zip` definida
- [x] assinatura de binarios definida
- [x] smoke de pos-publicacao definido
- [x] rollback documentado
- [ ] branch protection atualizada se novos checks forem obrigatorios

---

## 7. Rollback

Hoje o rollback ainda e semiautomatico, mas ja existe um procedimento concreto para os artefatos atuais:

1. identifique a tag ou commit afetado
2. preserve os checksums e artefatos do ultimo snapshot conhecido como bom
3. remova do draft release os artefatos ruins ou descarte o draft
4. gere novamente `portable zip`, `SHA256SUMS.txt` e, se aplicavel, o MSIX
5. rode `scripts/post-publish-smoke.ps1` contra o zip reconstruido e, se estiver fechando o gap local de MSIX no mesmo host, use `-EnableDevMsixSmoke` no `.msix`
6. se o problema estiver em manifests, rerenderize `packaging/winget/manifests/` com URLs corretos e rode `scripts/validate-winget-manifests.ps1 -RequirePublishableUrls`
7. rode `scripts/validate-release-assets.ps1` para garantir que artefatos, checksums e manifests continuam coerentes
8. rode `scripts/validate-publish-layout.ps1` para garantir que nenhum staging, smoke temporario ou arquivo incidental vazou para o diretorio publicavel
9. registre o incidente em `.specs/incidents/` quando essa pasta existir
10. adicione teste, smoke ou gate que teria evitado a regressao

Se a falha estiver apenas no MSIX:

- mantenha o portable zip como artefato valido
- marque explicitamente a release como bloqueada para distribuicao instalada
- nao publique manifests definitivos de winget apontando para o MSIX ruim

Quando a pipeline de assinatura e publicacao estiver completa, este procedimento deve ganhar rollback de release publicada, tag final e submissao de winget.
