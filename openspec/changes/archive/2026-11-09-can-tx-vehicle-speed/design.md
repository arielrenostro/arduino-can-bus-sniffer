# Design

## Context

Ver `proposal.md` (Why). O carro não tem ABS nem caixa automática, então o `0x388` (`VEHICLE_DYNAMICS_ENGINE`, 5 bytes, a cada 40 ms, enviado pela ECU do usuário para a BSI) é a única fonte de velocidade. Capturado com o carro parado: `00 00 00 03 AF` (velocidade 0, contador de distância 3, aceleração 0 m/s²). O usuário **não consegue alterar o firmware da ECU**, e ela continua enviando o `0x388`.

A escala da velocidade não consta no mapa do AEE2001. As definições do AEE2004/AEE2010 usam km/h × 0,01 (65535 = inválido), e a aceleração capturada (`0xAF` = 0 m/s² na fórmula × 0,08 − 14 dessas arquiteturas) sugere que as escalas valem para este carro. Premissa não confirmada: a velocidade ser km/h × 0,01.

Premissas do usuário: testes com o carro parado.

## Goals / Non-Goals

**Goals:**
- Fazer a BSI ver uma velocidade escolhida, sem colidir com a ECU.
- Não alterar o contador de distância (o odômetro do painel é permanente) nem a aceleração.
- Lógica pura e testável no PC.

**Non-Goals:**
- Gerar o `0x388` sem a ECU, ou enviar `0x38D` e `0x389` (não existem neste carro).
- Aplicar o mesmo mecanismo ao `0x208` (possível mudança futura).

## Decisions

**1. Seguir o quadro da ECU, sem temporizador.**
O `0x388` é enviado só como reação a um `0x388` recebido. Isso sincroniza o nosso quadro com o fim do quadro da ECU (o controlador transmite depois do intervalo mínimo entre quadros), então os dois nunca iniciam juntos e não há erro de bit nem retransmissão. *Alternativa:* temporizador de 40 ms (como o `0x208`): colide com a ECU em rajadas, gera erros e faz a BSI ver valores alternados. *Alternativa:* só alterar a ECU: o usuário não consegue.

**2. Cópia com bytes 1-2 trocados.**
O quadro enviado repete o comprimento e os bytes recebidos, trocando só a velocidade. O contador de distância e a aceleração continuam os da ECU, e o odômetro não anda. Frames com menos de 2 bytes são ignorados. *Alternativa:* montar um quadro próprio com contador fixo: exigiria acertar um contador que a ECU já controla.

**3. Montagem pura em `VehicleSpeed.h`.**
`SpeedState` (`active`, `kmh`, `sent`, `failed`) e `buildSpeedFrame(src, len, kmh, out)`, sem dependência do Arduino. O parser de comandos recebe um ponteiro opcional para `SpeedState`, como já faz com o `TxState`.

**4. Modo do controlador.**
`active` de qualquer um dos envios (`0x208` ou velocidade) mantém o controlador em modo normal; ele só volta a somente escuta quando ambos estão desligados.

**5. A reação ocorre na recepção.**
Ao ler um `0x388` do MCP2515, antes de enfileirar para exibição, o laço chama a função que monta e envia a cópia. Como o MCP2515 não recebe os próprios quadros, não há laço de retroalimentação.

## Risks / Trade-offs

- **A BSI vê dois `0x388` seguidos (o da ECU, com 0, e o nosso, com a velocidade)** → a BSI pode usar o último, a média ou o primeiro; não há garantia. Mitigação: o nosso quadro vem poucas centenas de microssegundos depois, e o velocímetro do painel mostra rapidamente o efeito; se a BSI usar o valor errado, o próximo passo é impedir o quadro da ECU.
- **Velocidade falsa com o carro parado** → o painel, o travamento automático das portas (se habilitado), o limpador e as luzes podem reagir. Mitigação: só com comando explícito, `v off` desliga, e o README avisa.
- **Escala não confirmada** → se a BSI não ler km/h × 0,01, o velocímetro mostrará outro valor. Mitigação: calibrar pelo velocímetro (`v 60` deve marcar ~60) e, se preciso, ajustar a escala.
- **Odômetro** → preservado porque o contador de distância é o da ECU. Se o odômetro do painel andar mesmo assim, `v off` imediatamente.
- **Se a ECU parar de enviar o `0x388`, nada é enviado** → a BSI fica sem velocidade; é o comportamento correto (o sistema não inventa quadros).
- **Quadro estendido ou remoto** → ignorado, como no restante.
