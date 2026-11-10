# Proposal

## Why

O usuário quer testar se a cadeia ECU → BSI → painel reage a pedidos de lâmpada, e também poder zerar o `MIL_REQUEST` (luz de injeção) que a ECU dele envia aceso, sem mexer na ECU (ele não consegue alterar o que ela envia). Ambos estão no quadro `0x488` (ENGINE_DATA, 100 ms, enviado pela ECU à BSI): bit 4.1 `PREHEAT_LAMP_REQUEST` (lâmpada de velas) e bit 4.2 `MIL_REQUEST` (luz de injeção). Como a ECU continua enviando o `0x488`, um envio por temporizador colidiria; é preciso o mesmo mecanismo de "seguir o quadro da ECU" já usado na velocidade.

## What Changes

- Novo comando `l` com `l preheat <0|1>`, `l mil <0|1>`, `l off` e `l show`.
- A cada `0x488` recebido da ECU, o Arduino envia logo em seguida uma cópia com só os bits pedidos alterados no byte 4; os demais bits e bytes continuam os da ECU. Campo não comandado mantém o valor da ECU.
- O controlador continua iniciando em somente escuta; o modo normal passa a valer enquanto qualquer um dos três envios (`0x208`, velocidade, lâmpadas) estiver ligado.
- Fora do escopo: outros bits do `0x488`, outros IDs, gerar o `0x488` sem a ECU.

## Capabilities

### New Capabilities
- `lamp-request-override`: substituição dos pedidos de lâmpada de velas e de injeção no `0x488`, seguindo o quadro da ECU, controlada pela serial.

### Modified Capabilities
- `can-bus-capture`: o modo normal (com transmissão) passa a valer também enquanto a substituição de lâmpadas estiver ligada.

## Impact

- Código: `lib/sniffer_core` (montagem do quadro de lâmpadas, estado e comandos `l`), `src/main.cpp`.
- Testes: novos testes nativos.
- Documentação: `README.md`.
- Risco operacional: a BSI pode alternar entre o quadro da ECU e o nosso (lâmpada piscando); zerar o `MIL_REQUEST` apaga no painel um aviso de falha de motor que a ECU pediu; ver `design.md`.
