#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <avr/interrupt.h>

// =====================================================================
// IDENTIFICAÇÃO DO CARRO
// =====================================================================
const char ID_CARRO[] = "C1";

// =====================================================================
// RADIO
// =====================================================================
RF24 radio(9, 10);
const byte enderecoRX[6]       = "ABC123";
const byte enderecoPista[6] = "FGH789";

// =====================================================================
// LEDS
// =====================================================================
const int LED_VERMELHO     = A0;
const int LED_AZUL      = A1;
const int  LED_VERDE  = A3;
const int LED_WAIT      = A2;

// =====================================================================
// MOTOR
// =====================================================================
const int IN1 = 2;
const int IN2 = 4;
const int EN  = 3;

// =====================================================================
// SENSOR VL53L0X
// =====================================================================
VL53L0X sensor;
unsigned long ultimoUpdateDist = 0;
const unsigned long intervaloDist = 90;

// =====================================================================
// ESTADOS
// =====================================================================
bool bloqueado = false;
bool comandoAtivo = false;
int  ultimoPWM = 0;



// =====================================================================
// BUFFER RF
// =====================================================================
char ValorPWM[15];
char strVel[12];
char strRot[12];

// =====================================================================
// SENSOR HALL / VELOCIDADE
// =====================================================================
const byte PIN_HALL = 5;
const byte IMAS_POR_VOLTA = 1;



const float DIAMETRO_RODA = 0.0245; // diametro em metros
const float CIRCUNFERENCIA = 3.1415926 * DIAMETRO_RODA;

// Contadores / estado do hall
volatile unsigned long pulsosTotais = 0;
volatile unsigned long pulsosRejeitados = 0; // debug
volatile unsigned long ultimoDtValido_us = 0;


volatile byte estadoAnterior = LOW;

// ===== NOVO: medição por Δt (micros) + filtro EMA =====
volatile unsigned long tPulsoAtual_us = 0;
volatile unsigned long tPulsoAnterior_us = 0;
volatile bool novoPulso = false;
volatile bool armadoParaNovoPulso = true;


// Debounce/blanking por tempo (anti-ruído). Ajuste conforme seu hardware.
const unsigned long TEMPO_MIN_US = 10000; // 10 ms
const float FATOR_MIN_DT = 0.75f;           // 75% do último dt válido


// Filtro de velocidade (1ª ordem / EMA)
float velocidade_inst = 0.0f;     // m/s
float velocidade_filt = 0.0f;     // m/s
const float ALPHA = 0.15f;        // quando maior mais ruidos

// Variáveis calculadas (mantidas)
float rpm = 0.0;
float velocidade = 0.0;
float rotacoesTotais = 0.0;
//float rotacoesTotaisAnt = 0.0;
// Controle de envio (mantido)
//unsigned long ultimaRotacaoEnviada = 0;
//  unsigned long pulsoAtual;
//static unsigned long ultimoPulsoEnviado = 0;

// =====================================================================

// ISR – SENSOR HALL (PCINT2 – D5)
// =====================================================================
ISR(PCINT2_vect) {
  byte estadoAtual = (PIND & (1 << PD5)) ? HIGH : LOW;
  unsigned long agora_us = micros();

  // borda de descida só conta se estiver armado
  if (armadoParaNovoPulso && estadoAnterior == LOW && estadoAtual == HIGH) {

    bool aceita = false;

    if (tPulsoAtual_us == 0) {
      // primeiro pulso
      aceita = true;
    } else {
      unsigned long dtDesdeUltimo = agora_us - tPulsoAtual_us;

      unsigned long limiteMinimo = TEMPO_MIN_US; // ignora pulsos muito próximos

      if (ultimoDtValido_us > 0) {
        unsigned long limiteRelativo = (unsigned long)(FATOR_MIN_DT * ultimoDtValido_us);
        if (limiteRelativo > limiteMinimo) {
          limiteMinimo = limiteRelativo;
        }
      }

      if (dtDesdeUltimo >= limiteMinimo) {
        aceita = true;
      }
    }

    if (aceita) {
      pulsosTotais ++;
      tPulsoAnterior_us = tPulsoAtual_us;
      tPulsoAtual_us = agora_us;

      if (tPulsoAnterior_us > 0) {
        ultimoDtValido_us = tPulsoAtual_us - tPulsoAnterior_us;
      }

      novoPulso = true;
      armadoParaNovoPulso = false; // trava até liberar
    } else {
      pulsosRejeitados++;
    }
  }

  // só rearma quando voltar para HIGH
if (estadoAnterior == HIGH  && estadoAtual == LOW ) {
  armadoParaNovoPulso = true;
}

  estadoAnterior = estadoAtual;
}

 

