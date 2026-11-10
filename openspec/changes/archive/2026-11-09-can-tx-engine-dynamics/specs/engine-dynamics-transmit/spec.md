# Spec Delta

## Purpose

Permitir gerar o quadro `0x208` (ENGINE_DYNAMICS) do HS.IS com valores controlados pela serial, partindo de um quadro de referência de motor em marcha lenta, para observar como a BSI e o painel reagem.

## ADDED Requirements

### Requirement: Envio desligado por padrão
O sistema SHALL iniciar com o envio do `0x208` desligado e SHALL transmitir quadros somente após um comando de envio.

#### Scenario: Inicialização
- **WHEN** o sistema inicia
- **THEN** o envio está desligado
- **AND** nenhum quadro é transmitido no barramento

### Requirement: Quadro de referência
Ao ligar o envio, o conteúdo do quadro SHALL partir do quadro de referência `1F 58 2D 00 40 2D 2D 2D` (1003 rpm, torque −10 Nm, pedal 0%, cruise 0, freio 0, A/C 0, `DIAG_MUX_ON` 1), seguido da alteração pedida no comando, se houver.

#### Scenario: Ligar sem alteração
- **WHEN** o envio está desligado e o usuário envia `t on`
- **THEN** o sistema passa a enviar o quadro `1F 58 2D 00 40 2D 2D 2D`

#### Scenario: Primeiro comando de campo liga o envio
- **WHEN** o envio está desligado e o usuário envia `t cruise 2`
- **THEN** o sistema passa a enviar o quadro de referência com o cruise em 2, ou seja, `1F 58 2D 00 48 2D 2D 2D`

#### Scenario: Religar parte de novo da referência
- **WHEN** o usuário envia `t cruise 2`, depois `t off` e depois `t pedal 10`
- **THEN** o novo envio usa o quadro de referência com apenas o pedal em 10% (cruise volta a 0)

### Requirement: Campos nomeados e conversões
O sistema SHALL aceitar os comandos `t rpm <0..8191>`, `t torque <-100..408>`, `t pedal <0..100>`, `t cruise <0..3>`, `t brake <0|1>`, `t ac <0|1>` e `t diag <0|1>` com números inteiros, e SHALL codificá-los no quadro: rpm × 8 em 16 bits (bytes 1-2, big-endian); torque como (Nm + 100) / 2, arredondado para cima, nos bytes 3, 6, 7 e 8; pedal × 2 no byte 4; cruise nos bits 5.3-5.2; freio no bit 5.1; A/C no bit 5.0; `DIAG_MUX_ON` no bit 5.6.

#### Scenario: Rotação
- **WHEN** o usuário envia `t rpm 2000`
- **THEN** os bytes 1 e 2 do quadro passam a ser `3E 80`

#### Scenario: Torque do motor parado
- **WHEN** o usuário envia `t torque 110`
- **THEN** os bytes 3, 6, 7 e 8 passam a ser `69`

#### Scenario: Pedal e flags
- **WHEN** o usuário envia `t pedal 50` e `t cruise 3`
- **THEN** o byte 4 passa a ser `64`
- **AND** o byte 5 passa a ser `4C`

### Requirement: Alteração com o envio ligado
Com o envio ligado, um comando de campo SHALL alterar só o campo indicado e manter os demais campos atuais, valendo a partir do próximo quadro enviado.

#### Scenario: Campos acumulados
- **WHEN** o envio está ligado com `t cruise 2` e o usuário envia `t rpm 1500`
- **THEN** o quadro passa a ter 1500 rpm e o cruise continua em 2

### Requirement: Cadência do envio
O sistema SHALL enviar o quadro com identificador padrão `0x208` e 8 bytes a cada período configurado, com padrão de 10 ms, enquanto o envio estiver ligado.

#### Scenario: Envio periódico
- **WHEN** o envio está ligado com o período padrão
- **THEN** um quadro `0x208` é enviado a cada 10 ms

### Requirement: Ajuste do período
O sistema SHALL aceitar `t period <ms>`, com valor inteiro de 2 a 5000, que altera o intervalo do envio. O comando SHALL NOT ligar o envio e o valor SHALL permanecer após `t off`.

#### Scenario: Período maior
- **WHEN** o usuário envia `t period 100` com o envio ligado
- **THEN** o quadro passa a ser enviado a cada 100 ms

#### Scenario: Período sem ligar o envio
- **WHEN** o envio está desligado e o usuário envia `t period 50`
- **THEN** o envio continua desligado
- **AND** o próximo envio usa o período de 50 ms

### Requirement: Desligar o envio
O sistema SHALL aceitar `t off`, que interrompe o envio e volta o controlador CAN ao modo somente escuta.

#### Scenario: Parar de enviar
- **WHEN** o envio está ligado e o usuário envia `t off`
- **THEN** nenhum outro quadro `0x208` é transmitido pelo sistema
- **AND** o sistema deixa de dar confirmação (ACK) a quadros de outros nós

### Requirement: Rejeição de comandos inválidos
O sistema SHALL rejeitar, informando o motivo pela serial, comandos `t` com campo desconhecido, valor ausente, não inteiro ou fora da faixa. Um comando rejeitado SHALL NOT alterar o quadro, o período nem ligar o envio.

#### Scenario: Valor fora da faixa
- **WHEN** o envio está desligado e o usuário envia `t cruise 4`
- **THEN** o sistema informa que o valor é inválido
- **AND** o envio continua desligado

#### Scenario: Valor não numérico
- **WHEN** o usuário envia `t rpm abc`
- **THEN** o sistema informa que o valor é inválido
- **AND** o quadro não muda

### Requirement: Estado do envio
O sistema SHALL responder a `t show` com se o envio está ligado, o período, os valores dos campos, o quadro em hexadecimal, o total de quadros enviados e o total de falhas de envio.

#### Scenario: Consulta com o envio ligado
- **WHEN** o envio está ligado com `t cruise 2` e o usuário envia `t show`
- **THEN** a resposta indica envio ligado, período de 10 ms, o quadro `1F 58 2D 00 48 2D 2D 2D`, e os totais de enviados e de falhas

### Requirement: Contagem de falhas de envio
O sistema SHALL contar como falha cada quadro que não puder ser enfileirado para transmissão porque o controlador CAN não tem buffer de envio livre, sem interromper o envio.

#### Scenario: Buffers de envio ocupados
- **WHEN** chega o momento de enviar e todos os buffers de envio do controlador estão ocupados
- **THEN** o contador de falhas é incrementado
- **AND** o sistema tenta de novo no próximo período

### Requirement: Somente o identificador 0x208
O sistema SHALL transmitir quadros apenas com o identificador `0x208`.

#### Scenario: Nenhum outro ID
- **WHEN** o envio está ligado
- **THEN** todo quadro transmitido pelo sistema tem identificador `0x208`
