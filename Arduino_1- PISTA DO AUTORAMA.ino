// CODIGO ARDUINO 1 DA PISTA

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ============================================================
// RADIO RF24
// ============================================================
RF24 radio(9, 10); // CE, CSN

const byte enderecoPista[6] = "FGH789"; // escuta (controle remoto + carro)
const byte enderecoCarro[6] = "ABC123"; // pipe 2 (reservado)

// ============================================================
// SERVO (CATRACA)
// ============================================================
Servo servoMotor;
const int SERVO_PIN = 4;

// ============================================================
// LEDS
// ============================================================
const int LEDYELLOW = 6;
const int LEDRED    = 7;
const int LEDGREEN  = 8;

// ============================================================
// SERIAL PARA ARDUINO 2
// ============================================================
SoftwareSerial serialPista2(A3, A2); // RX=A3, TX=A2

// ============================================================
// SENSORES ANALÓGICOS DA PISTA 1 → S7, S8, S9
// ============================================================
int analogPins[3]  = {A0, A1, A4};
int estadoAntA[3];

// ============================================================
// VARIÁVEIS DE CONTROLE
// ============================================================
char mensagem[32];
int  anguloAtual = 83; // posição inicial da catraca

char  carroID[5]    = "";
float velCarro      = 0.0f;
float RotDelta      = 0.0f;
float RotCarroAtual = 0.0f;
float RotCarroAnt   = 0.0f;
float RotInc        = 0.0f;
long  RotUltimoSensor = 0;

const float limitador    = 10.0f; // descarta saltos de rotação implausíveis
const float limitadorvel = 5.0f;  // descarta velocidades implausíveis (m/s)

// ============================================================
// CONTROLE DE SENSORES
// ============================================================
int   ultimoSensorGravado    = -1;
int   ultimoSensorDetectado  = -1;
float ultimaPosicaoRegistrada = 0.0f;
float UltimoTrecho            = 0.0f;
float PosProxSensor           = 0.0f;
float PosProxSensorLinear     = 0.0f;
float DistProxSensor          = 0.0f;
float deltaS  = 0.0f;
float deltaT  = 0.0f;
float difRof  = 0.0f;
int   prox    = 0;
int   Agr     = 0;
int   inicio  = 0;

float poslinearsensor             = 0.0f;
float distanciaTotal              = 0.0f;
float ultimaVelocidadeCalculada   = 0.0f;
float TrechoAgr = 0.0f;
float TrechoAnt = 0.0f;

unsigned long tUltimoPontoSensor = 0;
unsigned long tAgora             = 0;

// ============================================================
// MAPA DA PISTA (posições e distâncias em metros)
// ============================================================

// pos[i] = posição linear acumulada do sensor Si+1 na pista
float pos[9] = {
  4.285f,  // S1 (linha de chegada / lap)
  0.355f,  // S2
  0.635f,  // S3
  0.910f,  // S4
  1.190f,  // S5
  1.470f,  // S6
  2.220f,  // S7
  2.920f,  // S8
  3.510f   // S9
};

// distTrecho[i] = distância do sensor Si+1 ao próximo sensor
float distTrecho[9] = {
  0.355f,  // S1→S2
  0.280f,  // S2→S3
  0.275f,  // S3→S4
  0.280f,  // S4→S5
  0.280f,  // S5→S6
  0.750f,  // S6→S7
  0.700f,  // S7→S8
  0.590f,  // S8→S9
  0.775f   // S9→S1
};

