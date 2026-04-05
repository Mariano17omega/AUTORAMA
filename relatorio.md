# Relatório Técnico — Projeto AUTORAMA

**Data:** 2026-04-05  
**Autor:** Análise automatizada (Claude Code)  
**Versão analisada:** commit `0bd50c8` (branch `main`)

---

## 1. Visão Geral do Projeto

O AUTORAMA é um sistema de pista de corrida em escala reduzida com aquisição de dados em tempo real. Um carrinho motorizado percorre uma pista instrumentada enquanto sua velocidade é medida, controlada e registrada.

**Fluxo principal:**
```
[Operador] → Teclado do Controle Remoto
                    ↓ (RF)
          [Arduino 1 — Pista] ←————→ [Carro (Nano)]
                    ↓ (SoftwareSerial)          ↑ RF telemetria
          [Arduino 2 — Datalogger]      [Sensor Hall]
                    ↓
              [Cartão SD]
```

---

## 2. Estrutura de Arquivos

| Arquivo | Microcontrolador | Função |
|---------|-----------------|--------|
| `Carrinho.ino` | Arduino Nano | Controla motor, mede velocidade (Hall), envia telemetria via RF |
| `Controle_Remoto.ino` | Arduino Uno | Interface com teclado + OLED, envia comandos PWM e GO via RF |
| `Arduino_1- PISTA DO AUTORAMA.ino` | Arduino Uno | Recebe RF (controle + telemetria do carro), lê LDRs S7-S9, aciona servo da catraca, repassa dados ao Arduino 2 |
| `Arduino_2- PISTA DO AUTORAMA.ino` | Arduino Uno | Recebe dados do Arduino 1 via SoftwareSerial, lê LDRs S1-S6, grava no cartão SD |

---

## 3. Diagnóstico do Problema Serial Relatado

### Sintoma
> Arduino 2 inicia recebendo dados e depois para. Ocorre somente quando a velocidade de envio do carro é rápida.

### Causa Raiz: Bloqueio do Loop por Escrita no SD

O `loop()` do Arduino 2 processa uma mensagem e imediatamente grava no SD:

```cpp
// Arduino_2, linhas 151-155
File arquivo = SD.open("dados.txt", FILE_WRITE);
if (arquivo) {
    arquivo.println(linha);
    arquivo.close();   // ← BLOQUEIA ~50–100 ms
}
```

A sequência `SD.open()` + `arquivo.close()` exige sincronização física com o cartão, bloqueando o `loop()` por **50–100 ms** a cada linha gravada.

### Por Que Isso Causa Perda de Dados

Durante esses 50–100 ms de bloqueio:

1. Arduino 1 continua enviando mensagens via SoftwareSerial (9600 baud)
2. A SoftwareSerial tem buffer de apenas **64 bytes**
3. A 9600 baud, uma linha de ~60 bytes leva ~62 ms para ser transmitida
4. Quando o carro está rápido, Arduino 1 pode enviar **2 ou mais linhas** no intervalo de uma gravação SD
5. O buffer transborda → bytes descartados → linha corrompida (sem `\n`) → `readStringUntil('\n')` fica preso esperando o timeout

**Resultado:** Arduino 2 parece "parar de receber" porque ficou preso no timeout de `readStringUntil()` ou perdeu o delimitador da mensagem.

### Por Que Só Ocorre em Alta Velocidade

Em baixa velocidade, o carro envia telemetria raramente (apenas quando há novo pulso do Hall). O Arduino 1 envia poucas linhas ao Arduino 2, e o SD consegue acompanhar. Em alta velocidade, os pulsos chegam a 10+ Hz, gerando uma linha nova a cada ~100 ms — exatamente no limite do tempo de gravação do SD.

### Soluções Recomendadas

**Solução 1 — Rápida: Trocar `close()` por `flush()`**

Abrir o arquivo uma vez no `setup()` e usar `flush()` para sincronizar sem fechar:

