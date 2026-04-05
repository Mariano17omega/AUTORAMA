# Relatório Técnico — Projeto AUTORAMA

> **Data:** 05 de Abril de 2026  
> **Autor:** Análise automática  
> **Versão do código:** commit atual no repositório `Mariano17omega/AUTORAMA`

---

## 1. Visão Geral do Projeto

O projeto AUTORAMA implementa um sistema de controle de velocidade em malha fechada para um veículo em escala sobre uma pista instrumentada. A arquitetura é distribuída em **4 módulos embarcados** comunicando-se via rádio nRF24L01 e serial:

- **Controle Remoto** (Arduino Uno) → envia comandos via RF para a Pista
- **Arduino 1 — Pista Mestre** (Arduino Uno) → coordena o sistema, lê sensores S7-S9, recebe telemetria do carro
- **Arduino 2 — Pista Escravo** (Arduino Uno) → lê sensores S1-S6, grava dados no cartão SD
- **Carro** (Arduino Nano) → sensor Hall, motor DC, VL53L0X, telemetria RF

### Arquivos do Projeto

| Arquivo | Linhas | Módulo | Função |
|---|---|---|---|
| Carrinho.ino | 351 | Carro | Leitura do sensor Hall, controle do motor, telemetria RF, anti-colisão VL53L0X |
| Arduino_1- PISTA DO AUTORAMA.ino | 473 | Pista (Mestre) | Coordenação central, cálculo de velocidade por sensores, repasse de dados ao Arduino 2 |
| Arduino_2- PISTA DO AUTORAMA.ino | 157 | Pista (Escravo) | Leitura de sensores S1–S6, gravação de dados no cartão SD |
| Controle_Remoto.ino | 240 | Controle Remoto | Interface com usuário (teclado + OLED), envio de comandos RF |

---

## 2. Análise por Módulo

### 2.1 Carrinho (`Carrinho.ino`)

**Funcionalidades implementadas:**
- Recepção de comandos PWM via RF e aplicação no motor DC (ponte H L293D)
- Leitura de velocidade via sensor Hall com ISR (Pin Change Interrupt no pino D5)
- Filtro EMA (Exponential Moving Average) na velocidade — α = 0.15
- Anti-colisão com sensor laser VL53L0X (bloqueia motor se distância ≤ 10 cm)
- Envio contínuo de telemetria (ID, velocidade, rotações) à pista

**Pontos positivos:**
- Uso correto de `noInterrupts()`/`interrupts()` para copiar variáveis voláteis
- Filtro anti-bounce no sensor Hall com threshold temporal e relativo (`FATOR_MIN_DT`)
- Histerese no sensor de distância (bloqueia a 10 cm, desbloqueia a 15 cm)

### 2.2 Arduino 1 — Pista Mestre (`Arduino_1- PISTA DO AUTORAMA.ino`)

**Funcionalidades implementadas:**
- Recepção de comandos do Controle Remoto via RF
- Recepção de telemetria do Carro (velocidade e rotações)
- Detecção de passagem nos sensores LDR locais (S7, S8, S9)
- Cálculo de velocidade entre sensores (Δs/Δt)
- Estimativa de posição intermediária usando rotações do sensor Hall
- Acionamento de servo motor (catraca) para liberação do carro
- Formatação e repasse de dados para Arduino 2 via SoftwareSerial

**Pontos positivos:**
- Lógica de posição linear acumulada para lidar com pista circular
- Limitação da posição estimada pelo próximo sensor (evita extrapolação)

### 2.3 Arduino 2 — Pista Escravo (`Arduino_2- PISTA DO AUTORAMA.ino`)

**Funcionalidades implementadas:**
- Recepção de dados do Arduino 1 via SoftwareSerial
- Detecção de passagem nos sensores LDR locais (S1–S6)
- Gravação de eventos e dados em arquivo `dados.txt` no cartão SD
- Envio de notificação de sensor ativado de volta ao Arduino 1

### 2.4 Controle Remoto (`Controle_Remoto.ino`)

