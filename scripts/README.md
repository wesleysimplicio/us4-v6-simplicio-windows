# Scripts

Os scripts deste repositorio cobrem dois cenarios:

- validar e atualizar a camada do `agentic-starter` que hoje esta versionada aqui
- preparar e validar o caminho do runtime C++/CLI sem mascarar bloqueios de toolchain

## Scripts atuais

- `start.ps1` / `start.sh`
  - validam toolchain, configuram o build local e falham com mensagem direta quando `cmake`, `ninja` ou compilador nao estao expostos no shell
- `test.ps1` / `test.sh`
  - validam a camada do starter com `npm run test:cli`, `npm run pack:dry`, parse do `bootstrap.ps1` e smoke Playwright quando `us4-cli` ja existe
- `evidence.ps1` / `evidence.sh`
  - rodam Playwright para o `us4-cli`, exigem binario existente, verificam os artefatos gerados e falham se o resultado for so `skip`
- `update-starter.ps1` / `update-starter.sh`
  - atualizam a estrutura instalada do starter de forma segura

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