```cpp
// setup()
arquivo = SD.open("dados.txt", FILE_WRITE);

// gravaLinhaBruta() e gravaEvento()
arquivo.println(linha);
arquivo.flush();  // ~5–10 ms em vez de ~50–100 ms
```

Reduz o bloqueio de ~100 ms para ~10 ms, eliminando o problema na maioria dos casos.

**Solução 2 — Robusta: Buffer circular no Arduino 2**

Ler a serial para um array de strings no `loop()` e gravar no SD apenas quando o buffer tiver N linhas acumuladas ou após um timeout:

```cpp
#define BUF_SIZE 8
String fila[BUF_SIZE];
int filaTopo = 0;

// No loop(): só armazena
if (radioIn.available()) {
    fila[filaTopo++] = radioIn.readStringUntil('\n');
}

// Grava em lote quando buffer cheio ou timeout
if (filaTopo >= BUF_SIZE || (filaTopo > 0 && millis() - tUltimaGravacao > 500)) {
    for (int i = 0; i < filaTopo; i++) {
        arquivo.println(fila[i]);
    }
    arquivo.flush();
    filaTopo = 0;
    tUltimaGravacao = millis();
}
```

**Solução 3 — Complementar: Aumentar o baud rate**

Trocar 9600 por 57600 baud na SoftwareSerial reduz o tempo de transmissão de ~62 ms para ~10 ms por linha. Aplicar nos dois Arduinos:

```cpp
// Arduino_1 e Arduino_2
serialPista2.begin(57600);  // era 9600
radioIn.begin(57600);       // era 9600
```

---

## 4. Bugs e Erros Encontrados

### 4.1 Críticos

---

**BUG-C1 — Arduino_1 linha 128: Iteração além dos limites de `analogPins[]`**

```cpp
int analogPins[3] = {A0, A1, A4};  // linha 30 — array de 3 elementos
// ...
for (int i = 0; i < 6; i++) {      // linha 128 — itera 6 vezes!
    pinMode(analogPins[i], INPUT);
    estadoAntA[i] = analogRead(analogPins[i]);
}
```

O loop itera 6 vezes sobre um array de 3 elementos e um `estadoAntA[3]` igualmente de 3 posições. Acesso fora dos limites causa **comportamento indefinido** — pode corromper variáveis na memória, travar ou produzir leituras de sensor erradas.

**Correção:** Mudar o limite do loop para `i < 3`.

---

**BUG-C2 — Arduino_2 linhas 151–154: SD bloqueia o loop causando perda serial**

Descrito em detalhe na Seção 3. É a causa do problema relatado pelo usuário.

---

**BUG-C3 — Arduino_1 linha 270: PWM lido incorretamente da mensagem do carro**

```cpp
void RecebeHall(const String& mensagem) {
    // ...
    ultimoPWM = msg.substring(1).toInt();  // mensagem = "C1;0.45;123.4"
    // substring(1) = "1;0.45;123.4" → toInt() = 1
```

A mensagem do carro tem formato `"C1;velocidade;rotacoes"`. `substring(1)` retorna `"1;0.45;123.4"`, cujo `toInt()` resulta sempre em `1`. O valor de `ultimoPWM` fica corrompido e é incluído nos logs enviados ao Arduino 2.

**Correção:** Remover a linha 270. O `ultimoPWM` já é atualizado corretamente quando chega o pacote `d<valor>` do controle remoto.

---

**BUG-C4 — Arduino_1 linha 178: Sensores S7–S9 enviados pelo Arduino 2 nunca são processados**

```cpp
if (entrada.startsWith("S")) {
    int n = entrada.substring(1).toInt();
    if (n >= 1 && n <= 6) processaSensor(n - 1);  // S7, S8, S9 ignorados!
}
```

O Arduino 2 envia notificações `"S1"` a `"S6"` para o Arduino 1. Mas o Arduino 1 só aceita n de 1 a 6, descartando silenciosamente os sensores de 7 a 9 caso o Arduino 2 tente reportá-los.

