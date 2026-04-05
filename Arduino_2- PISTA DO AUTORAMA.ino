// CODIGO ARDUINO 2 DA PISTA

#include <SoftwareSerial.h>
#include <SD.h>

SoftwareSerial radioIn(8, 9); // RX=8 (recebe do Arduino 1), TX=9 (envia ao Arduino 1)
const int chipSelect = 4;

// ============================================================
// SENSORES LDR LOCAIS (S1–S6)
// ============================================================
int analogPins[6]  = {A0, A1, A2, A3, A4, A5};
int estadoAntA[6];

// ============================================================
// SD CARD
// CORREÇÃO: arquivo aberto uma vez no setup() e mantido aberto.
// Antes: SD.open()/close() a cada linha bloqueava ~50-100 ms,
// causando overflow do buffer da SoftwareSerial (64 bytes) em
// alta frequência de telemetria. flush() sincroniza em ~5-10 ms.
// ============================================================
File arquivo;

// ============================================================
// LED DE STATUS DO SD
// ============================================================
const int LEDBLUE_PIN = 5;

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  radioIn.begin(9600);
  radioIn.setTimeout(200); // evita bloqueio longo em mensagens malformadas

  for (int i = 0; i < 6; i++)
    estadoAntA[i] = analogRead(analogPins[i]);

  pinMode(LEDBLUE_PIN, OUTPUT);
  digitalWrite(LEDBLUE_PIN, LOW);

  // CORREÇÃO: falha no SD trava o sistema — antes a execução continuava
  // silenciosamente e todos os dados do experimento eram perdidos
  if (!SD.begin(chipSelect)) {
    Serial.println("ERRO: Falha ao inicializar SD. Verifique o cartao.");
    while (1); // trava com LED apagado como indicação de falha
  }

  arquivo = SD.open("dados.txt", FILE_WRITE);
  if (!arquivo) {
    Serial.println("ERRO: Falha ao abrir dados.txt no SD.");
    while (1);
  }

  Serial.println("SD OK - dados.txt aberto.");
  digitalWrite(LEDBLUE_PIN, HIGH);
}

// ============================================================
// LOOP
// ============================================================
void loop() {

  // ----- RECEBE DADOS DO ARDUINO 1 -----
  if (radioIn.available()) {
    String msg = radioIn.readStringUntil('\n');
    msg.trim();

    if (msg.endsWith("GO_aberto")) {
      int p1 = msg.indexOf(';');
      if (p1 != -1) {
        gravaEvento("s - Catraca Aberta", msg.substring(0, p1));
      }

    } else if (msg.endsWith("GO_fechado")) {
      int p1 = msg.indexOf(';');
      if (p1 != -1) {
        gravaEvento("s - Catraca Fechada", msg.substring(0, p1));
      }

    } else if (msg.startsWith("L;") || msg.startsWith("H;")) {
      int p1 = msg.indexOf(';');
      int p2 = msg.indexOf(';', p1 + 1);
      int p3 = msg.indexOf(';', p2 + 1);
      int p4 = msg.indexOf(';', p3 + 1);
      int p5 = msg.indexOf(';', p4 + 1);
      int p6 = msg.indexOf(';', p5 + 1);
      int p7 = msg.indexOf(';', p6 + 1);
      int p8 = msg.indexOf(';', p7 + 1);

      if (p1 > 0 && p2 > p1 && p3 > p2 && p4 > p3 &&
          p5 > p4 && p6 > p5 && p7 > p6 && p8 > p7) {

        gravaLinhaBruta(
          msg.substring(p1 + 1, p2),  // tempo (µs)
          msg.substring(p2 + 1, p3),  // sensor (ex: "S1")
          msg.substring(p3 + 1, p4),  // tipo PWM (ex: "d")
          msg.substring(p4 + 1, p5),  // valor PWM
          msg.substring(p5 + 1, p6),  // velocidade
          msg.substring(p6 + 1, p7),  // distância
          msg.substring(p7 + 1, p8),  // fonte ("S" ou "C1")
          msg.substring(p8 + 1)        // rotações
        );
      }
    }
  }

  // ----- LÊ SENSORES LDR LOCAIS (S1–S6) E NOTIFICA ARDUINO 1 -----
  for (int i = 0; i < 6; i++) {
    int leitura = analogRead(analogPins[i]);

    if (leitura < 700 && estadoAntA[i] >= 700) {
      char buffer[8];
      sprintf(buffer, "S%d", i + 1);
      radioIn.println(buffer); // envia ao Arduino 1 via SoftwareSerial TX
      Serial.println(buffer);
    }

    estadoAntA[i] = leitura;
  }
}

// ============================================================
// GRAVA EVENTO (abertura/fechamento de catraca)
// ============================================================
void gravaEvento(String texto, String tempoString) {
  unsigned long tMicros = strtoul(tempoString.c_str(), NULL, 10);
  float tSeg = tMicros / 1000000.0f;

  String linha = String(tSeg, 3) + texto;
  Serial.println("SD-> " + linha);

  // CORREÇÃO: usa arquivo global já aberto; flush() em vez de close()
  arquivo.println(linha);
  arquivo.flush();
}

// ============================================================
// GRAVA LINHA DE DADOS (sensor ou Hall contínuo)
// ============================================================
void gravaLinhaBruta(String t, String nomeS, String tPWM, String vPWM,
                     String vel, String dist, String tipo, String rot) {

  unsigned long tMicros = strtoul(t.c_str(), NULL, 10);
  float tSeg = tMicros / 1000000.0f;

  String linha = String(tSeg, 2) + ", "
               + nomeS + ", "
               + tPWM  + ", "
               + vPWM  + ", "
               + vel   + ", "
               + dist  + ", "
               + tipo  + ", "
               + rot;

  Serial.println("SD-> " + linha);

  // CORREÇÃO: usa arquivo global já aberto; flush() em vez de close()
  arquivo.println(linha);
  arquivo.flush();
}
