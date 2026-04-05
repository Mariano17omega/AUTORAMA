# Relatório Técnico - Projeto AUTORAMA

Data da análise: 05/04/2026  
Escopo: revisão estática dos arquivos `README.md`, `Arduino_1- PISTA DO AUTORAMA.ino`, `Arduino_2- PISTA DO AUTORAMA.ino`, `Carrinho.ino` e `Controle_Remoto.ino`.

## 1. Objetivo do projeto
O sistema implementa um autorama instrumentado com controle de velocidade em malha aberta/fechada, composto por:
- `Controle_Remoto` (entrada do operador e envio de comandos RF)
- `Pista Arduino 1` (orquestração, leitura parcial de sensores, catraca, agregação de telemetria)
- `Pista Arduino 2` (leitura complementar de sensores e persistência em SD)
- `Carrinho` (atuação do motor, medição de velocidade por Hall, distância por VL53L0X e telemetria RF)

## 2. Arquitetura e fluxo técnico
1. O controle remoto envia comandos RF (`dNNN`, `GO`) para pista/carro.
2. O carrinho aplica PWM no motor, mede rotação (Hall) e envia telemetria para a pista.
3. A pista detecta passagem por sensores LDR distribuídos em 2 Arduinos.
4. O Arduino 1 compõe registros (`L;...` e `H;...`) e repassa por serial para o Arduino 2.
5. O Arduino 2 grava eventos e linhas de telemetria no cartão SD (`dados.txt`).

## 3. Pontos fortes observados
- Divisão funcional coerente por módulos (comando, aquisição, atuação, logging).
- Estratégia de filtro temporal para pulsos Hall no carrinho (reduz ruído em alta frequência).
- Protocolo textual entre módulos facilita depuração em serial.

## 4. Erros e problemas no código (priorizados)

## 4.1 Crítico
1. **Estouro de array no Arduino da pista (comportamento indefinido / corrupção de memória).**  
Arquivo: `Arduino_1- PISTA DO AUTORAMA.ino`  
Linhas: **30-31** e **128-131**  
Detalhe: `analogPins` e `estadoAntA` foram declarados com tamanho 3, mas o `for` em `setup()` itera até `i < 6`. Isso acessa índices inválidos `[3..5]`.

2. **Processamento de sensores ocorre mesmo fora do teste habilitado.**  
Arquivo: `Arduino_1- PISTA DO AUTORAMA.ino`  
Linhas: **183-195** e **340-419**  
Detalhe: `processaSensor()` é chamado continuamente, mesmo com `testeHabilitado == false`. Isso pode iniciar cálculos de posição/tempo antes da abertura válida da catraca.

## 4.2 Alto
1. **`ultimoPWM` é sobrescrito por parsing incorreto da telemetria do carro.**  
Arquivo: `Arduino_1- PISTA DO AUTORAMA.ino`  
Linha: **270**  
Detalhe: em `RecebeHall()`, `ultimoPWM = msg.substring(1).toInt();` usa pacote no formato `C1;vel;rot`, convertendo indevidamente parte do ID para PWM.

2. **Critério de filtro de rotação usa variável inadequada (`difRof`).**  
Arquivo: `Arduino_1- PISTA DO AUTORAMA.ino`  
Linhas: **295-299**  
Detalhe: `difRof` recebe `RotCarroAtual` absoluto (não delta). O filtro `difRof < limitador` acaba limitando o valor total acumulado, não a variação instantânea.

3. **Uso de `readStringUntil('\n')` em loop principal pode bloquear aquisição.**  
Arquivo: `Arduino_2- PISTA DO AUTORAMA.ino`  
Linha: **53**  
Detalhe: `String` bloqueante com timeout padrão de serial pode atrasar leituras de sensores em tempo real.

## 4.3 Médio
1. **Inconsistência de estado/mensagem da catraca no controle remoto.**  
Arquivo: `Controle_Remoto.ino`  
Linhas: **140-149** e **189-197**  
Detalhe: textos "Aberta/Fechada" e `StatusCatrarca` estão invertidos em fluxos diferentes, dificultando diagnóstico operacional.

2. **Comentário de borda no Hall contradiz a lógica implementada.**  
Arquivo: `Carrinho.ino`  
Linhas: **112-114**  
Detalhe: comentário diz "borda de descida", mas condição detecta transição `LOW -> HIGH`.

3. **LED de atividade RF permanece sempre em HIGH após transmissão.**  
Arquivo: `Carrinho.ino`  
Linhas: **230** e **238**  
Detalhe: `LED_WAIT` nunca é desligado na função `enviarTelemetria()`.

4. **Ausência de `Serial.begin()` no controle remoto antes de `Serial.println`.**  
Arquivo: `Controle_Remoto.ino`  
Linha: **127**  
Detalhe: não quebra compilação, mas torna log serial inconsistente/ineficaz.

5. **Variáveis declaradas e não utilizadas (sinal de dívida técnica).**  
Arquivos: múltiplos (`Arduino_1`, `Arduino_2`, `Controle_Remoto`, `Carrinho`)  
Exemplos: `msgRadio`, `idx`, `tInicio` (Arduino 2), `stringPWM`, `rejeitados`.

## 5. Riscos sistêmicos
- **Confiabilidade de dados:** o estouro de array no Arduino 1 pode invalidar toda telemetria.
- **Risco de sincronismo:** parsing bloqueante e uso intensivo de `String` podem gerar jitter/perda de eventos em cenários de alta taxa.
- **Rastreabilidade de experimento:** estados inconsistentes de catraca e PWM comprometem reprodutibilidade de ensaios.

## 6. Recomendações técnicas objetivas
1. Corrigir imediatamente o laço de inicialização dos sensores no Arduino 1 (`i < 3`) ou redimensionar arrays conforme necessidade real.
2. Proteger `processaSensor()` com `if (!testeHabilitado) return;` (ou gate equivalente no chamador).
3. Em `RecebeHall()`, separar parsing de telemetria e remover escrita de `ultimoPWM` nesse ponto.
4. Trocar parsing bloqueante por parser incremental baseado em buffer de `char[]` + delimitador.
5. Padronizar máquina de estados da catraca (`ABERTA/FECHADA`) com enum e mensagens consistentes.
6. Reduzir fragmentação de heap: substituir `String` por buffers fixos em rotinas críticas.

## 7. Conclusão
O projeto possui uma arquitetura funcional e bem segmentada para o objetivo de controle e aquisição no autorama. Entretanto, há erros de implementação importantes, com destaque para um bug crítico de acesso fora dos limites de array no Arduino da pista, além de inconsistências de parsing/estado que podem degradar medições e controle. A correção dos itens críticos e altos deve ser priorizada antes de novos testes experimentais.