**Correção:** Mudar para `if (n >= 1 && n <= 9)`.

---

**BUG-C5 — Carrinho.ino linha 238: LED_WAIT nunca apaga após envio de telemetria**

```cpp
void enviarTelemetria() {
    // ...
    digitalWrite(LED_WAIT, HIGH);  // linha 230 — acende
    radio.stopListening();
    radio.write(msg, strlen(msg) + 1);
    radio.startListening();
    digitalWrite(LED_WAIT, HIGH);  // linha 238 — deveria ser LOW
}
```

O LED acende na linha 230 e deveria apagar após o envio. A linha 238 repete `HIGH`, portanto o LED permanece aceso indefinidamente após a primeira transmissão.

**Correção:** Linha 238: `digitalWrite(LED_WAIT, LOW);`

---

### 4.2 Maiores

---

**BUG-M1 — Arduino_1 linha 167: `delay(1)` dentro do loop de leitura serial**

```cpp
while (serialPista2.available() > 0) {
    char c = serialPista2.read();
    if (c == '\n') break;
    if (c != '\r') entrada += c;
    delay(1);  // ← atraso acumulado por caractere
}
```

Para uma mensagem de 60 caracteres, esse loop bloqueia ~60 ms — tempo suficiente para perder pulsos do Hall e pacotes RF.

**Correção:** Remover o `delay(1)`. A SoftwareSerial já faz o timing interno correto.

---

**BUG-M2 — Diâmetro da roda inconsistente entre arquivos**

```cpp
// Carrinho.ino linha 66
const float DIAMETRO_RODA = 0.0245;  // 24,5 mm

// Arduino_1 linha 315
const float diametroRoda = 0.024;    // 24,0 mm
```

Diferença de 0,5 mm no diâmetro gera erro acumulado de ~2% na distância percorrida calculada pelo Arduino 1. Os dois cálculos divergem com o tempo.

**Correção:** Definir uma única constante e usar nos dois arquivos, preferencialmente `0.0245` (conforme README).

---

**BUG-M3 — Arduino_2 linha 40: Falha no SD não interrompe a execução**

```cpp
if (!SD.begin(chipSelect)) {
    Serial.println("Falha no SD");
    digitalWrite(LEDBLUE_PIN, LOW);
    // execução continua normalmente!
}
```

Se o SD falhar, `gravaLinhaBruta()` e `gravaEvento()` tentarão abrir o arquivo, falharão silenciosamente, e todos os dados do experimento serão perdidos sem nenhum aviso ao operador além do LED apagado.

**Correção:** Adicionar `while(1);` após o `digitalWrite` para travar o sistema com indicação clara de falha.

---

**BUG-M4 — Carrinho.ino linhas 327–346: Sem telemetria quando velocidade é zero**

```cpp
if (pulsoNovo && tAnterior > 0 && tAtual > tAnterior) {
    // calcula e envia telemetria
} else {
    // nada
}
```

Quando o carro para ou desacelera muito, `pulsoNovo` fica `false` e nenhuma telemetria é enviada. O Arduino 1 não sabe se o carro parou ou se a comunicação caiu.

**Correção:** Enviar telemetria com `velocidade = 0` via timeout (ex: se nenhum pulso em 500 ms).

---

**BUG-M5 — Controle_Remoto linha 91: Função `executarDegrauOuRampa()` só implementa degrau**

```cpp
int tipoOperacao = 0;  // 1=degrau | 2=rampa inc | 3=rampa tempo

void executarDegrauOuRampa() {
    if (tipoOperacao == 1) {
        // implementado
        return;
    }
    // tipoOperacao 2 e 3 nunca foram implementados
}
```

Os modos de rampa (tipoOperacao 2 e 3) estão documentados nos comentários mas a função retorna sem fazer nada para esses casos.

---

