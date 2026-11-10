# Tasks

## 1. Projeto e configuração

- [x] 1.1 Adicionar a dependência `autowp/arduino-mcp2515` e `monitor_speed = 500000` ao `platformio.ini`, e um env `native` com Unity para testes no PC; verificar com `pio run` (build do Nano) e `pio test -e native` (executa sem erro de configuração)
- [x] 1.2 Criar `include/config.h` com cristal (8 MHz), bitrate (250 kbit/s), pinos (CS=D10, INT=D3), baud (500000) e capacidades (ring de 32, filtro de 16); verificar que `pio run` compila usando as constantes

## 2. Lógica pura (`lib/sniffer_core`, sem dependência do Arduino)

- [x] 2.1 Implementar o ring buffer de quadros com descarte e contagem de perdidos quando cheio; verificar com testes nativos de ordem FIFO, preenchimento total e incremento de `lost` (spec `can-bus-capture`)
- [x] 2.2 Implementar o filtro de IDs (definir, `+`, `-`, `off`, desativar ao remover o último, rejeição de ID inválido/acima de 0x7FF/lista acima de 16, estado inalterado em erro); verificar com testes nativos de todos os cenários da spec `frame-id-filter`
- [x] 2.3 Implementar a formatação da linha de quadro (`<ms> <ID> [<n>] <bytes>`, hex maiúsculo, 0 a 8 bytes) sem `printf`; verificar com testes nativos incluindo `12345 412 [8] 00 20 00 00 00 00 00 00` e quadro de 0 bytes (spec `serial-frame-output`)
- [x] 2.4 Implementar o parser de comandos por linha (`f`, `s`, `\r`/`\n`, linha vazia, linha longa, comando desconhecido) com respostas prefixadas por `#`; verificar com testes nativos de cada cenário e do relatório de estado (spec `serial-frame-output`)

## 3. Firmware do Nano

- [x] 3.1 Em `src/main.cpp`, inicializar o MCP2515 (250 kbit/s, 8 MHz, listen-only, aceitando todos os IDs) e emitir a mensagem de início ou o erro de inicialização com prefixo `#`; verificar com `pio run` e, na bancada, observando a mensagem no monitor serial (specs `can-bus-capture`, `serial-frame-output`)
- [x] 3.2 Implementar o laço principal: drenar o MCP2515 quando INT estiver em nível baixo para o ring, contar recebidos e perdidos (inclusive estouro do controlador), e emitir linhas só quando `Serial.availableForWrite()` comportar a linha; verificar na bancada com um gerador de tráfego CAN (ou no carro) que o contador de perdidos fica em zero com filtro desativado
- [x] 3.3 Ligar a leitura de comandos da serial ao filtro e ao relatório `s`; verificar no monitor serial os cenários `f 412 208`, `f +388`, `f -388`, `f off`, `f 800` e `s`

## 4. Documentação e validação em campo

- [x] 4.1 Escrever `README.md` com ligação (CS/SI/SO/SCK/INT), jumper de 120 Ω aberto, GND comum, aviso HS.IS (CAN) versus VAN, comandos, formato de saída e troca de cristal/baud; verificar seguindo o README do zero até ver quadros no monitor
- [x] 4.2 Validar no carro: ignição ligada, sem filtro, `s` mostra `perdidos = 0`; com `f 412 208`, só esses IDs aparecem e os campos de `0x412` batem com o mapeamento do PSA-RE (`CRUISE_CONTROL_ON`, `BRAKE_PEDAL_MAIN_SWITCH`)
