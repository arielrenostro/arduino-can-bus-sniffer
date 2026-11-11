# Tasks

## 1. Quadro de lâmpadas (`lib/sniffer_core`, sem dependência do Arduino)

- [x] 1.1 Implementar `LampState` (campos com três estados: ECU, 0, 1) e `buildLampFrame` (cópia do `0x488` recebido ajustando só os bits 1 e 2 do byte 4); verificar com testes nativos: `43 9A FF 04 00 43 1A 45` com preheat 1 dá byte 4 `06`, com mil 0 dá `00`, demais bytes preservados, quadros com menos de 4 bytes recusados

## 2. Comandos `l` (parser)

- [x] 2.1 Estender `CommandProcessor` com ponteiro opcional para `LampState` e os comandos `l preheat <0|1>`, `l mil <0|1>`, `l off` e `l show`: comando válido liga, campo não comandado segue a ECU, `l off` devolve os campos à ECU, valor inválido ou argumento a mais é rejeitado sem alterar nem ligar; verificar com testes nativos de todos os cenários da spec `lamp-request-override` e que os testes anteriores continuam passando

## 3. Firmware

- [x] 3.1 Em `src/main.cpp`, ao receber um `0x488` com a substituição ligada, enviar a cópia com os bits ajustados (contando `sent` e `failed`), e manter o modo normal enquanto o `0x208`, a velocidade ou as lâmpadas estiverem ligados; verificar com `pio run` (RAM abaixo de 90%) e, na bancada, com `l mil 0` e `l preheat 1`, observando o painel (spec `lamp-request-override`, spec modificada `can-bus-capture`)

## 4. Documentação

- [x] 4.1 Atualizar o `README.md` com os comandos `l`, o funcionamento e os avisos (carro parado, lâmpada pode piscar, `l mil 0` esconde um aviso da ECU); verificar que o exemplo de `l show` é a saída real dos testes