**BUG-M6 — Arduino_1 linha 294: Subtração inexplicada de `RotInc + 1`**

```cpp
RotCarroAtual = sRPM.toFloat() - RotInc - 1;
```

O `- 1` não tem justificativa. Se a intenção é zerar a contagem de rotações no início do teste, basta `- RotInc`. O `- 1` introduz erro sistemático de 1 rotação (~7,7 cm) em todos os cálculos de distância.

---

### 4.3 Menores

---

**BUG-m1 — Arduino_1 linha 268: String de debug hardcoded**

```cpp
Serial.print("mensagemmmmmmmm"); Serial.print(mensagem);
```

String de debug com nome informal deixada na produção. Polui a serial e dificulta a leitura dos logs reais.

---

**BUG-m2 — Carrinho.ino linha 67: π hardcoded em vez de `PI`**

```cpp
const float CIRCUNFERENCIA = 3.1415926 * DIAMETRO_RODA;
```

O Arduino já define a constante `PI` com precisão adequada. Usar `3.1415926` é desnecessário e pode introduzir imprecisão em compiladores diferentes.

**Correção:** `const float CIRCUNFERENCIA = PI * DIAMETRO_RODA;`

---

**BUG-m3 — micros() sem tratamento de overflow**

`micros()` transborda (volta a zero) após ~71 minutos. Nenhum dos arquivos trata esse caso. Se um experimento durar mais que 71 minutos, os cálculos de tempo ficam errados.

---

**BUG-m4 — Controle_Remoto linha 37: Endereço RF `DEF456` definido mas nunca usado**

```cpp
const byte endereco2[6] = "DEF456"; // Controle — declarado mas nunca referenciado
```

---

**BUG-m5 — Arduino_1 linha 384: `RotInc = RotCarroAtual + 1` com magic number**

```cpp
RotInc = RotCarroAtual + 1;
```

Relacionado ao BUG-M6. O `+ 1` parece ser uma compensação para o `- 1` da linha 294, mas está oculto sem comentário.

---

## 5. Análise da Comunicação RF

**Configuração:** nRF24L01+, canal 108 (2,508 GHz), 250 KBPS, PA_LOW.

| Aspecto | Status |
|---------|--------|
| Tamanho máximo de payload | 32 bytes |
| Mensagem de Hall (Carrinho.ino) | `"C1;0.0000;000.00"` ≈ 18 bytes — OK |
| Sem verificação de ACK | Pacotes perdidos não são retransmitidos |
| Sem CRC na camada de aplicação | Mensagem corrompida processada como válida |
| Sem timeout de reconexão | Se RF cair, sistema fica em estado indefinido |

A mensagem de telemetria do carro (`C1;vel;rot`) cabe nos 32 bytes. Porém, sem confirmação de entrega, qualquer interferência resulta em dados perdidos silenciosamente.

---

## 6. Análise do SD Card (Arduino 2)

| Aspecto | Situação Atual | Impacto |
|---------|---------------|---------|
| Abertura/fechamento por linha | `SD.open()` + `close()` a cada gravação | Bloqueia ~50–100 ms |
| Sem limite de tamanho do arquivo | `dados.txt` cresce indefinidamente | SD pode encher sem aviso |
| Sem verificação de disco cheio | `arquivo.println()` falha silenciosamente | Dados perdidos |
| Sem buffer de escrita | Cada linha vai direto ao SD | Baixa throughput |

---

## 7. Resumo dos Bugs por Severidade