**Funcionalidades implementadas:**
- Interface com teclado matricial 4x4
- Display OLED SSD1306 (0.91")
- Envio de comandos: GO (catraca), PWM degrau
- Máquina de estados: entrada de PWM → aguarda GO → execução → regime permanente
- Botões dedicados: STOP (finaliza teste) e GO (catraca)

---

## 3. Erros e Problemas Encontrados

### 3.1 Erros Críticos (podem causar crash ou comportamento indefinido)

> **CUIDADO:** Estes erros podem causar travamentos, corrupção de memória ou falhas silenciosas em tempo de execução.

#### BUG-01: Overflow de Array — `estadoAntA` no Arduino 1
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino, linhas 128-131  
**Severidade:** 🔴 Crítico

O array `analogPins` tem **3 elementos** (linha 30), e `estadoAntA` tem **3 elementos** (linha 31), mas o loop do `setup()` itera até `i < 6`:

```cpp
int analogPins[3] = {A0, A1, A4};    // 3 elementos
int estadoAntA[3];                     // 3 elementos

// No setup():
for (int i = 0; i < 6; i++) {         // ❌ Itera 6 vezes!
    pinMode(analogPins[i], INPUT);     // Acesso fora dos limites para i >= 3
    estadoAntA[i] = analogRead(analogPins[i]); // Corrupção de memória
}
```

**Correção:** Alterar o loop para `i < 3`.

---

#### BUG-02: Lógica de toggle da catraca inconsistente — Arduino 1
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino, linhas 37, 222-246  
**Severidade:** 🔴 Crítico

O ângulo inicial da catraca é definido como `83` (linha 37), mas a verificação compara com `100`:

```cpp
int anguloAtual = 83;   // Valor inicial

// No handler do "GO":
if (anguloAtual == 100) {   // ❌ NUNCA será verdadeiro na primeira ativação!
    anguloAtual = 220;       // Catraca abre
} else {
    anguloAtual = 100;       // Catraca fecha
}
```

Na primeira execução, `anguloAtual` vale `83`, que não é igual a `100`, então entra no `else` e **fecha** a catraca em vez de abrir. A partir da segunda execução funciona como toggle entre 100 e 220, mas o primeiro acionamento é invertido.

**Correção:** Inicializar `anguloAtual = 100` ou usar um `bool catracaAberta = false` para controlar o estado.

---

#### BUG-03: `RecebeHall` sobrescreve `ultimoPWM` com lixo
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino, linhas 269-270  
**Severidade:** 🔴 Crítico

Na função `RecebeHall`, antes de extrair os campos corretamente, o código faz:

```cpp
void RecebeHall(const String& mensagem) {
    String msg = String(mensagem);
    ultimoPWM = msg.substring(1).toInt();  // ❌ Sobrescreve ultimoPWM com valor errado!
    msg.trim();
    // ... depois faz o parsing correto com indexOf(';')
```

A mensagem tem formato `C1;0.1234;56.78`. O `msg.substring(1)` retorna `1;0.1234;56.78`, e `toInt()` retorna apenas `1` (o número do carro). Isso sobrescreve a variável `ultimoPWM` que deveria armazenar o valor do PWM comandado.

**Correção:** Remover a linha `ultimoPWM = msg.substring(1).toInt();`.

---

#### BUG-04: Divisão por zero potencial no primeiro sensor
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino, linhas 379-381  
**Severidade:** 🟠 Alto

Quando `inicio == 0` (primeiro sensor ativado), o código calcula:

```cpp
deltaT = tAgora / 1e6;
deltaS = 0.03;
ultimaVelocidadeCalculada = deltaS / deltaT;  // ❌ Se tAgora == 0 → divisão por zero!
```

Se o primeiro sensor for ativado imediatamente após `tInicio = micros()`, `tAgora` será 0 ou muito próximo de 0, causando divisão por zero ou valor astronomicamente alto.

**Correção:** Adicionar verificação `if (deltaT > 0.0001)` antes da divisão.

---

### 3.2 Erros de Lógica e Inconsistências

> **ATENÇÃO:** Estes erros não causam crash mas produzem dados incorretos ou comportamento inesperado.

#### BUG-05: Indentação enganosa esconde estrutura real — Arduino 2
**Arquivo:** Arduino_2- PISTA DO AUTORAMA.ino, linhas 57-100  
**Severidade:** 🟠 Alto

A indentação do `loop()` no Arduino 2 é extremamente confusa. Os blocos `else if` estão indentados de forma inconsistente, tornando a manutenção do código muito difícil. O `}}` na linha 100 fecha dois blocos simultaneamente (o `else if` e o `if (radioIn.available())`), o que funciona sintaticamente mas é muito propenso a erros de manutenção.

**Correção:** Reformatar com indentação consistente.

---

#### BUG-06: O Controle Remoto não envia PWM=0 ao parar o teste
**Arquivo:** Controle_Remoto.ino, linhas 161-174  
**Severidade:** 🟠 Alto

Quando o botão STOP é pressionado, o controle remoto marca `mantendoPWM = false` mas **nunca envia um comando para parar o motor do carro** (PWM=0). O carro continua com o último PWM recebido indefinidamente.

```cpp
if (digitalRead(BOTAOstop) == LOW) {
    mantendoPWM   = false;   // Apenas para de enviar
    tipoOperacao  = 0;
    // ❌ Falta: enviarParaCarro("d0"); para parar o motor!
    oledShow2Lines("Teste Finalizado", ":)");
```

**Correção:** Adicionar `enviarParaCarro("d0");` e `enviarParaPista("d0");` antes de desabilitar o PWM.

---

#### BUG-07: Endereços RF — Controle e Carro compartilham o mesmo pipe
**Arquivo:** Controle_Remoto.ino linhas 35-37 vs Arduino_1 linhas 12-13  
**Severidade:** 🟠 Alto

O Controle Remoto e o Carro enviam mensagens para o mesmo endereço RF (`"FGH789"`), usando o mesmo pipe na Pista. Embora funcione por causa da distinção por prefixo de mensagem (`C` para carro, `d`/`GO` para controle), isso causa colisões de pacotes quando ambos transmitem simultaneamente.

**Recomendação:** Usar pipes separados ou implementar um protocolo de arbitragem.

---

#### BUG-08: Mapeamento de sensores S7-S9 inconsistente
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino, linhas 178-190  
**Severidade:** 🟡 Médio

O Arduino 2 envia sensores como `S1` a `S6`. O Arduino 1 extrai `n` e chama `processaSensor(n - 1)`, passando `0` a `5`.

Para os sensores locais, chama `processaSensor(i + 7)`, passando `7`, `8`, `9`. Dentro de `processaSensor`, faz `Agr = idxAtual % 9`:
- `idxAtual = 7` → `Agr = 7` → `pos[7]` ✅
- `idxAtual = 8` → `Agr = 8` → `pos[8]` ✅
- `idxAtual = 9` → `Agr = 0` → `pos[0]` — mapeia para S1 em vez de representar S9

O nome do sensor na saída fica `S10` (`sensorPassado + 1 = 9 + 1`), mas S10 não existe na pista.

**Correção:** Mudar para `int idSensor = i + 6;` para que o mapeamento seja S7=6, S8=7, S9=8.

---

#### BUG-09: Variável global `arquivo` declarada mas não usada — Arduino 2
**Arquivo:** Arduino_2- PISTA DO AUTORAMA.ino, linha 23  
**Severidade:** 🟡 Médio

A variável global `File arquivo;` é declarada mas nunca usada. As funções `gravaEvento` e `gravaLinhaBruta` declaram variáveis locais com o mesmo nome (shadowing), desperdiçando memória RAM preciosa no Arduino.

---

#### BUG-10: Dimensões do display OLED possivelmente incorretas
**Arquivo:** Controle_Remoto.ino, linhas 11-12  
**Severidade:** 🟡 Médio

O README indica um "Display OLED 0.91"", que tipicamente é 128x32 pixels. Porém o código define:

```cpp
#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 16
```

Se o display for realmente 128x32, parte da tela não será utilizada e o texto pode ficar cortado.

**Verificar:** Conferir o modelo exato do display e ajustar as dimensões.

---

### 3.3 Problemas de Qualidade e Boas Práticas

#### BUG-11: Uso excessivo de `String` no Arduino — risco de fragmentação de heap
**Arquivos:** Todos os 4 arquivos  
**Severidade:** 🟡 Médio

O uso intensivo da classe `String` do Arduino (concatenação com `+`, `substring()`, `readStringUntil()`) causa fragmentação do heap na memória limitada do ATmega328P (2 KB de RAM). Em execuções prolongadas, pode levar a travamentos aleatórios.

**Recomendação:** Substituir por buffers `char[]` com `strtok()`, `strncpy()` e `snprintf()`.

---

#### BUG-12: `Serial.begin()` ausente no Controle Remoto
**Arquivo:** Controle_Remoto.ino, linhas 112-129  
**Severidade:** 🟢 Baixo

O `setup()` do Controle Remoto chama `Serial.println("Arduino controle pronto...")` na linha 127, mas **nunca inicializa** `Serial.begin()`. A mensagem não será enviada.

---

#### BUG-13: Debug prints com texto informal deixados no código
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino, linha 268  
**Severidade:** 🟢 Baixo

```cpp
Serial.print("mensagemmmmmmmm"); Serial.print(mensagem);
```

Mensagens de debug com texto informal permanecem no código. Devem ser removidas ou encapsuladas com `#ifdef DEBUG`.

---

#### BUG-14: `LED_WAIT` nunca é desligado no Carrinho
**Arquivo:** Carrinho.ino, linhas 198, 238  
**Severidade:** 🟢 Baixo

Em `checkRF()` e `enviarTelemetria()`, `LED_WAIT` é ligado (`HIGH`) mas nunca desligado. O LED ficará permanentemente aceso após a primeira comunicação RF.

---

#### BUG-15: Variáveis não utilizadas — Arduino 1
**Arquivo:** Arduino_1- PISTA DO AUTORAMA.ino  
**Severidade:** 🟢 Baixo

Variáveis declaradas mas nunca usadas:
- `msgRadio` (linha 28) — String vazia, nunca referenciada
- `ledBlueState` (linha 38) — nunca lida/escrita
- `ultimoSensorDetectado` (linha 50) — atribuído `-1` mas nunca lido
- `ultimaVelocidadeCalculadaAnt` (linha 70) — nunca usada
- `ultimoSensorAcelPos` (linha 74) — nunca lido
- `qtd` (linha 76) — nunca usada
- `buffer[TAM_BUFFER]` (linha 115) e `idx` (linha 119) — shadow do buffer local

---

## 4. Tabela Resumo de Erros

| ID | Arquivo | Linha(s) | Severidade | Descrição |
|---|---|---|---|---|
| BUG-01 | Arduino_1 | 128-131 | 🔴 Crítico | Loop itera 6x em array de tamanho 3 (overflow) |
| BUG-02 | Arduino_1 | 37, 222 | 🔴 Crítico | Catraca com lógica invertida no primeiro acionamento |
| BUG-03 | Arduino_1 | 269-270 | 🔴 Crítico | `ultimoPWM` sobrescrito com ID do carro |
| BUG-04 | Arduino_1 | 379-381 | 🟠 Alto | Divisão por zero no primeiro sensor |
| BUG-05 | Arduino_2 | 57-100 | 🟠 Alto | Indentação enganosa esconde estrutura real |
| BUG-06 | Controle | 161-174 | 🟠 Alto | STOP não envia PWM=0 ao carro |
| BUG-07 | Controle + Arduino_1 | - | 🟠 Alto | Controle e Carro compartilham pipe RF |
| BUG-08 | Arduino_1 | 178-190 | 🟡 Médio | Mapeamento de sensor S7-S9 inconsistente |
| BUG-09 | Arduino_2 | 23 | 🟡 Médio | Variável global `arquivo` com shadowing |
| BUG-10 | Controle | 11-12 | 🟡 Médio | Dimensões OLED possivelmente incorretas |
| BUG-11 | Todos | - | 🟡 Médio | Uso excessivo de `String` → fragmentação de heap |
| BUG-12 | Controle | 127 | 🟢 Baixo | `Serial.println` sem `Serial.begin()` |
| BUG-13 | Arduino_1 | 268 | 🟢 Baixo | Debug prints informais no código |
| BUG-14 | Carrinho | 198, 238 | 🟢 Baixo | `LED_WAIT` nunca é desligado |
| BUG-15 | Arduino_1 | Várias | 🟢 Baixo | Variáveis declaradas e não utilizadas |

---

## 5. Recomendações Gerais

1. **Corrigir BUG-01 imediatamente** — o overflow de array é a fonte mais provável de comportamentos aleatórios.
2. **Unificar o valor inicial da catraca** (BUG-02) para evitar a inversão no primeiro acionamento.
3. **Remover a linha 270 do Arduino 1** que sobrescreve `ultimoPWM` erroneamente (BUG-03).
4. **Enviar `d0` ao parar o teste** no controle remoto (BUG-06) — caso contrário o carro continua andando.
5. **Substituir `String` por `char[]`** progressivamente, começando pelos loops mais frequentes (recepção serial e RF).
6. **Reformatar a indentação** do Arduino 2 (BUG-05) para refletir a estrutura real das condições.
7. **Adicionar `#define DEBUG`** e encapsular todos os `Serial.print` de debug para facilitar builds de produção.
8. **Considerar watchdog timer** em todos os Arduinos para recuperação automática em caso de travamento.

---

> **IMPORTANTE:** Os 3 bugs críticos (BUG-01, BUG-02, BUG-03) devem ser corrigidos antes de qualquer teste na pista, pois causam comportamento indefinido e dados incorretos.
