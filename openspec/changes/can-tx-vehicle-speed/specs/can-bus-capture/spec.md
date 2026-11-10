# Spec Delta

## MODIFIED Requirements

### Requirement: Captura passiva do barramento
O sistema SHALL iniciar em modo somente escuta, recebendo quadros CAN a 250 kbit/s sem transmitir nenhum quadro, nem confirmação (ACK) nem quadro de erro. O sistema SHALL transmitir apenas enquanto o envio do `0x208` (capability `engine-dynamics-transmit`) ou a substituição de velocidade do `0x388` (capability `vehicle-speed-override`) estiver ligado, e SHALL voltar a somente escuta quando ambos estiverem desligados.

#### Scenario: Barramento ativo
- **WHEN** o dispositivo está conectado ao barramento HS.IS com as ECUs transmitindo e o envio do `0x208` e a substituição de velocidade estão desligados
- **THEN** o sistema recebe os quadros transmitidos pelas ECUs
- **AND** não transmite nenhum quadro no barramento

#### Scenario: Falha na inicialização do controlador CAN
- **WHEN** o controlador CAN não responde ou não pode ser configurado na inicialização
- **THEN** o sistema informa o erro pela serial
- **AND** não inicia a captura

#### Scenario: Captura continua durante o envio
- **WHEN** o envio do `0x208` ou a substituição de velocidade está ligado
- **THEN** o sistema continua recebendo e exibindo os quadros dos outros nós

#### Scenario: Volta a somente escuta
- **WHEN** o usuário desliga o envio do `0x208` e a substituição de velocidade
- **THEN** o sistema volta ao modo somente escuta
- **AND** não transmite nenhum quadro

#### Scenario: Um dos envios continua ligado
- **WHEN** o usuário desliga o envio do `0x208`, mas a substituição de velocidade continua ligada
- **THEN** o sistema permanece em modo normal
