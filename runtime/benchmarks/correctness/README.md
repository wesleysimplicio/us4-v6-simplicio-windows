# Correctness Gate

Este diretório é o ponto de integração para checks de logit diff entre backends.

## Escopo esperado
- `reference/`: logits ou tokens de referência por modelo e perfil.
- `reports/`: saídas comparativas geradas na validação.
- tolerâncias por tarefa ou por backend, sem afrouxar o gate silenciosamente.

## Regras
- mudanças de tolerância exigem revisão explícita;
- speculative decoding deve produzir resultado idêntico ao baseline;
- qualquer backend novo precisa plugar aqui antes de ser promovido a primário;
- mudanças em `RuntimeMode`, KV tiering, MoE routing ou NPU opt-in precisam anexar evidência daqui.
