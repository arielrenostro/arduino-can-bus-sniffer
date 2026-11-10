# can-sniffer

Sniffer CAN para o barramento **HS.IS** do Peugeot 206 (AEE2001, 250 kbit/s), com Arduino Nano + módulo MCP2515. Mostra todos os quadros no monitor serial e permite filtrar por ID em tempo de execução.

Objetivo: observar o que a BSI e a ECU trocam sobre o controle de cruzeiro (por exemplo `0x412`, enviado pela BSI, e `0x208`, enviado pela ECU), antes de simular o `0x208`.

## Ligação

| MCP2515 | Arduino Nano |
|---|---|
| VCC | 5V |
| GND | GND |
| CS | D10 |
| SI (MOSI) | D11 |
| SO (MISO) | D12 |
| SCK | D13 |
| INT | D3 |
| CAN H / CAN L | fios CAN H / CAN L do HS.IS |

Antes de ligar ao carro:

- **Jumper de 120 Ω do módulo (geralmente J1): deixe aberto.** O barramento já é terminado nas duas pontas; uma terminação extra carrega a linha e prejudica a leitura.
- **GND comum:** o GND do módulo precisa ser o mesmo do carro.
- **Barramento certo:** o MCP2515 só lê **CAN**. O barramento de conforto do 206 é **VAN** (125 kbit/s), que não é CAN. Ligue no HS.IS (CAN H/L a 250 kbit/s nesta arquitetura; outras arquiteturas PSA usam 500 kbit/s).
- O firmware **inicia sempre em somente escuta**: não transmite quadros, nem ACK, nem quadros de erro. Só transmite depois de um comando `t`, `v` ou `l` (ver "Enviando o 0x208", "Substituindo a velocidade" e "Pedidos de lâmpada").

## Uso

```
pio run -t upload        # grava no Nano
pio device monitor       # 500000 baud (monitor_speed já está no platformio.ini)
```

Ao iniciar, aparece:

```
# can-sniffer: captura a 250 kbit/s, cristal 8 MHz, somente escuta (f = filtro, s = estado, t = enviar 0x208, v = velocidade, l = lampadas)
```

Cada quadro vira uma linha `<ms> <ID> [<n>] <bytes em hex>`:

```
12345 412 [8] 00 20 00 00 00 00 00 00
```

Linhas que começam com `#` são mensagens do sistema (respostas de comando, erros).

### Comandos

Digite no monitor e tecle Enter. IDs em hexadecimal (com ou sem `0x`), de `0` a `7FF`.

| Comando | Efeito |
|---|---|
| `f 412 208` | mostra **só** esses IDs (substitui a lista; até 16) |
| `f +388` / `f -388` | adiciona / remove IDs da lista (aceita vários: `f +388 -412`) |
| `f off` | mostra todos os IDs (padrão ao ligar) |
| `s` | filtro ativo e contadores: recebidos, exibidos, perdidos |

Remover o último ID da lista desativa o filtro. Um comando inválido (ID acima de `7FF`, não hexadecimal, lista com mais de 16) é rejeitado e o filtro não muda. O filtro afeta só a exibição: os quadros ocultos continuam contados como recebidos.

### Contador de perdidos

`perdidos` soma os quadros descartados porque o buffer interno encheu ou o MCP2515 estourou. Se for maior que zero, os dados estão incompletos: use `f <ids>` para reduzir a saída. Os estouros do MCP2515 são contados uma vez por ocorrência de flag, então o número é um mínimo.

## Enviando o 0x208

Gera o quadro `0x208` (ENGINE_DYNAMICS, normalmente enviado pela ECU) para ver como a BSI e o painel reagem. **Só o ID `0x208` pode ser enviado.**

Todo envio começa do quadro de referência capturado com o motor em marcha lenta:

```
1F 58 2D 00 40 2D 2D 2D      1003 rpm, torque -10 Nm, pedal 0%, cruise 0, freio 0, A/C 0, DIAG 1
```

