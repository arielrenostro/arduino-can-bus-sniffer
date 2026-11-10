# Proposal

## Why

O 206 do usuário não tem ABS nem caixa automática: a única fonte de velocidade no barramento é o quadro `0x388`, enviado pela ECU dele. Para testar se a BSI ativa o cruise e mostra o "CC" com o carro parado, é preciso que a BSI veja uma velocidade diferente de zero. O usuário não consegue alterar o firmware da ECU, então o Arduino precisa enviar essa velocidade. Como a ECU continua enviando o `0x388`, um envio por temporizador colidiria com ela (o mesmo problema visto com o `0x208`, em que o conta-giros oscila).

## What Changes

- Novo comando `v <km/h>` que liga a substituição de velocidade, e `v off` e `v show`.
- O Arduino **segue o quadro da ECU**: a cada `0x388` recebido, envia logo em seguida uma cópia com os mesmos bytes, exceto a velocidade (bytes 1-2), trocada pelo valor pedido. Não há temporizador próprio, então não há disputa pelo barramento, e o contador de distância e a aceleração continuam sendo os da própria ECU (o odômetro do painel não anda).
- O controlador continua iniciando em somente escuta; o modo normal passa a valer enquanto o envio do `0x208` **ou** a substituição de velocidade estiver ligada.
- Fora do escopo: enviar `0x38D` ou `0x389`, gerar um `0x388` sem a ECU, alterar contador de distância ou aceleração, aplicar o modo "seguir a ECU" ao `0x208`.

## Capabilities

### New Capabilities
- `vehicle-speed-override`: substituição da velocidade do quadro `0x388` seguindo o quadro da ECU, controlada pela serial.

### Modified Capabilities
- `can-bus-capture`: o modo normal (com transmissão) passa a valer também enquanto a substituição de velocidade estiver ligada.

## Impact

- Código: `lib/sniffer_core` (montagem do quadro de velocidade, estado e comandos `v`), `src/main.cpp` (reação ao `0x388` recebido e escolha do modo do controlador).
- Testes: novos testes nativos.
- Documentação: `README.md`.
- Risco operacional: a BSI passa a ver velocidade diferente de zero com o carro parado (travamento automático das portas, limpador, luzes, painel); ver `design.md`.
