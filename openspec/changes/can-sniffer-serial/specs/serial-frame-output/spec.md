# Spec Delta

## Purpose

Definir como os quadros capturados e o estado do sniffer são apresentados na serial, de modo legível em um monitor serial comum e fácil de comparar com as definições de quadros do PSA-RE.

## ADDED Requirements

### Requirement: Uma linha de texto por quadro exibido
O sistema SHALL emitir uma linha de texto para cada quadro exibido, no formato `<ms> <ID> [<n>] <b1> <b2> ... <bn>`, onde `<ms>` é o tempo desde o início em milissegundos, `<ID>` tem 3 dígitos hexadecimais maiúsculos, `<n>` é o número de bytes de dados e cada `<bi>` é um byte com 2 dígitos hexadecimais maiúsculos, na ordem de transmissão.

#### Scenario: Quadro de 8 bytes
- **WHEN** um quadro `0x412` com os bytes `00 20 00 00 00 00 00 00` é exibido aos 12345 ms
- **THEN** a linha emitida é `12345 412 [8] 00 20 00 00 00 00 00 00`

#### Scenario: Quadro sem dados
- **WHEN** um quadro de 0 bytes com ID `0x609` é exibido
- **THEN** a linha emitida contém o ID e `[0]`, sem bytes de dados

### Requirement: Ordem de exibição
O sistema SHALL emitir as linhas na mesma ordem em que os quadros chegaram do barramento.

#### Scenario: Dois quadros em sequência
- **WHEN** chegam um quadro `0x208` e depois um quadro `0x412`, ambos exibíveis
- **THEN** a linha do `0x208` é emitida antes da linha do `0x412`

### Requirement: Vazão compatível com o barramento
O sistema SHALL operar a serial a 500000 baud, de modo que o tráfego dos IDs do HS.IS mapeados em PSA-RE, sem filtro, possa ser emitido sem perda de quadros.

#### Scenario: Tráfego típico sem filtro
- **WHEN** o veículo está com a ignição ligada, o filtro está desativado e o monitor serial está a 500000 baud
- **THEN** o contador de quadros perdidos permanece em zero em condições normais de uso

### Requirement: Relatório de estado
O sistema SHALL emitir, em resposta ao comando `s`, o estado do filtro (desativado ou a lista de IDs ativos), o total de quadros recebidos, o total de quadros exibidos e o total de quadros perdidos.

#### Scenario: Consulta com filtro ativo
- **WHEN** o filtro contém `0x412` e `0x208` e o usuário envia `s`
- **THEN** o sistema emite a lista `412 208` como filtro ativo
- **AND** emite os totais de recebidos, exibidos e perdidos

#### Scenario: Consulta sem filtro
- **WHEN** o filtro está desativado e o usuário envia `s`
- **THEN** o sistema indica que o filtro está desativado e emite os totais

### Requirement: Mensagem de início
O sistema SHALL emitir ao iniciar uma linha identificando o programa, a velocidade do barramento e o resultado da inicialização do controlador CAN.

#### Scenario: Inicialização bem-sucedida
- **WHEN** o sistema inicia e o controlador CAN é configurado
- **THEN** uma linha informa que a captura começou a 500 kbit/s em modo somente escuta

### Requirement: Distinção entre quadros e mensagens do sistema
O sistema SHALL iniciar toda mensagem de sistema (respostas de comando, erros, relatório) com o prefixo `#`, para distingui-las das linhas de quadro.

#### Scenario: Erro de comando
- **WHEN** o usuário envia um comando desconhecido
- **THEN** a linha de resposta começa com `#` e descreve o erro
