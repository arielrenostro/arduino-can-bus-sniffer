# Spec Delta

## Purpose

Alterar, sem mexer na ECU, os pedidos de lâmpada de velas (preheat) e de injeção (MIL) que ela envia à BSI no quadro `0x488`, sem colidir com a ECU e preservando todo o resto do quadro dela.

## ADDED Requirements

### Requirement: Substituição desligada por padrão
O sistema SHALL iniciar com a substituição de lâmpadas desligada e SHALL transmitir o quadro `0x488` somente enquanto ela estiver ligada.

#### Scenario: Inicialização
- **WHEN** o sistema inicia
- **THEN** a substituição de lâmpadas está desligada
- **AND** nenhum quadro `0x488` é transmitido

### Requirement: Comandos de lâmpada
O sistema SHALL aceitar `l preheat <0|1>`, que define o bit 1 do byte 4 do `0x488` (`PREHEAT_LAMP_REQUEST`), e `l mil <0|1>`, que define o bit 2 do byte 4 (`MIL_REQUEST`). Um comando válido SHALL ligar a substituição se estiver desligada. O campo que não foi comandado SHALL manter o valor enviado pela ECU.

#### Scenario: Pedir a lâmpada de velas
- **WHEN** a substituição está desligada e o usuário envia `l preheat 1`
- **THEN** a substituição passa a ligada
- **AND** nos quadros enviados o bit 1 do byte 4 é 1 e o bit 2 do byte 4 é o que a ECU enviou

#### Scenario: Zerar a luz de injeção
- **WHEN** a ECU envia o byte 4 igual a `04` (MIL pedido) e o usuário envia `l mil 0`
- **THEN** o quadro transmitido tem o byte 4 igual a `00`

#### Scenario: Os dois campos
- **WHEN** o usuário envia `l preheat 1` e depois `l mil 0`
- **THEN** os quadros enviados têm o bit 1 em 1 e o bit 2 em 0 no byte 4

### Requirement: Seguir o quadro da ECU
Para cada quadro `0x488` de identificador padrão e com ao menos 4 bytes recebido enquanto a substituição estiver ligada, o sistema SHALL transmitir logo em seguida um quadro `0x488` com o mesmo comprimento e os mesmos bytes, exceto os bits comandados do byte 4. O sistema SHALL NOT transmitir o `0x488` em outro momento.

#### Scenario: Demais bits preservados
- **WHEN** a ECU envia `43 9A FF 04 00 43 1A 45` e a substituição está ligada com `l preheat 1`
- **THEN** o sistema transmite `43 9A FF 06 00 43 1A 45`

#### Scenario: Sem quadro da ECU
- **WHEN** a substituição está ligada e a ECU não envia nenhum `0x488`
- **THEN** o sistema não transmite nenhum `0x488`

### Requirement: Desligar a substituição
O sistema SHALL aceitar `l off`, que interrompe a transmissão do `0x488` e descarta os valores comandados (os campos voltam a seguir a ECU). O controlador CAN SHALL voltar a somente escuta se nenhum outro envio estiver ligado.

#### Scenario: Parar e religar do zero
- **WHEN** o usuário envia `l preheat 1`, depois `l off` e depois `l mil 0`
- **THEN** o quadro transmitido mantém o bit 1 do byte 4 igual ao da ECU e tem o bit 2 em 0

### Requirement: Rejeição de comandos inválidos
O sistema SHALL rejeitar, informando o motivo pela serial, comandos `l` sem argumentos, com campo desconhecido, valor diferente de 0 ou 1, ou argumentos a mais. Um comando rejeitado SHALL NOT alterar os campos nem ligar a substituição.

#### Scenario: Valor inválido
- **WHEN** a substituição está desligada e o usuário envia `l mil 2`
- **THEN** o sistema informa que o valor é inválido
- **AND** a substituição continua desligada

### Requirement: Estado da substituição
O sistema SHALL responder a `l show` com se a substituição está ligada, o valor de cada campo (0, 1 ou o da ECU), o total de quadros enviados e o total de falhas de envio.

#### Scenario: Consulta
- **WHEN** o usuário envia `l preheat 1` e depois `l show`
- **THEN** a resposta indica ligada, preheat em 1, injeção seguindo a ECU, e os totais de enviados e de falhas

### Requirement: Contagem de falhas de envio
O sistema SHALL contar como falha cada quadro `0x488` que não puder ser enfileirado para transmissão porque o controlador CAN não tem buffer de envio livre, sem interromper a substituição.

#### Scenario: Buffers de envio ocupados
- **WHEN** a ECU envia o `0x488` e todos os buffers de envio do controlador estão ocupados
- **THEN** o contador de falhas é incrementado
- **AND** o sistema tenta de novo no próximo `0x488` da ECU