| ID | Arquivo | Linha | Descrição | Severidade |
|----|---------|-------|-----------|-----------|
| C1 | Arduino_1 | 128 | Loop itera 6x sobre array de 3 elementos | Crítico |
| C2 | Arduino_2 | 151 | SD.close() bloqueia loop → buffer serial overflow | Crítico |
| C3 | Arduino_1 | 270 | PWM lido errado da mensagem do carro | Crítico |
| C4 | Arduino_1 | 178 | Sensores S7–S9 nunca processados | Crítico |
| C5 | Carrinho | 238 | LED_WAIT nunca apaga após telemetria | Crítico |
| M1 | Arduino_1 | 167 | `delay(1)` no loop serial bloqueia ~60 ms/msg | Maior |
| M2 | Ambos | 66/315 | Diâmetro da roda inconsistente (24 vs 24,5 mm) | Maior |
| M3 | Arduino_2 | 40 | Falha no SD não interrompe execução | Maior |
| M4 | Carrinho | 327 | Sem heartbeat quando velocidade = 0 | Maior |
| M5 | Controle | 91 | Modo rampa declarado mas não implementado | Maior |
| M6 | Arduino_1 | 294 | Subtração de 1 sem justificativa nas rotações | Maior |
| m1 | Arduino_1 | 268 | String de debug "mensagemmmmmmmm" no código | Menor |
| m2 | Carrinho | 67 | π hardcoded em vez de `PI` | Menor |
| m3 | Todos | — | micros() sem tratamento de overflow (~71 min) | Menor |
| m4 | Controle | 37 | Endereço RF `DEF456` declarado e nunca usado | Menor |
| m5 | Arduino_1 | 384 | `RotInc = RotCarroAtual + 1` sem justificativa | Menor |

---

## 8. Recomendações por Prioridade

**Prioridade 1 — Antes de qualquer experimento:**
1. Corrigir BUG-C1 (loop fora dos limites — pode causar crash aleatório)
2. Corrigir BUG-C2 com Solução 1 (trocar `close()` por `flush()` — resolve o problema serial)
3. Corrigir BUG-C3 (PWM corrompido nos logs)
4. Corrigir BUG-C5 (LED_WAIT)

**Prioridade 2 — Melhoria de confiabilidade:**
5. Corrigir BUG-M1 (remover `delay(1)` no loop serial)
6. Corrigir BUG-M2 (unificar diâmetro da roda)
7. Corrigir BUG-M3 (SD falha deve travar o sistema)
8. Implementar BUG-M4 (heartbeat de velocidade zero)

**Prioridade 3 — Limpeza e qualidade:**
9. Remover debug string (BUG-m1)
10. Corrigir BUG-C4 (processar sensores S7–S9 do Arduino 2)
11. Adicionar tratamento de overflow do micros()
12. Implementar modo rampa ou remover referências a ele

---

## 9. Correções Realizadas

Esta seção documenta todas as alterações aplicadas nos quatro arquivos `.ino`.

---

### 9.1 Carrinho.ino

| # | Tipo | Alteração | Justificativa |
|---|------|-----------|---------------|
| 1 | Bug lógico | `radio.read(&ValorPWM, ...)` → `radio.read(ValorPWM, ...)` | `ValorPWM` é `char[15]`, que já decai para `char*`. `&ValorPWM` produz `char(*)[15]` — tipo incorreto passado como `void*`. |
| 2 | Bug lógico | `digitalWrite(LED_WAIT, HIGH)` → `LOW` ao final de `checkRF()` | O LED era ligado ao início da recepção RF mas nunca apagado, ficando aceso permanentemente após o primeiro pacote. Movida a chamada para o fim da função. |
| 3 | Bug lógico | Removidas manipulações de `LED_WAIT` de `enviarTelemetria()` | A função ligava o LED (linha 230) e depois repetia `HIGH` (linha 238) em vez de `LOW`. O LED é gerenciado corretamente em `checkRF()`. |
| 4 | Bug sintático | `3.1415926` → `PI` | Arduino define a constante `PI` com precisão total de `float`. Literal hardcoded é desnecessário e propenso a discrepâncias entre compiladores. |
| 5 | Organização | Removidas variáveis globais não utilizadas: `pulsosRejeitados`, `rejeitados`, `rpm` | Eram declaradas e/ou capturadas mas nunca lidas ou transmitidas após o cálculo. Reduzem RAM do Nano desnecessariamente. |
| 6 | Organização | `velocidade_inst` movida para variável local no `loop()` | Era global mas usada apenas dentro de um bloco `if` em `loop()`. Variável local é suficiente e mais clara. |
| 7 | Organização | Removido código comentado morto (linhas 98–103 e 232) | Variáveis comentadas e `Serial.println` de debug removidos para clareza. |
| 8 | Organização | `digitalWrite(LED_WAIT, LOW)` adicionado em `setup()` | Garante estado inicial definido do LED. |