O primeiro comando de campo já liga o envio: `t cruise 2` envia a referência com o cruise em 2 (`... 00 48 ...`).

| Comando | Efeito | Codificação |
|---|---|---|
| `t on` | liga o envio com a referência | |
| `t off` | para e volta a somente escuta | |
| `t rpm <0..8191>` | rotação | rpm × 8, bytes 1-2 |
| `t torque <-100..408>` | torque em Nm | (Nm + 100) / 2, bytes 3, 6, 7 e 8 |
| `t pedal <0..100>` | acelerador em % | % × 2, byte 4 |
| `t cruise <0..3>` | estado do cruise | byte 5, bits 3-2 |
| `t brake <0\|1>` | contato secundário do freio | byte 5, bit 1 |
| `t ac <0\|1>` | pedido do compressor do A/C | byte 5, bit 0 |
| `t diag <0\|1>` | `DIAG_MUX_ON` | byte 5, bit 6 |
| `t period <ms>` | período do envio (2..5000, padrão 10) | não liga o envio |
| `t show` | estado, campos, quadro em hex e contadores | |

- Com o envio ligado, um comando de campo altera só aquele campo; os demais continuam.
- Depois de `t off`, o próximo envio volta a partir da referência (o período é mantido).
- Valores inválidos são rejeitados e não alteram o quadro nem ligam o envio.
- `t torque 110` produz `0x69`, o valor visto com o motor parado. O torque tem resolução de 2 Nm.
- Os valores de `t cruise` seguem a tabela do AEE2010 (0 espera, 1 retomado pelo pedal, 2 ativo, 3 inibido); não estão confirmados para o 206.
- `t show` mostra enviados e falhas; falha é um quadro que não coube nos buffers de envio do MCP2515 (por exemplo, ninguém dando ACK).

### Avisos

- **Só com o carro parado.** A caixa automática (BVA), se houver, também recebe o `0x208`.
- **A ECU também envia o `0x208`.** Dois nós enviando o mesmo ID causam erros de bit e retransmissões, e a BSI recebe valores alternados (o da ECU e o seu). Se isso atrapalhar, pare o `0x208` na ECU ou tire-a do barramento.
- Enquanto envia, o MCP2515 fica em **modo normal**: dá ACK aos quadros dos outros nós e pode gerar quadros de erro.
- "Cruise ativo" aqui só informa o painel; quem regula a velocidade é a ECU.

## Substituindo a velocidade (0x388)

Neste carro (sem ABS nem caixa automática) a única fonte de velocidade da BSI é o quadro `0x388`, enviado pela ECU: bytes 1-2 velocidade, 3-4 contador de distância, 5 aceleração (parado: `00 00 00 03 AF`). Os comandos `v` fazem a BSI enxergar outra velocidade, sem mexer na ECU.

| Comando | Efeito |
|---|---|
| `v <0..300>` | liga a substituição com essa velocidade em km/h (ou altera, se já ligada) |
| `v off` | desliga |
| `v show` | estado, velocidade e contadores de enviados e falhas |

Exemplo de `v show`:

```
# v: ligado, velocidade = 60 km/h
# v enviados = 250
# v falhas = 1
```

**Como funciona:** o Arduino não tem relógio próprio para esse quadro. A cada `0x388` que a ECU envia, ele manda logo em seguida uma **cópia com só a velocidade trocada** (bytes 1-2 = km/h × 100, big-endian; 60 km/h = `17 70`). Assim os dois não disputam o barramento, e o contador de distância e a aceleração continuam sendo os da ECU, então o odômetro do painel **não anda**. Se a ECU não enviar o `0x388`, nada é enviado.

### Avisos

