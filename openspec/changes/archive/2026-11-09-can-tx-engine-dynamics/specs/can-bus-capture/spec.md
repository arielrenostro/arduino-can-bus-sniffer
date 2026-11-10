# Spec Delta

## MODIFIED Requirements

### Requirement: Captura passiva do barramento
O sistema SHALL iniciar em modo somente escuta, recebendo quadros CAN a 250 kbit/s sem transmitir nenhum quadro, nem confirmação (ACK) nem quadro de erro. O sistema SHALL transmitir apenas enquanto o envio do `0x208` estiver ligado (capability `engine-dynamics-transmit`) e SHALL voltar a somente escuta ao desligá-lo.

#### Scenario: Barramento ativo
- **WHEN** o dispositivo está conectado ao barramento HS.IS com as ECUs transmitindo e o envio do `0x208` está desligado
- **THEN** o sistema recebe os quadros transmitidos pelas ECUs
- **AND** não transmite nenhum quadro no barramento

#### Scenario: Falha na inicialização do controlador CAN
- **WHEN** o controlador CAN não responde ou não pode ser configurado na inicialização
- **THEN** o sistema informa o erro pela serial
- **AND** não inicia a captura

#### Scenario: Captura continua durante o envio
- **WHEN** o envio do `0x208` está ligado
- **THEN** o sistema continua recebendo e exibindo os quadros dos outros nós

#### Scenario: Volta a somente escuta
- **WHEN** o usuário desliga o envio do `0x208`
- **THEN** o sistema volta ao modo somente escuta
- **AND** não transmite nenhum quadro