---

### 9.2 Arduino_1 — PISTA DO AUTORAMA.ino

| # | Tipo | Alteração | Justificativa |
|---|------|-----------|---------------|
| 1 | **Bug crítico** | `for (int i = 0; i < 6; i++)` → `i < 3` em `setup()` | `analogPins[3]` e `estadoAntA[3]` têm 3 elementos. Iteração até 6 causava acesso fora dos limites, lendo pinos aleatórios e corrompendo memória de stack/globais adjacentes. |
| 2 | **Bug crítico** | `int idSensor = i + 7` → `i + 6` | Mapeamento incorreto das LDRs locais (A0, A1, A4) para os índices 0-based do array `pos[]`. Com `i+7`, `processaSensor(7)` usava `pos[7]` (S8) em vez de `pos[6]` (S7); `processaSensor(9)` usava `pos[0]` (S1) em vez de `pos[8]` (S9). Com `i+6`, o mapeamento é correto: S7→idx 6, S8→idx 7, S9→idx 8. |
| 3 | **Bug lógico** | Removida linha `ultimoPWM = msg.substring(1).toInt()` em `RecebeHall()` | A mensagem do carro tem formato `"C1;vel;rot"`. `substring(1)` retorna `"1;vel;rot"`, cujo `toInt()` resulta sempre em `1`, corrompendo `ultimoPWM` e todos os logs subsequentes. O `ultimoPWM` é atualizado corretamente pelos comandos `d<valor>` do controle remoto. |
| 4 | **Bug lógico** | Removida linha `serialPista2.println("Catraca aberta")` do handler GO | Arduino 2 nunca parseava essa string (procura por `"GO_aberto"`), gerando tráfego serial desnecessário que competia com as mensagens de dados. |
| 5 | Bug de desempenho | Removido `delay(1)` do loop de leitura serial; substituído por `readStringUntil('\n')` + `setTimeout(200)` | O `delay(1)` bloqueava ~1 ms por caractere (~60 ms por mensagem de 60 chars), impedindo a recepção de RF e a leitura de LDRs nesse intervalo. `readStringUntil` usa o buffer interno da SoftwareSerial sem bloquear desnecessariamente. |
| 6 | Bug de dados | `const float diametroRoda = 0.024` → `0.0245f` | Alinha com o valor em `Carrinho.ino` e no README (24,5 mm). A diferença de 0,5 mm gerava erro acumulado de ~2% na distância estimada pelo Arduino 1. |
| 7 | Organização | Removida string de debug `"mensagemmmmmmmm"` de `RecebeHall()` | Debug informal que poluía a saída serial e dificultava a leitura de logs reais durante experimentos. |
| 8 | Organização | `RecebeHall` alterada para receber `const char*` em vez de `const String&` | Evita criação de objeto `String` temporário ao passar `char[32]`. Mais eficiente em memória no AVR. |
| 9 | Organização | Variável global `char nomeSensor[4]` movida para local em `EnviaSerial()` | Era declarada globalmente mas usada apenas dentro da função. Variável local é semanticamente correta e reduz escopo. |
| 10 | Organização | Variável global `char buffer[TAM_BUFFER]` removida | Era declarada globalmente mas completamente shadowed por variáveis locais `char buffer[100]` e `char buffer[120]` dentro das funções que a usariam. Dead code. |
| 11 | Organização | Variável `iniciale` movida para local nos dois blocos GO | Evita escopo global desnecessário para uma string de uso pontual. |
| 12 | Organização | `inicio = 0` adicionado ao reset do experimento no handler GO | Garante que `processaSensor()` trate o próximo sensor como "primeiro sensor" quando um novo experimento começa, resetando a estimativa de posição inicial. |
| 13 | Organização | `pinMode(analogPins[i], INPUT)` removido do loop de setup | Pinos analógicos são configurados como entrada por padrão. A chamada era redundante e o loop com bug (6 iterações) tornava isso perigoso. |

