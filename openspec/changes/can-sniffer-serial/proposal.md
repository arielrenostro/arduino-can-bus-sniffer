# Proposal

## Why

Para simular o envio do quadro `0x208` (ENGINE_DYNAMICS, status do controle de cruzeiro enviado pela ECU), é preciso antes entender o que a BSI e a ECU realmente trafegam no barramento HS.IS (CAN 500 kbit/s) do Peugeot 206. Os mapeamentos do PSA-RE têm lacunas nos sinais de cruzeiro (`CRUISE_CONTROL_BUTTONS` e `CRUISE_CONTROL_ON` no `0x412`, `CRUISE_CONTROL_STATUS` no `0x208`: sem tabela de valores). Falta uma ferramenta de captura passiva para observar o tráfego real e preencher essas lacunas.

## What Changes

- Firmware Arduino (PlatformIO, Nano ATmega328 + MCP2515) que captura passivamente quadros CAN do HS.IS a 500 kbit/s, em modo listen-only.
- Recepção desacoplada do serial: o pino INT do MCP2515 indica quadro pendente, o laço principal o drena para um ring buffer e a saída serial é não bloqueante, para não perder quadros enquanto o serial transmite.
- Saída textual, uma linha por quadro, pela serial (500000 baud), visualizável no `pio device monitor`.
- Filtro por ID em software, alterável em runtime por comandos digitados no monitor serial: ver tudo ou só IDs específicos.
- Contadores de quadros recebidos, exibidos e perdidos (overflow), consultáveis por comando, para saber se os dados estão completos.
- Fora do escopo: transmissão de quadros (a simulação do `0x208` virá em uma change futura), modo "mostrar só quando o payload mudar" (`on-change`), decodificação de sinais, gravação em arquivo, filtros por hardware do MCP2515.

## Capabilities

### New Capabilities
- `can-bus-capture`: recepção passiva e sem perda silenciosa de quadros CAN do HS.IS pelo MCP2515 (listen-only, 500 kbit/s, cristal de 8 MHz, pino INT em D3, buffer em RAM) e contabilização de quadros recebidos e perdidos.
- `frame-id-filter`: seleção em runtime dos IDs CAN que serão exibidos (todos ou uma lista), controlada por comandos de texto no serial.
- `serial-frame-output`: formato da linha de saída por quadro e do relatório de estatísticas/filtros ativos na serial.

### Modified Capabilities
<!-- Nenhuma: o projeto ainda não possui specs. -->

## Impact

- Código: `src/main.cpp` (hoje apenas o template do PlatformIO) e eventuais módulos novos em `src/` ou `lib/`.
- `platformio.ini`: dependência da biblioteca do MCP2515 (`autowp/arduino-mcp2515`) e `monitor_speed = 500000`.
- Hardware: Nano ATmega328, MCP2515 com cristal de 8 MHz (marcação EAS8000, assumido como 8,000 MHz), CS=D10, SI=D11, SO=D12, SCK=D13, INT=D3; ligação direta ao barramento HS.IS, jumper de 120 Ω aberto e GND comum com o carro.
- Sem APIs externas e sem alteração de dados persistidos.
