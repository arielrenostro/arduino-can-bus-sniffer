# Tasks

## 1. Quadro 0x208 (`lib/sniffer_core`, sem dependência do Arduino)

- [x] 1.1 Implementar `EngineDynamics` e `TxState`: referência (`1F 58 2D 00 40 2D 2D 2D`), setters com validação e conversão (rpm × 8; (Nm + 100 + 1) / 2; % × 2; flags), `encode()` e `reset()`; verificar com testes nativos de cada cenário da spec `engine-dynamics-transmit` (referência, `t rpm 2000` = `3E 80`, `t torque 110` = `69`, pedal 50 + cruise 3 = byte 4 `64` e byte 5 `4C`, limites e rejeição)

## 2. Comandos `t` (parser)

- [x] 2.1 Estender `CommandProcessor` com ponteiro opcional para `TxState` e os comandos `t on`, `t off`, `t rpm|torque|pedal|cruise|brake|ac|diag <n>`, `t period <ms>` e `t show`: comando de campo liga o envio a partir da referência, com o envio ligado só altera o campo, comando inválido não altera nada nem liga; verificar com testes nativos de todos os cenários da spec `engine-dynamics-transmit` e confirmar que os 31 testes anteriores do parser continuam passando

## 3. Firmware

- [x] 3.1 Em `src/main.cpp`, trocar o controlador entre somente escuta e modo normal quando `TxState.active` muda (com erro `#` e envio desligado se a troca falhar) e enviar o `0x208` a cada `periodMs` com `micros()` acumulado, contando `sent` e `failed`; verificar com `pio run` (RAM abaixo de 90%) e, na bancada, com `t cruise 2`, `t show` e `t off`, observando o painel e o contador de enviados (spec `engine-dynamics-transmit`, spec modificada `can-bus-capture`)

## 4. Documentação

- [x] 4.1 Atualizar o `README.md` com os comandos `t`, o quadro de referência e os avisos (somente com o carro parado; colisão com o `0x208` da ECU; modo normal dá ACK); verificar seguindo o README: `t show` mostra o que o texto descreve
