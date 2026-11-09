# Design

## Context

Ver `proposal.md` (Why) para a motivação. Restrições que moldam a abordagem:

- **Alvo:** Arduino Nano (ATmega328, 16 MHz, 2 KB de RAM, 32 KB de flash), board `nanoatmega328new`, framework Arduino, PlatformIO. O projeto hoje só tem o template em `src/main.cpp`.
- **Barramento:** HS.IS a 500 kbit/s. Os quadros periódicos já mapeados no PSA-RE (0x208 a 100/s, 0x349 a 50/s, 0x388/0x389/0x38D/0x44D a 25/s cada, 0x412, 0x488, 0x489, 0x50E e outros) somam algumas centenas de quadros por segundo; com os demais IDs, o pico real deve ficar acima disso.
- **Fiação confirmada:** CS=D10, SI=D11 (MOSI), SO=D12 (MISO), SCK=D13, INT=D3. Cristal marcado EAS8000, assumido como 8,000 MHz.
- **Gargalo:** o MCP2515 tem só 2 buffers de recepção e o `Serial` do AVR tem 64 bytes de buffer de transmissão. Uma impressão que bloqueie por mais que ~0,5 ms (tempo de 2 quadros de 8 bytes) já arrisca perder quadros. A 115200 baud não cabe o tráfego em texto; a 500000 baud (50 KB/s) cabe com folga.

## Goals / Non-Goals

**Goals:**
- Captura passiva, sem bloquear a recepção, com perdas sempre visíveis (contadores).
- Lógica pura (filtro, ring buffer, formatação, parsing de comandos) separada do hardware, para ser testável no PC.
- Ponto único de configuração (cristal, bitrate, pinos, baud) para trocar sem caçar valores.

**Non-Goals:**
- Transmissão de quadros (a simulação do 0x208 é outra change). A configuração do modo do controlador deve ficar num único lugar para facilitar essa troca depois.
- Decodificação de sinais, `on-change`, gravação em arquivo, filtros por hardware do MCP2515, suporte a quadros estendidos ou remotos.

## Decisions

**1. Drenar o controlador por polling do pino INT, sem ISR.**
O INT (D3) do MCP2515 fica em nível baixo enquanto houver quadro pendente. O `loop()` lê o nível do pino e, se baixo, lê o quadro por SPI para um ring buffer em RAM. Como a saída serial é não bloqueante (decisão 2), o `loop()` roda rápido o bastante para drenar o controlador sem interrupção. Vantagens: não há SPI dentro de ISR, sem risco de reentrância com o acesso SPI do loop, e ler o nível é mais robusto que latch de borda (não perde evento se dois quadros chegarem seguidos).
*Alternativas:* ISR de borda de descida que só levanta uma flag (ok, mas redundante, e a flag pode ficar desatualizada se vários quadros pendentes forem lidos de uma vez); ISR que lê por SPI (arriscada com o resto do código também usando SPI).

**2. Saída não bloqueante.**
Antes de formatar uma linha, o `loop()` confere `Serial.availableForWrite()` e só emite se couber a linha inteira (≤ 43 bytes, limiar de 48). Se não couber, o quadro fica no ring buffer e o loop volta a drenar o controlador. A linha é montada em um buffer local com conversão hexadecimal manual, sem `printf`/`String`, e escrita com um único `Serial.write(buf, n)`.
*Alternativa:* `Serial.print` direto (simples, mas bloqueia quando o buffer de TX enche, perdendo quadros no MCP2515).

**3. Ring buffer de 32 quadros.**
Cada entrada tem ID (2 B), comprimento (1 B), dados (8 B) e carimbo de tempo `millis()` (4 B): 15 B, ou 480 B no total, cerca de 23% da RAM. Potência de 2 para indexação por máscara. Quando cheio, o quadro novo é descartado e `lost` incrementa (decisão consistente com `can-bus-capture`).
*Nota:* o carimbo é tirado quando o quadro é lido do controlador, não no instante em que chegou ao barramento; o erro é da ordem do atraso do drain (normalmente bem abaixo de 1 ms).

**4. Biblioteca `autowp/arduino-mcp2515`.**
API enxuta (`setBitrate(CAN_500KBPS, MCP_8MHZ)`, `setListenOnlyMode()`, `readMessage()`), suporta os 500 kbit/s com cristal de 8 MHz. Configuração do controlador para aceitar todos os IDs (máscaras zeradas). Estouro do buffer do controlador é verificado por flag de erro (EFLG) e contado em `lost`.
*Alternativas:* `coryjfowler/MCP_CAN_lib` (muito usada, mais código e semântica de filtros diferente); driver SPI próprio (controle total, mas é trabalho a mais sem benefício agora).

**5. Filtro em software, lista de até 16 IDs.**
Um vetor de 16 `uint16_t` (32 B) com um contador; filtro ativo = contador > 0. A checagem é uma busca linear de no máximo 16 comparações por quadro, desprezível frente ao tempo de quadro de ~230 µs no barramento. Os filtros do MCP2515 ficam abertos.
*Alternativa:* filtros por hardware (só 6 IDs exatos, exigem reconfigurar o controlador a cada mudança e escondem quadros do contador de recebidos, contrariando a spec).

**6. Comandos por linha, não bloqueantes.**
Os caracteres da serial são acumulados num buffer de linha de 48 B; `\r` ou `\n` fecham o comando e linhas vazias são ignoradas. Comandos: `f ...` e `s`. Respostas começam com `#`. Linhas maiores que o buffer são descartadas com erro `#`.

**7. Serial a 500000 baud.**
Com `U2X`, 16 MHz e UBRR=3 o baud sai exato. `platformio.ini` recebe `monitor_speed = 500000`. Constante única para o caso de precisar baixar para 250000.

**8. Estrutura do código.**
- `lib/sniffer_core/`: lógica sem dependência do Arduino: `FrameRing`, `IdFilter`, formatação da linha, parser de comandos. Testável com um env `native` do PlatformIO (Unity) no PC.
- `src/main.cpp`: configuração, ligação com o MCP2515 e com o `Serial`, laço principal.
- Um cabeçalho de configuração (`config.h`) com cristal, bitrate, pinos, baud e capacidades (ring e lista de filtro).

## Risks / Trade-offs

- **Cristal diferente de 8 MHz** → o MCP2515 configurado com o clock errado não recebe nada e nada acusa o erro. Mitigação: o cristal é uma constante em `config.h`; a mensagem de início mostra o bitrate e o clock configurados; se não aparecer tráfego, trocar para 16 MHz.
- **Perda sob carga alta sem filtro** → o ring buffer enche. Mitigação: o contador `lost` aparece no comando `s`; usar `f <ids>` reduz a saída; o baud pode subir/descer pela constante.
- **Adaptador USB-serial não aceitar 500000 baud** → saída ilegível. Mitigação: constante única para trocar para 250000 (o `monitor_speed` acompanha).
- **Terminação e massa** → jumper de 120 Ω do módulo deve ficar aberto (o barramento já está terminado nas duas pontas) e o GND do módulo precisa ser comum com o do carro; sem isso a leitura pode falhar de modo difícil de diagnosticar. Entra na documentação de ligação, não no firmware.
- **Barramento errado** → o MCP2515 só lê CAN; o barramento de conforto (VAN, 125 kbit/s) não é CAN. Entra na documentação.
- **Quadros estendidos ou remotos** → fora de escopo; se o controlador entregar um, é contado como recebido e não é exibido.
- **Carimbo de tempo aproximado** → ver decisão 3; não serve para medir latência fina entre quadros.
