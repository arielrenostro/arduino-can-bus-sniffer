# Design

## Context

Ver `proposal.md` (Why). Estado atual: o firmware captura em modo somente escuta a 250 kbit/s, com o laço principal sem bloqueios (drena o MCP2515 por polling do INT, emite pela serial só quando a linha cabe no buffer de TX) e o parser de comandos em `lib/sniffer_core`.

Premissas informadas pelo usuário:
- Os testes serão feitos **com o carro parado**.
- A ECU programável **continua conectada e enviando o `0x208`**; por enquanto não há como pará-lo. Se der problema, o usuário tentará tirá-la do barramento.

O quadro de referência vem de uma captura real com o motor em marcha lenta: `1F 58 2D 00 40 2D 2D 2D`. Escalas usadas (rpm × 0,125; torque Nm = cru × 2 − 100; pedal % = cru × 0,5) vêm das definições do PSA-RE para AEE2004/AEE2010 e são coerentes com as capturas do usuário; não foram confirmadas para o AEE2001.

## Goals / Non-Goals

**Goals:**
- Gerar o `0x208` periodicamente com campos nomeados, partindo do quadro de referência.
- Manter o padrão seguro: sempre iniciar em somente escuta e só transmitir após comando.
- Lógica do quadro e dos comandos pura e testável no PC.

**Non-Goals:**
- Enviar outros IDs ou bytes crus.
- Evitar colisões com o `0x208` da ECU (modo "seguir a ECU"); fica como possível mudança futura.
- Recuperar bus-off ou reportar contadores de erro do controlador.

## Decisions

**1. O quadro é guardado em campos "crus", não em valores físicos.**
`EngineDynamics` (em `lib/sniffer_core`) guarda `rpmRaw` (16 bits), `torqueRaw`, `demandRaw`, `cruise`, `brake`, `ac` e `diag`. O padrão de referência é definido nesses crus (8024, 45, 0, 0, 0, 0, 1), o que reproduz exatamente `1F 58 2D 00 40 2D 2D 2D`. Os comandos convertem rpm, Nm e % para crus (rpm × 8; (Nm + 100 + 1) / 2 em inteiros, que arredonda para cima; % × 2) e `encode()` monta os 8 bytes. *Alternativa:* guardar valores físicos e converter na hora do envio; descartada porque 1003 rpm e −10 Nm só são exatos nos crus, e a referência não seria reproduzida bit a bit.

**2. Um estado de envio compartilhado, `TxState`.**
`TxState` reúne o `EngineDynamics`, `active`, `periodMs` (padrão 10) e os contadores `sent` e `failed`. O parser de comandos altera `TxState`; o laço principal reage às mudanças de `active`. O parser recebe um ponteiro opcional para `TxState` (sem ele, `t` é "comando desconhecido"), então o uso e os testes atuais não mudam.

**3. Comando de campo liga o envio e reinicia a referência só ao ligar.**
Se o envio está desligado, um comando de campo válido faz `reset()` para a referência, aplica o campo e liga. Se está ligado, só aplica o campo. `t on` faz o `reset()` e liga sem alteração. Valores inválidos são rejeitados antes de qualquer mudança, para que um erro não ligue o envio nem altere o quadro. O período (`t period`) não faz parte do conteúdo e persiste.

**4. Troca de modo do MCP2515 pelo laço principal.**
O controlador só transmite fora do modo somente escuta. Quando `active` passa a verdadeiro, o `loop()` chama `setNormalMode()`; quando passa a falso, `setListenOnlyMode()`. Se a troca falhar, o envio é desligado e um erro `#` é emitido. Efeito colateral aceito: em modo normal o controlador dá ACK aos quadros dos outros nós e pode emitir quadros de erro; por isso o modo normal só vale enquanto o envio está ligado.

**5. Agendamento por `micros()` com acúmulo.**
O próximo envio é `nextSendUs += periodMs × 1000`, o que evita deriva. Se o laço atrasar por mais de um período, `nextSendUs` é ressincronizado para agora (sem rajada de quadros atrasados). O envio é uma transferência SPI curta (`sendMessage`). Como o laço não bloqueia, a variação do período fica bem abaixo de 1 ms.

**6. Falha de envio é contada, não repetida.**
`sendMessage` devolve erro quando os três buffers de TX estão ocupados (por exemplo, sem ninguém dando ACK). Isso incrementa `failed` e o próximo período tenta de novo. Quadros já enfileirados são retransmitidos pelo próprio controlador até receberem ACK.

**7. Apenas o ID `0x208`.**
O ID é constante no código, sem parâmetro. O sniffer fica ligado a um barramento de veículo real e restringir o ID evita enviar outro quadro por engano.

## Risks / Trade-offs

- **Colisão com o `0x208` da ECU** → Dois nós transmitindo o mesmo ID quase ao mesmo tempo causam erro de bit, quadros de erro e retransmissões; os contadores de erro dos dois sobem e um deles pode ficar passivo ou entrar em bus-off e perder a comunicação. Mesmo sem colisão, a BSI e o painel recebem valores alternados (o da ECU e o nosso). Mitigação: usar `t off`, desligar o `0x208` da ECU ou tirá-la do barramento; se o problema persistir, uma mudança futura pode enviar o nosso quadro logo após cada `0x208` da ECU. Esta mudança não resolve isso.
- **Escalas não confirmadas para o AEE2001** → rpm, torque e pedal usam as escalas de arquiteturas mais novas. Mitigação: o painel (conta-giros) valida a rotação; os crus reproduzem a referência exata.
- **Cruise "ativo" é só um aviso** → o painel pode mostrar cruise ativo sem a ECU regular a velocidade. Aceito: é o objetivo do teste, e o carro estará parado.
- **Modo normal dá ACK e pode gerar erros se o bit timing estivesse errado** → A velocidade (250 kbit/s) já foi validada na captura.
- **Bus-off não é tratado** → se o controlador entrar em bus-off, o envio falha e `failed` sobe; a recuperação é manual (`t off` e `t on`).
- **Variação do período** → depende do laço; sem garantia de tempo real. Suficiente para observar a reação da BSI.
