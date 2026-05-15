# RELEASE - `US4 V6 Windows Edition`

Estado atual do processo de release do repo `us4-v6-simplicio-windows`.

Este documento descreve o que ja existe, o que falta para a primeira release distribuivel e como registrar esse gap sem fingir que a automacao esta pronta.

---

## 1. Estado atual

Hoje o repositorio ainda nao tem pipeline de release do runtime Windows.

Faltam estes blocos basicos:

- [`.github/workflows/release.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/release.yml)
- `CHANGELOG.md`
- `packaging/msix/`
- `packaging/winget/`
- fonte canonica da versao do runtime em C++ (`runtime/version.h` ou geracao equivalente)
- assinatura de binarios
- smoke de pos-publicacao

O que existe hoje e suficiente para validar base de engenharia:

- build CMake em Windows via [`.github/workflows/ci.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/ci.yml)
- `ctest`
- smoke `us4-cli --probe`
- evidencia Playwright quando a PR toca CLI/UX
- gate de corpo de PR e ADR via [`.github/workflows/dod.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/dod.yml)

Conclusao pratica: o repo esta em fase de fundacao e ainda nao deve prometer release automatizada, MSIX assinado ou publicacao via winget.

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

## 3. Processo interim

Enquanto a pipeline de release nao existe, use este processo como preparacao:

### 3.1 Preparar a base

- manter `CI` e `DoD Gate` verdes
- manter o PR template alinhado com o estado real do repo
- registrar gaps de release nas tasks e ADRs quando o desenho mudar

### 3.2 Definir versao

Ainda nao ha uma fonte unica da versao do runtime no codigo C++. Antes da primeira release, escolha e implemente uma destas abordagens:

- gerar header de versao a partir do CMake
- declarar versao no `project(...)` do CMake e propagar para o binario

Nao publique runtime usando apenas a versao de `package.json`, porque esse arquivo hoje cobre tooling herdado do starter, nao o contrato do runtime C++.

### 3.3 Criar changelog

Antes da primeira release, adicione `CHANGELOG.md` em ingles com entradas especificas. Nao use linhas vagas como "various improvements".

### 3.4 Empacotamento

Quando o trabalho de distribuicao comecar, ele deve entrar com arquivos reais no repo. Exemplos esperados:

- `packaging/msix/`
- `packaging/winget/`
- scripts de build e assinatura

Enquanto isso nao existir, documente qualquer entrega manual no corpo da PR ou da issue correspondente.

---

## 4. SemVer

SemVer continua sendo a regra alvo do produto:

- `PATCH`: bug fix compativel
- `MINOR`: feature compativel
- `MAJOR`: quebra de contrato

Mas esse versionamento so passa a valer como processo operacional quando a fonte canonica da versao do runtime estiver implementada.

---

## 5. Checklist para habilitar release automation

- [ ] `CHANGELOG.md` criado
- [ ] versao do runtime definida no build C++
- [ ] `us4-cli --version` implementado e coberto por teste
- [ ] `release.yml` criado
- [ ] `packaging/msix/` criado
- [ ] estrategia de `portable zip` definida
- [ ] assinatura de binarios definida
- [ ] smoke de pos-publicacao definido
- [ ] rollback documentado
- [ ] branch protection atualizada se novos checks forem obrigatorios

---

## 6. Rollback

Hoje rollback e um procedimento manual porque nao ha release pipeline nem publicacao automatica.

Se houver distribuicao manual de binarios antes da pipeline oficial:

- identifique a versao afetada no PR/issue
- preserve o artefato anterior
- registre o incidente em `.specs/incidents/` quando essa pasta existir no processo do time
- adicione teste ou gate que teria evitado o problema

Quando `release.yml` existir, este documento deve ganhar passos concretos de rollback de tag, release e pacote.