// ============================================================
// CONTROLE DE TEMPO E PWM
// ============================================================
unsigned long tInicio = 0;
bool testeHabilitado  = false;
int  ultimoPWM        = 0;
char ultimoTipoPWM    = '-';

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  serialPista2.begin(9600);
  serialPista2.setTimeout(200); // evita bloqueio longo em mensagens malformadas

  // CORREÇÃO: loop só até 3 — analogPins[] e estadoAntA[] têm 3 elementos
  for (int i = 0; i < 3; i++) {
    estadoAntA[i] = analogRead(analogPins[i]);
  }

  pinMode(LEDRED,    OUTPUT);
  pinMode(LEDGREEN,  OUTPUT);
  pinMode(LEDYELLOW, OUTPUT);

  digitalWrite(LEDGREEN,  HIGH);
  digitalWrite(LEDRED,    LOW);
  digitalWrite(LEDYELLOW, LOW);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.openReadingPipe(1, enderecoPista);
  radio.openReadingPipe(2, enderecoCarro);
  radio.startListening();

  Serial.println("Arduino Pista 1 pronto. Aguardando RF...");
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {

  // ----- RECEBE NOTIFICAÇÃO DE SENSOR DO ARDUINO 2 -----
  if (serialPista2.available() > 0) {
    // CORREÇÃO: readStringUntil substitui o loop com delay(1) por caractere
    String entrada = serialPista2.readStringUntil('\n');
    entrada.trim();

    if (entrada.length() > 0 && entrada.startsWith("S")) {
      int n = entrada.substring(1).toInt();
      if (n >= 1 && n <= 6) processaSensor(n - 1);
    }
  }

  // ----- LÊ SENSORES LDR LOCAIS (S7, S8, S9) -----
  for (int i = 0; i < 3; i++) {
    int leitura = analogRead(analogPins[i]);

    if (leitura <= 700 && estadoAntA[i] > 700) {
      // CORREÇÃO: i+6 mapeia corretamente 0,1,2 → índices 6,7,8 (S7, S8, S9)
      processaSensor(i + 6);
    }

    estadoAntA[i] = leitura;
  }

  // ----- RECEBE RF (CONTROLE REMOTO E CARRO) -----
  if (radio.available()) {
    digitalWrite(LEDYELLOW, HIGH);
    memset(mensagem, 0, sizeof(mensagem));
    radio.read(mensagem, sizeof(mensagem));

    Serial.print("RF recebido: ");
    Serial.println(mensagem);

    if (mensagem[0] == 'C') {
      // Telemetria do carro: "C1;velocidade;rotacoes"
      RecebeHall(mensagem);

    } else if (mensagem[0] == 'd') {
      // Comando de PWM degrau do controle remoto
      ultimoTipoPWM = 'd';
      String msg = String(mensagem);
      msg.trim();
      ultimoPWM = msg.substring(1).toInt();

    } else if (strcmp(mensagem, "GO") == 0) {
      servoMotor.attach(SERVO_PIN);

      if (anguloAtual == 100) {
        // Abre a catraca e inicia o experimento
        anguloAtual = 220;
        digitalWrite(LEDGREEN, LOW);
        digitalWrite(LEDRED,   HIGH);

        testeHabilitado = true;
        ultimaVelocidadeCalculada = 0.0f;
        distanciaTotal            = 0.0f;
        ultimoSensorGravado       = -1;
        ultimoSensorDetectado     = -1;
        inicio = 0;
        tInicio = micros();
        tAgora  = 0;

        char iniciale[50];
        snprintf(iniciale, sizeof(iniciale), "%lu;GO_aberto", tAgora);
        serialPista2.println(iniciale);

      } else {
        // Fecha a catraca e encerra o experimento
        anguloAtual = 100;
        digitalWrite(LEDGREEN, HIGH);
        digitalWrite(LEDRED,   LOW);

        tAgora = micros() - tInicio;
        char iniciale[50];
        snprintf(iniciale, sizeof(iniciale), "%lu;GO_fechado", tAgora);
        serialPista2.println(iniciale);
      }

      servoMotor.write(anguloAtual);
      delay(100);
      servoMotor.detach();
    }

    digitalWrite(LEDYELLOW, LOW);
  }
}

// ============================================================
// RECEPÇÃO DE TELEMETRIA DO CARRO
// ============================================================
// CORREÇÃO: parâmetro como const char* (eficiência — evita cópia de String)
// CORREÇÃO: removida linha que lia ultimoPWM incorretamente da mensagem do carro
void RecebeHall(const char* dados) {
  String msg = String(dados);
  msg.trim();

  int p1 = msg.indexOf(';');
  int p2 = msg.indexOf(';', p1 + 1);

  if (p1 > 0 && p2 > p1) {
    String sID  = msg.substring(0, p1);
    String sVel = msg.substring(p1 + 1, p2);
    String sRPM = msg.substring(p2 + 1);

    sID.toCharArray(carroID, sizeof(carroID));
    velCarro      = sVel.toFloat();
    RotCarroAtual = sRPM.toFloat() - RotInc - 1;
    difRof        = RotCarroAtual;

    if (RotCarroAtual >= RotCarroAnt && difRof < limitador && velCarro < limitadorvel) {
      RotDelta = RotCarroAtual - RotUltimoSensor;
      gravaPWM_C();
    }

    RotCarroAnt = RotCarroAtual;
  }
}