// =====================================================================
// FUNÇÕES AUXILIARES
// =====================================================================
int medirDistancia() { 
  return sensor.readRangeContinuousMillimeters() /10 ; 
}


void aplicarPWMnoMotor(int pwm) {
  analogWrite(EN, pwm);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

void atualizarLEDs() {

  digitalWrite(LED_AZUL, comandoAtivo ? HIGH : LOW);

  if (bloqueado) {
    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    
  } else {
    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, (ultimoPWM > 0) ? HIGH : LOW);
  }
}

// =====================================================================
// RECEPÇÃO RF
// =====================================================================
void checkRF() {

  if (!radio.available()) return;

  digitalWrite(LED_WAIT, HIGH);

  while (radio.available()) {

    radio.read(&ValorPWM, sizeof(ValorPWM));

    comandoAtivo = true;

    int pwmRecebido = atoi(&ValorPWM[1]);
    pwmRecebido = constrain(pwmRecebido, 0, 255);
    ultimoPWM = pwmRecebido;

    if (!bloqueado)
      aplicarPWMnoMotor(ultimoPWM);
    else
      aplicarPWMnoMotor(0);
  }

  atualizarLEDs();
  
}

// =====================================================================
// ENVIO DE TELEMETRIA (COMPARAÇÃO SEGURA, SEM == EM FLOAT)
// =====================================================================
void enviarTelemetria() {
  char msg[32];

  dtostrf(velocidade, 0, 4, strVel);
  dtostrf(rotacoesTotais, 0, 2, strRot);

  snprintf(msg, sizeof(msg), "%s;%s;%s", ID_CARRO, strVel, strRot);
  digitalWrite(LED_WAIT, HIGH);

  //Serial.println(msg);

  radio.stopListening();
  radio.openWritingPipe(enderecoPista);
  radio.write(msg, strlen(msg) + 1);
  radio.startListening();
  digitalWrite(LED_WAIT, HIGH);

}

// =====================================================================
// SETUP
// =====================================================================
void setup() {

  Serial.begin(9600);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_WAIT, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(EN, OUTPUT);

  pinMode(PIN_HALL, INPUT);

  // PCINT D5
  PCICR  |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT21);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.openReadingPipe(1, enderecoRX);
  radio.startListening();

  Wire.begin();
  sensor.init();
  sensor.setTimeout(500);
  sensor.startContinuous();

  Serial.println("CARRO COM TELEMETRIA INICIALIZADO");
}

// =====================================================================
// LOOP PRINCIPAL
// =====================================================================
void loop() {

  checkRF();

  // ===== SENSOR DE DISTÂNCIA =====
  if (millis() - ultimoUpdateDist >= intervaloDist) {

    ultimoUpdateDist = millis();
    int dist = medirDistancia();
    bool estavaBloqueado = bloqueado;

    if (dist <= 10) {
      bloqueado = true;
      aplicarPWMnoMotor(0);
    }
    else if (dist >= 15) {
      bloqueado = false;
      if (comandoAtivo && estavaBloqueado)
        aplicarPWMnoMotor(ultimoPWM);
    }

    atualizarLEDs();
  }


  //atualiza dados 
  
unsigned long pulsos;
unsigned long tAtual;
unsigned long tAnterior;
unsigned long rejeitados;
bool pulsoNovo;
noInterrupts();
pulsos = pulsosTotais;
tAtual = tPulsoAtual_us;
tAnterior = tPulsoAnterior_us;
pulsoNovo = novoPulso;
rejeitados = pulsosRejeitados;
novoPulso = false;
interrupts();

  rotacoesTotais = (float)pulsos / IMAS_POR_VOLTA; //Sempre atualiza rotações totais (acumulado)


  // ===== calcular velocidade) =====
  if (pulsoNovo && tAnterior > 0 && tAtual > tAnterior) {

    float dt = (tAtual - tAnterior) * 1e-6f; // tempo entre pulsos (s)

    
    if (dt > 0.000001f) { // Proteção simples contra dt absurdo (por segurança)

      float rps = (1.0f / dt) / IMAS_POR_VOLTA;   // rotações/s - aqui mede quantos pulsos daria por segundo, ele tem o valor do dt, ou seja divide 1/dt e sabe que em q s ele daria X pulsos
      velocidade_inst = rps * CIRCUNFERENCIA;     // m/s 

      // Filtro EMA (1ª ordem)
      velocidade_filt += ALPHA * (velocidade_inst - velocidade_filt);
      velocidade = velocidade_filt;
      rpm = rps * 60.0f;

    enviarTelemetria();
    }
  } else {
   
  }


  
}
