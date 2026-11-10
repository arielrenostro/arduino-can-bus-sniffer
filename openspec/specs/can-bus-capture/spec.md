# can-bus-capture Specification

## Purpose

Capturar de forma passiva os quadros CAN do barramento HS.IS do veículo, sem interferir na comunicação entre as ECUs e sem perder quadros silenciosamente, para que o tráfego real possa ser observado.

## Requirements

### Requirement: Captura passiva do barramento
O sistema SHALL receber quadros CAN no barramento a 250 kbit/s sem transmitir nenhum quadro, nem confirmação (ACK) nem quadro de erro, de forma a não interferir no tráfego do veículo.

#### Scenario: Barramento ativo
- **WHEN** o dispositivo está conectado ao barramento HS.IS com as ECUs transmitindo
- **THEN** o sistema recebe os quadros transmitidos pelas ECUs
- **AND** não transmite nenhum quadro no barramento

#### Scenario: Falha na inicialização do controlador CAN
- **WHEN** o controlador CAN não responde ou não pode ser configurado na inicialização
- **THEN** o sistema informa o erro pela serial
- **AND** não inicia a captura

### Requirement: Recepção sem bloquear a saída
O sistema SHALL continuar recebendo quadros do barramento enquanto transmite dados pela serial, armazenando temporariamente os quadros recebidos até que possam ser processados.

#### Scenario: Rajada de quadros durante transmissão serial
- **WHEN** vários quadros chegam do barramento enquanto o sistema envia uma linha pela serial
- **THEN** os quadros são armazenados e processados na ordem de chegada
- **AND** nenhum quadro é descartado enquanto houver espaço no armazenamento temporário

### Requirement: Contabilização de quadros perdidos
O sistema SHALL contar os quadros recebidos do barramento e os quadros perdidos, seja por estouro do armazenamento temporário, seja por estouro do controlador CAN, de modo que o usuário saiba se os dados estão completos.

#### Scenario: Armazenamento temporário cheio
- **WHEN** um quadro chega e o armazenamento temporário está cheio
- **THEN** o quadro é descartado
- **AND** o contador de quadros perdidos é incrementado

#### Scenario: Estouro no controlador CAN
- **WHEN** o controlador CAN sinaliza estouro de seu buffer de recepção
- **THEN** o contador de quadros perdidos é incrementado
- **AND** a captura continua

### Requirement: Quadros de dados com identificador padrão
O sistema SHALL tratar quadros CAN de dados com identificador padrão de 11 bits e comprimento de 0 a 8 bytes, preservando identificador, comprimento e conteúdo exatos.

#### Scenario: Quadro de 8 bytes
- **WHEN** chega um quadro com identificador `0x412` e 8 bytes de dados
- **THEN** o sistema preserva o identificador, o comprimento 8 e os 8 bytes sem alteração
