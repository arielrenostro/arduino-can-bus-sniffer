# Proposal

## Why

Para entender como a BSI e o painel reagem ao quadro `0x208` (ENGINE_DYNAMICS, enviado pela ECU, com o estado do cruise, rotação, torque e pedais), é preciso poder gerar esse quadro com valores controlados. O sniffer hoje só escuta. A sua ECU programável ainda não deixa de enviar o `0x208`, então o teste precisa ser possível mesmo assim.

## What Changes

- Novo comando `t` na serial para enviar o `0x208` a cada 10 ms (período ajustável), com campos nomeados: `t rpm`, `t torque`, `t pedal`, `t cruise`, `t brake`, `t ac`, `t diag`, `t period`, além de `t on`, `t off` e `t show`.
- Todo envio começa a partir de um quadro de referência capturado com o motor em marcha lenta (`1F 58 2D 00 40 2D 2D 2D`). O primeiro comando de campo, como `t cruise 2`, já liga o envio com esse conteúdo mais a alteração pedida.
- O firmware continua iniciando em modo somente escuta. O modo normal (com ACK e transmissão) só vale enquanto o envio estiver ligado; `t off` volta a somente escuta.
- Contadores de quadros enviados e de falhas de envio, visíveis em `t show`.
- Fora do escopo: enviar outros IDs, enviar bytes crus, sincronizar o envio com o `0x208` da ECU (modo "seguir a ECU"), recuperação de bus-off.

## Capabilities

### New Capabilities
- `engine-dynamics-transmit`: geração controlada do quadro `0x208` por comandos na serial (campos, conversões, quadro de referência, cadência, ligar e desligar, contadores).

### Modified Capabilities
- `can-bus-capture`: a captura deixa de ser sempre sem transmissão; passa a ser somente escuta por padrão, com transmissão apenas enquanto o envio do `0x208` estiver ligado.

## Impact

- Código: `lib/sniffer_core` (novo módulo do quadro `0x208` e extensão do parser de comandos), `src/main.cpp` (troca de modo do MCP2515 e agendamento do envio).
- Testes: novos testes nativos para o quadro e para os comandos `t`.
- Documentação: `README.md` (comandos `t`, avisos).
- Risco operacional: a ECU continua enviando o `0x208`; ver `design.md`.
