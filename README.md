# can-sniffer

Sniffer CAN passivo (somente escuta) para o barramento **HS.IS** do Peugeot 206 (AEE2001, 250 kbit/s), com Arduino Nano + módulo MCP2515. Mostra todos os quadros no monitor serial e permite filtrar por ID em tempo de execução.

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
- O firmware só escuta: não transmite quadros, nem ACK, nem quadros de erro.

## Uso

```
pio run -t upload        # grava no Nano
pio device monitor       # 500000 baud (monitor_speed já está no platformio.ini)
```

Ao iniciar, aparece:

```
# can-sniffer: captura a 250 kbit/s, cristal 8 MHz, somente escuta (f = filtro, s = estado)
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