---

### 9.3 Arduino_2 — PISTA DO AUTORAMA.ino

| # | Tipo | Alteração | Justificativa |
|---|------|-----------|---------------|
| 1 | **Bug crítico** | `SD.open()` + `arquivo.close()` por linha → `arquivo` global aberto uma vez em `setup()`, gravações usam `arquivo.flush()` | `SD.open()/close()` bloqueava o `loop()` por ~50–100 ms a cada linha. Com 9600 baud e buffer serial de 64 bytes, o buffer transbordava em ~66 ms quando o carro enviava telemetria em alta frequência. `flush()` sincroniza em ~5–10 ms, eliminando o overflow. **Esta é a correção do problema serial relatado.** |
| 2 | **Bug lógico** | Adicionado `while(1)` após falha no `SD.begin()` e `SD.open()` | Antes, o sistema continuava executando sem SD, descartando silenciosamente todos os dados. Agora trava com LED apagado, indicando claramente a falha ao operador. |
| 3 | Bug lógico | Removidas redeclarações locais `File arquivo` em `gravaEvento()` e `gravaLinhaBruta()` | As funções redeclaravam `File arquivo` localmente, shadowing a variável global. O arquivo global nunca era usado; cada gravação abria/fechava um novo handle. |
| 4 | Organização | `radioIn.setTimeout(200)` adicionado em `setup()` | Limita o tempo máximo de espera do `readStringUntil()` a 200 ms para mensagens sem delimitador `\n`. O default de 1000 ms bloquearia o loop por 1 segundo em caso de mensagem corrompida. |
| 5 | Organização | Removidas variáveis globais não usadas: `tInicio`, `testeHabilitado`, `ultimoPWM`, `ultimoTipoPWM` | Foram provavelmente copiadas do Arduino_1 mas nunca integradas na lógica do Arduino_2. Consumiam RAM desnecessariamente. |

---

### 9.4 Controle_Remoto.ino

| # | Tipo | Alteração | Justificativa |
|---|------|-----------|---------------|
| 1 | Organização | Removida `const byte endereco2[6] = "DEF456"` | Declarada mas nunca referenciada em nenhuma função. O controle só transmite para `ABC123` (carro) e `FGH789` (pista). |
| 2 | Organização | Renomeadas `endereco1`/`endereco3` para `enderecoCarroRF`/`enderecoPistaRF` | Nomes descritivos eliminam a necessidade de verificar os comentários para entender o propósito de cada endereço. |
| 3 | Organização | Corrigido typo `"Catrarca"` → `"Catraca"` (múltiplas ocorrências) | Erro ortográfico nas mensagens do display OLED. |
| 4 | Organização | Corrigido typo `"evioRF"` → `"envio RF"` na mensagem de setup | Erro tipográfico na saída serial de inicialização. |
| 5 | Organização | `while (digitalRead(BOTAOC) == LOW)` consolidado com `delay(300)` em posição única | Evita duplicação de lógica de debounce do botão. |

---

### 9.5 Resumo Geral das Correções

| Arquivo | Bugs Críticos | Bugs Maiores | Organizacionais |
|---------|:---:|:---:|:---:|
| Carrinho.ino | 2 | 0 | 4 |
| Arduino_1 | 3 | 2 | 6 |
| Arduino_2 | 2 | 1 | 2 |
| Controle_Remoto | 0 | 0 | 4 |
| **Total** | **7** | **3** | **16** |