- **Só com o carro parado.** A BSI passa a ver velocidade diferente de zero: o painel, o travamento automático das portas (se habilitado), o limpador e as luzes podem reagir. Não deixe a chave dentro do carro trancado.
- **A BSI recebe dois `0x388` por ciclo** (o da ECU, com a velocidade dela, e o seu logo depois). Não há garantia de qual ela usa; se o velocímetro não mostrar o valor pedido, avise.
- **A escala (km/h × 0,01) vem do PSA-RE para outras arquiteturas e não está confirmada para o 206.** Calibre pelo velocímetro: `v 60` deve marcar cerca de 60.
- Se o **odômetro ou o parcial** mexerem, desligue na hora com `v off`.

## Pedidos de lâmpada (0x488)

O `0x488` (ENGINE_DATA, enviado pela ECU à BSI a cada 100 ms) tem, no byte 4, o pedido da **lâmpada de velas** (bit 1, `PREHEAT_LAMP_REQUEST`) e da **luz de injeção** (bit 2, `MIL_REQUEST`). Os comandos `l` alteram esses dois bits sem mexer na ECU.

| Comando | Efeito |
|---|---|
| `l preheat <0\|1>` | pedido da lâmpada de velas |
| `l mil <0\|1>` | pedido da luz de injeção |
| `l off` | desliga e devolve os dois campos à ECU |
| `l show` | estado e contadores |

O campo que você não comandou continua com o valor que a ECU enviou (aparece como `ecu`). Exemplo de `l show` depois de `l preheat 1`:

```
# l: ligado, preheat=1 mil=ecu
# l enviados = 40
# l falhas = 0
```

**Como funciona:** igual à velocidade. A cada `0x488` da ECU, o Arduino envia logo em seguida uma cópia com só esses bits ajustados (por exemplo, `43 9A FF 04 00 43 1A 45` com `l preheat 1` vira `43 9A FF 06 00 43 1A 45`). O resto do quadro é o da ECU.

### Avisos

- **Só com o carro parado.**
- **A lâmpada pode piscar:** a BSI recebe o quadro da ECU e o seu, e pode alternar entre os dois em vez de ficar com o último.
- **A lâmpada de velas é de motor diesel;** em painel de carro a gasolina ela pode nem existir. A luz de injeção existe em qualquer painel.
- **`l mil 0` esconde no painel um aviso que a ECU pediu**, o que pode mascarar uma falha real. Use só para teste e volte com `l off`.

## Configuração

Tudo que depende do hardware está em [`include/config.h`](include/config.h):

- **Cristal:** a marcação "EAS8000" foi assumida como 8 MHz. Se nada aparecer, troque `CFG_CAN_CLOCK` para `MCP_16MHZ` (e `CFG_CAN_CLOCK_MHZ` para 16).
- **Baud:** `CFG_SERIAL_BAUD` = 500000. Se o adaptador USB-serial do seu Nano não aceitar, use 250000 e ajuste `monitor_speed` no `platformio.ini`.
- Pinos, velocidade do barramento e capacidades (ring de 32 quadros, filtro de 16 IDs).

Se a inicialização falhar, o firmware repete a cada 5 s `# erro: falha ao inicializar o MCP2515 ...`: confira fiação, cristal e alimentação.

## Lendo os quadros de cruzeiro

As definições dos quadros estão no PSA-RE (`buses/AEE2001.full/HS.IS`). A numeração de bits é `<byte>.<bit>`, com bytes a partir de 1 e big-endian. Alguns sinais:

- `0x412` (BSI): byte 1 bit 5 = pedal de freio; byte 2 bits 5-4 = botões do cruzeiro; byte 2 bit 1 = cruzeiro ligado.
- `0x208` (ECU, a cada 10 ms): byte 5 bits 3-2 = estado do cruzeiro.

Para ver só esses dois: `f 412 208`.

## Testes

A lógica pura (ring buffer, filtro, formatação, comandos) fica em `lib/sniffer_core` e é testada no PC:

```
pio test -e native
```