// ============================================================
// GRAVA DADOS CONTÍNUOS DO HALL (entre sensores)
// ============================================================
void gravaPWM_C() {
  if (!testeHabilitado) return;

  tAgora = micros() - tInicio;

  if (RotDelta <= 0) return; // sem avanço real, não grava

  // CORREÇÃO: diâmetro 0.0245 m (consistente com Carrinho.ino e README)
  const float diametroRoda = 0.0245f;
  const float circRoda     = PI * diametroRoda;

  float deltaDistRPM      = RotDelta * circRoda;
  float distanciaEstimativa = ultimaPosicaoRegistrada + deltaDistRPM;

  // Limita estimativa ao alcance do próximo sensor físico
  if (distanciaEstimativa > PosProxSensorLinear)
    distanciaEstimativa = PosProxSensorLinear;

  distanciaTotal            = distanciaEstimativa;
  ultimaVelocidadeCalculada = velCarro;

  if (RotCarroAtual != RotCarroAnt) {
    EnviaSerialHall();
  }
  RotCarroAnt = RotCarroAtual;
}

// ============================================================
// PROCESSA DETECÇÃO DE SENSOR
// ============================================================
void processaSensor(int idxAtual) {
  if (idxAtual == ultimoSensorGravado) return; // evita duplo registro

  tAgora = micros() - tInicio;
  Agr    = idxAtual % 9;
  TrechoAgr = pos[Agr];

  float L = pos[0]; // comprimento total da pista

  if (TrechoAgr >= TrechoAnt) {
    UltimoTrecho = TrechoAgr - TrechoAnt;
  } else {
    // Cruzamento da linha de chegada (S9 → S1)
    UltimoTrecho = (L - TrechoAnt) + TrechoAgr;
  }
  TrechoAnt = TrechoAgr;

  prox = (idxAtual + 1) % 9;
  PosProxSensor    = pos[prox];
  DistProxSensor   = distTrecho[prox];

  if (inicio == 0) {
    // Primeiro sensor do experimento
    ultimaPosicaoRegistrada  = 0.0f;
    distanciaTotal           = 0.0f;
    deltaT = tAgora / 1e6f;
    deltaS = 0.03f; // 3 cm: distância física estimada do ponto inicial ao sensor
    ultimaVelocidadeCalculada = deltaS / deltaT;
    TrechoAnt = 0.0f;
    RotInc    = RotCarroAtual + 1;

    EnviaSerial(Agr);

    tUltimoPontoSensor  = tAgora;
    UltimoTrecho        = 0.0f;
    poslinearsensor     = 0.0f;
    PosProxSensorLinear = ultimaPosicaoRegistrada + pos[1];
    inicio = 1;

  } else {
    poslinearsensor += UltimoTrecho;
    deltaT = (tAgora - tUltimoPontoSensor) / 1e6f;
    deltaS = poslinearsensor - ultimaPosicaoRegistrada;
    ultimaVelocidadeCalculada = deltaS / deltaT;
    distanciaTotal = poslinearsensor;

    EnviaSerial(Agr);

    tUltimoPontoSensor  = tAgora;
    PosProxSensorLinear = distanciaTotal + DistProxSensor;
    ultimaPosicaoRegistrada = distanciaTotal;
  }

  ultimoSensorGravado = idxAtual;
  RotUltimoSensor     = RotCarroAtual;
  RotDelta            = 0;
}

// ============================================================
// ENVIA DADOS DE SENSOR PARA ARDUINO 2
// ============================================================
void EnviaSerial(int sensorPassado) {
  char sVel[10], sDist[10], sRot[10];
  char nomeSensor[4]; // CORREÇÃO: variável local (antes era global desnecessária)

  dtostrf(ultimaVelocidadeCalculada, 1, 3, sVel);
  dtostrf(distanciaTotal,            1, 4, sDist);
  dtostrf(RotCarroAtual,             1, 2, sRot);
  sprintf(nomeSensor, "S%d", sensorPassado + 1);

  char buffer[100];
  snprintf(buffer, sizeof(buffer), "L;%lu;%s;%c;%d;%s;%s;S;%s",
           tAgora, nomeSensor, ultimoTipoPWM, ultimoPWM, sVel, sDist, sRot);

  Serial.print("buffer: "); Serial.println(buffer);
  serialPista2.println(buffer);
}

// ============================================================
// ENVIA DADOS CONTÍNUOS DO HALL PARA ARDUINO 2
// ============================================================
void EnviaSerialHall() {
  char sVel[10], sDist[10], sRot[10];

  dtostrf(ultimaVelocidadeCalculada, 1, 3, sVel);
  dtostrf(distanciaTotal,            1, 4, sDist);
  dtostrf(RotCarroAtual,             1, 2, sRot);

  char buffer[120];
  snprintf(buffer, sizeof(buffer), "H;%lu;S%d;%c;%d;%s;%s;%s;%s",
           tAgora,
           ultimoSensorGravado + 1,
           ultimoTipoPWM,
           ultimoPWM,
           sVel, sDist,
           carroID,
           sRot);

  Serial.print("buffer Hall: "); Serial.println(buffer);
  serialPista2.println(buffer);
}
