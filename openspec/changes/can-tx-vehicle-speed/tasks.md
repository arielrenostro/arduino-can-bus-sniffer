# Tasks

## 1. Quadro de velocidade (`lib/sniffer_core`, sem dependência do Arduino)

- [x] 1.1 Implementar `SpeedState` e `buildSpeedFrame` (cópia do quadro recebido com os bytes 1-2 trocados por km/h × 100 em big-endian); verificar com testes nativos: `v 60` sobre `00 00 00 03 AF` dá `17 70 00 03 AF`, 90 km/h dá `23 28`, contador e aceleração preservados, quadros com menos de 2 bytes recusados, comprimento mantido

## 2. Comandos `v` (parser)

- [x] 2.1 Estender `CommandProcessor` com ponteiro opcional para `SpeedState` e os comandos `v <km/h>`, `v off` e `v show`: ligar ou alterar com 0 a 300, rejeitar valor inválido, ausente, não inteiro ou com argumentos a mais sem ligar nem alterar; verificar com testes nativos de todos os cenários da spec `vehicle-speed-override` e que os testes anteriores continuam passando

## 3. Firmware

- [ ] 3.1 Em `src/main.cpp`, ao receber um `0x388` com a substituição ligada, enviar a cópia com a velocidade trocada (contando `sent` e `failed`), e manter o controlador em modo normal enquanto o `0x208` ou a velocidade estiver ligado; verificar com `pio run` (RAM abaixo de 90%) e, na bancada, com `v 60` e `v show`, observando o velocímetro do painel (spec `vehicle-speed-override`, spec modificada `can-bus-capture`)

## 4. Documentação

- [x] 4.1 Atualizar o `README.md` com os comandos `v`, o funcionamento (seguir o quadro da ECU) e os avisos (carro parado, travamento das portas, odômetro, escala não confirmada); verificar que o texto descreve a saída real de `v show`
