# Design

## Context

Ver `proposal.md` (Why). O `0x488` (`ENGINE_DATA`, 8 bytes, 100 ms, enviado pela ECU à BSI) capturado: `43 9A FF 04 00 43 1A 45`. O byte 4 (`0x04`) tem o bit 2 `MIL_REQUEST` em 1; o bit 1 é `PREHEAT_LAMP_REQUEST` e o bit 6 é `CRUISE_CONTROL_FAULT` (em 0). O usuário não consegue alterar o firmware da ECU, e ela continua enviando o `0x488`. O mecanismo "seguir o quadro da ECU" já existe para a velocidade (`0x388`) e foi validado na bancada: a BSI reage ao quadro copiado, embora também considere o da ECU.

Premissa: testes com o carro parado.

## Goals / Non-Goals

**Goals:**
- Alterar dois bits do byte 4 do `0x488` sem colidir com a ECU e sem alterar o resto do quadro dela.
- Lógica pura e testável no PC, no mesmo molde do `v`.

**Non-Goals:**
- Outros bits ou IDs; gerar o `0x488` sem a ECU.
- Garantir que a BSI use o nosso quadro e não o da ECU (ver riscos).

## Decisions

**1. Seguir o quadro da ECU, sem temporizador.**
Mesma decisão da velocidade: o `0x488` só é enviado em reação a um `0x488` recebido, logo após o dele. Sem temporizador próprio, os dois quadros nunca iniciam juntos.

**2. Cada campo tem três estados: seguir a ECU, 0 ou 1.**
`LampState` guarda `preheat` e `mil` como `int8_t` (-1 = manter o valor da ECU). `l preheat 1` não toca no MIL, e `l mil 0` não toca no preheat. `l off` volta os dois a -1 (e desliga), para que cada nova sessão comece do padrão.

**3. Montagem pura em `LampRequest.h`.**
`buildLampFrame(src, len, preheat, mil, out)` copia o quadro e ajusta só as máscaras `0x02` (preheat) e `0x04` (MIL) do byte 4. Exige ao menos 4 bytes. O parser recebe um ponteiro opcional para `LampState`, como já faz com `TxState` e `SpeedState`.

**4. Modo do controlador.**
Qualquer um dos três envios ligados mantém o modo normal; somente escuta quando todos estão desligados.

## Risks / Trade-offs

- **A BSI pode alternar entre o nosso quadro e o da ECU** → a lâmpada pode piscar em vez de ficar fixa, como a velocidade oscilou. Mitigação: é um teste de bancada; se piscar, fica claro que a BSI considera os dois e a próxima opção seria impedir o quadro da ECU.
- **O painel pode não ter a lâmpada de velas** (é de motor diesel) → o teste não mostraria nada. Mitigação: o `l mil` serve como alternativa, porque a luz de injeção existe em qualquer painel.
- **Zerar o `MIL_REQUEST` apaga no painel um aviso que a ECU pediu** → pode esconder uma falha real. Mitigação: só com comando explícito, `l off` devolve o controle à ECU, e o README avisa.
- **Quadro com menos de 4 bytes, estendido ou remoto** → ignorado.
