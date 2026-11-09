# Spec Delta

## Purpose

Permitir que o usuário escolha, em tempo de execução e pela serial, quais identificadores CAN são exibidos: todos ou apenas uma lista específica, para focar nos quadros de interesse (por exemplo `0x412` e `0x208`).

## ADDED Requirements

### Requirement: Exibição de todos os IDs por padrão
O sistema SHALL exibir quadros de todos os identificadores ao iniciar, sem filtro ativo.

#### Scenario: Inicialização
- **WHEN** o sistema inicia
- **THEN** o filtro está desativado
- **AND** todos os quadros recebidos são exibidos

### Requirement: Definição da lista de IDs exibidos
O sistema SHALL aceitar o comando `f <id> [<id> ...]`, com IDs em hexadecimal (com ou sem prefixo `0x`), que substitui a lista de IDs exibidos e ativa o filtro. Somente quadros cujo ID está na lista SHALL ser exibidos.

#### Scenario: Filtro por dois IDs
- **WHEN** o usuário envia `f 412 208`
- **THEN** apenas quadros com ID `0x412` ou `0x208` são exibidos
- **AND** quadros de outros IDs não são exibidos

#### Scenario: Nova lista substitui a anterior
- **WHEN** o filtro está ativo com `0x412` e o usuário envia `f 388`
- **THEN** apenas quadros com ID `0x388` são exibidos

### Requirement: Ajuste incremental da lista
O sistema SHALL aceitar `f +<id>` para adicionar um ID à lista e `f -<id>` para removê-lo, mantendo os demais. Adicionar um ID com o filtro desativado SHALL ativar o filtro contendo apenas esse ID. Remover o último ID da lista SHALL desativar o filtro.

#### Scenario: Adição a uma lista existente
- **WHEN** o filtro contém `0x412` e o usuário envia `f +208`
- **THEN** o filtro passa a conter `0x412` e `0x208`

#### Scenario: Remoção do último ID
- **WHEN** o filtro contém apenas `0x412` e o usuário envia `f -412`
- **THEN** o filtro é desativado
- **AND** todos os quadros voltam a ser exibidos

### Requirement: Desativação do filtro
O sistema SHALL aceitar o comando `f off`, que desativa o filtro e volta a exibir todos os IDs.

#### Scenario: Volta a exibir tudo
- **WHEN** o filtro está ativo e o usuário envia `f off`
- **THEN** quadros de todos os IDs voltam a ser exibidos

### Requirement: Rejeição de comandos inválidos
O sistema SHALL rejeitar, informando o motivo pela serial e sem alterar o filtro atual, comandos de filtro com ID inválido (não hexadecimal ou acima de `0x7FF`), sem argumentos, ou que excedam a capacidade de 16 IDs na lista.

#### Scenario: ID fora da faixa
- **WHEN** o usuário envia `f 800`
- **THEN** o sistema informa que o ID é inválido
- **AND** o filtro permanece como estava

#### Scenario: Lista além da capacidade
- **WHEN** o usuário envia `f` com mais de 16 IDs
- **THEN** o sistema informa que a capacidade foi excedida
- **AND** o filtro permanece como estava

### Requirement: Filtro não afeta a contagem
O filtro SHALL afetar somente a exibição; quadros não exibidos SHALL continuar sendo contados como recebidos.

#### Scenario: Quadro fora do filtro
- **WHEN** o filtro contém `0x412` e chega um quadro `0x388`
- **THEN** o quadro não é exibido
- **AND** o contador de quadros recebidos é incrementado
