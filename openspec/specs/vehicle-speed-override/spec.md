# vehicle-speed-override Specification

## Purpose
Fazer a BSI enxergar uma velocidade escolhida pelo usuário, no quadro `0x388`, sem colidir com a ECU que já o envia e sem alterar o contador de distância nem a aceleração dela.

## Requirements

### Requirement: Substituição desligada por padrão
O sistema SHALL iniciar com a substituição de velocidade desligada e SHALL transmitir o quadro `0x388` somente enquanto ela estiver ligada.

#### Scenario: Inicialização
- **WHEN** o sistema inicia
- **THEN** a substituição de velocidade está desligada
- **AND** nenhum quadro `0x388` é transmitido

### Requirement: Ligar e ajustar a velocidade
O sistema SHALL aceitar `v <km/h>`, com número inteiro de 0 a 300, que liga a substituição com essa velocidade ou altera a velocidade com ela já ligada. A velocidade SHALL ser codificada em km/h × 100, em 16 bits big-endian, nos bytes 1-2 do quadro.

#### Scenario: Ligar com 60 km/h
- **WHEN** a substituição está desligada e o usuário envia `v 60`
- **THEN** a substituição passa a ligada com 60 km/h
- **AND** os bytes 1-2 dos quadros enviados passam a ser `17 70`

#### Scenario: Alterar com a substituição ligada
- **WHEN** a substituição está ligada com 60 km/h e o usuário envia `v 90`
- **THEN** os quadros seguintes têm os bytes 1-2 iguais a `23 28`

### Requirement: Seguir o quadro da ECU
Para cada quadro `0x388` de identificador padrão recebido enquanto a substituição estiver ligada, o sistema SHALL transmitir logo em seguida um quadro `0x388` com o mesmo comprimento e os mesmos bytes, exceto os bytes 1-2, que SHALL conter a velocidade pedida. O sistema SHALL NOT transmitir o `0x388` em outro momento.

#### Scenario: Cópia com velocidade trocada
- **WHEN** a substituição está ligada com 60 km/h e a ECU envia `0x388` com `00 00 00 03 AF`
- **THEN** o sistema transmite `0x388` com `17 70 00 03 AF`

#### Scenario: Contador de distância e aceleração preservados
- **WHEN** a ECU passa a enviar o contador de distância `00 04` e a aceleração `B0`
- **THEN** o quadro seguinte transmitido tem os bytes 3 a 5 iguais a `00 04 B0`

#### Scenario: Sem quadro da ECU
- **WHEN** a substituição está ligada e a ECU não envia nenhum `0x388`
- **THEN** o sistema não transmite nenhum `0x388`

### Requirement: Desligar a substituição
O sistema SHALL aceitar `v off`, que interrompe a transmissão do `0x388`. O controlador CAN SHALL voltar ao modo somente escuta se o envio do `0x208` também estiver desligado.

#### Scenario: Parar
- **WHEN** a substituição está ligada e o usuário envia `v off`
- **THEN** o sistema deixa de transmitir o `0x388`

### Requirement: Rejeição de comandos inválidos
O sistema SHALL rejeitar, informando o motivo pela serial, comandos `v` sem argumento, com valor não inteiro ou fora de 0 a 300, ou com argumentos a mais. Um comando rejeitado SHALL NOT alterar a velocidade nem ligar a substituição.

#### Scenario: Valor fora da faixa
- **WHEN** a substituição está desligada e o usuário envia `v 301`
- **THEN** o sistema informa que o valor é inválido
- **AND** a substituição continua desligada

### Requirement: Estado da substituição
O sistema SHALL responder a `v show` com se a substituição está ligada, a velocidade, o total de quadros enviados e o total de falhas de envio.

#### Scenario: Consulta
- **WHEN** a substituição está ligada com 60 km/h e o usuário envia `v show`
- **THEN** a resposta indica ligada, 60 km/h, e os totais de enviados e de falhas

### Requirement: Contagem de falhas de envio
O sistema SHALL contar como falha cada quadro `0x388` que não puder ser enfileirado para transmissão porque o controlador CAN não tem buffer de envio livre, sem interromper a substituição.

#### Scenario: Buffers de envio ocupados
- **WHEN** a ECU envia o `0x388` e todos os buffers de envio do controlador estão ocupados
- **THEN** o contador de falhas é incrementado
- **AND** o sistema tenta de novo no próximo `0x388` da ECU
