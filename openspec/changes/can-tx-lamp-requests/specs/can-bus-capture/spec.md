# Spec Delta

## MODIFIED Requirements

### Requirement: Captura passiva do barramento
O sistema SHALL iniciar em modo somente escuta, recebendo quadros CAN a 250 kbit/s sem transmitir nenhum quadro, nem confirmação (ACK) nem quadro de erro. O sistema SHALL transmitir apenas enquanto o envio do `0x208` (capability `engine-dynamics-transmit`), a substituição de velocidade do `0x388` (capability `vehicle-speed-override`) ou a substituição de lâmpadas do `0x488` (capability `lamp-request-override`) estiver ligado, e SHALL voltar a somente escuta quando todos estiverem desligados.

#### Scenario: Barramento ativo
- **WHEN** o dispositivo está conectado ao barramento HS.IS com as ECUs transmitindo e o envio do `0x208`, a substituição de velocidade e a substituição de lâmpadas estão desligados
- **THEN** o sistema recebe os quadros transmitidos pelas ECUs
- **AND** não transmite nenhum quadro no barramento

#### Scenario: Falha na inicialização do controlador CAN
- **WHEN** o controlador CAN não responde ou não pode ser configurado na inicialização
- **THEN** o sistema informa o erro pela serial
- **AND** não inicia a captura

#### Scenario: Captura continua durante o envio
- **WHEN** o envio do `0x208`, a substituição de velocidade ou a de lâmpadas está ligado
- **THEN** o sistema continua recebendo e exibindo os quadros dos outros nós

#### Scenario: Volta a somente escuta
- **WHEN** o usuário desliga o envio do `0x208`, a substituição de velocidade e a de lâmpadas
- **THEN** o sistema volta ao modo somente escuta
- **AND** não transmite nenhum quadro

#### Scenario: Um dos envios continua ligado
- **WHEN** o usuário desliga um dos envios, mas outro continua ligado
- **THEN** o sistema permanece em modo normal
