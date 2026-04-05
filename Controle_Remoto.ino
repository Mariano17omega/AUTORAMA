#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Keypad.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================================
// OLED
// =====================================================================
#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 16
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================================================================
// TECLADO 4x4
// =====================================================================
const byte LINHAS  = 4;
const byte COLUNAS = 4;
char teclasHexa[LINHAS][COLUNAS] = {
  {'*', '0', '#', 'D'},
  {'7', '8', '9', 't'},
  {'4', '5', '6', 'r'},
  {'1', '2', '3', 'd'}  // 'd' = degrau
};

byte pinosLinhas[LINHAS]   = {2, 3, 4, 5};
byte pinosColunas[COLUNAS] = {A0, A1, A2, A3};

Keypad teclado = Keypad(makeKeymap(teclasHexa), pinosLinhas, pinosColunas, LINHAS, COLUNAS);
String stringEntrada = "";

// =====================================================================
// RF
// CORREÇÃO: removido endereco2 "DEF456" que estava declarado mas nunca usado
// =====================================================================
RF24 radio(9, 10);
const byte enderecoCarroRF[6] = "ABC123"; // Carro
const byte enderecoPistaRF[6] = "FGH789"; // Pista (Arduino 1)

// =====================================================================
// BOTÕES
// =====================================================================
const int BOTAOstop = 7; // encerra o envio contínuo
const int BOTAOC    = 8; // GO (catraca)

// =====================================================================
// OLED — FUNÇÕES DE EXIBIÇÃO
// =====================================================================
void oledClear() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void oledShow2Lines(const String& l1, const String& l2) {
  oledClear();
  display.setCursor(0, 0);
  display.println(l1);
  display.setCursor(0, 8);
  display.println(l2);
  display.display();
}

void oledShowInput() {
  oledShow2Lines("Inicio :)", stringEntrada);
}

// =====================================================================
// RF — ENVIO
// =====================================================================
static inline void rfSend(const byte* addr, const char* msg) {
  radio.stopListening();
  radio.openWritingPipe(addr);
  radio.write(msg, strlen(msg) + 1); // inclui '\0'
  radio.startListening();
}

void enviarParaCarro(const char* msg) { rfSend(enderecoCarroRF, msg); }
void enviarParaPista(const char* msg) { rfSend(enderecoPistaRF, msg); }

// =====================================================================
// CONTROLE DE ESTADO
// =====================================================================
int  PWMDEGRAU        = 0;
bool modoDegrauPergunta = false;
bool aguardaGO          = false;
int  StatusCatraca      = 0;
int  tipoOperacao       = 0; // 1=degrau | (2=rampa inc | 3=rampa tempo: não implementados)

bool mantendoPWM   = false;
int  PWMfinalAtual = 0;
char prefixoFinal  = 'd';

// =====================================================================
// EXECUTOR DE ENTRADA
// =====================================================================
void executarDegrauOuRampa() {
  if (tipoOperacao == 1) {
    char msg[12];
    snprintf(msg, sizeof(msg), "d%d", PWMDEGRAU);

    enviarParaCarro(msg);
    enviarParaPista(msg);

    PWMfinalAtual = PWMDEGRAU;
    prefixoFinal  = 'd';
    mantendoPWM   = true;

    oledShow2Lines("Entrada Degrau.", "PWM: " + String(PWMDEGRAU));
  }
  // tipoOperacao 2 e 3 (rampa): não implementados
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true); // trava se o display não inicializar
  }
  display.setRotation(2);
  oledShow2Lines("Sistema", "Pronto");

  pinMode(BOTAOC,    INPUT_PULLUP);
  pinMode(BOTAOstop, INPUT_PULLUP);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.stopListening(); // controle remoto só transmite

  Serial.println("Controle remoto pronto. Envio RF habilitado.");
  oledShowInput();
}

// =====================================================================
// LOOP
// =====================================================================
void loop() {

  // ===== MODO REGIME PERMANENTE (PWM enviado, aguardando STOP) =====
  if (mantendoPWM) {

    // GO: alterna a catraca sem encerrar o teste
    if (digitalRead(BOTAOC) == LOW) {
      if (StatusCatraca == 0) {
        enviarParaPista("GO");
        oledShow2Lines("Catraca Aberta", "PWM: " + String(PWMDEGRAU));
        StatusCatraca = 1;
      } else {
        enviarParaPista("GO");
        oledShow2Lines("Catraca Fechada", "PWM: " + String(PWMDEGRAU));
        StatusCatraca = 0;
      }
      delay(300);
      while (digitalRead(BOTAOC) == LOW); // aguarda soltar
      return;
    }

    // STOP: encerra o envio contínuo
    if (digitalRead(BOTAOstop) == LOW) {
      mantendoPWM  = false;
      tipoOperacao = 0;
      aguardaGO    = false;
      oledShow2Lines("Teste Finalizado", ":)");
      delay(800);
      oledShowInput();
      while (digitalRead(BOTAOstop) == LOW); // aguarda soltar
      return;
    }

    return;
  }

  // ===== BOTÃO GO (antes do experimento iniciar) =====
  if (digitalRead(BOTAOC) == LOW) {
    if (aguardaGO && tipoOperacao == 1) {
      enviarParaPista("GO");
      // CORREÇÃO: "Catrarca" → "Catraca"
      oledShow2Lines("Catraca Aberta", "Inicio Corrida");
      StatusCatraca = 1;
      executarDegrauOuRampa();
    } else if (StatusCatraca == 0) {
      enviarParaPista("GO");
      oledShow2Lines("Catraca", "Fechada.");
      StatusCatraca = 1;
    } else {
      enviarParaPista("GO");
      oledShow2Lines("Catraca", "Aberta.");
      StatusCatraca = 0;
    }
    delay(300);
    return;
  }

  // ===== LEITURA DO TECLADO =====
  char tecla = teclado.getKey();
  if (!tecla) return;

  // Tecla 'D': confirma o valor digitado como PWM do degrau
  if (tecla == 'D') {
    if (modoDegrauPergunta) {
      PWMDEGRAU           = stringEntrada.toInt();
      stringEntrada       = "";
      modoDegrauPergunta  = false;
      tipoOperacao        = 1;
      aguardaGO           = true;
      oledShow2Lines("Entrada Degrau", "Aguard. Inicio");
      return;
    }
  }

  // Tecla '#': apaga último caractere
  if (tecla == '#') {
    if (stringEntrada.length())
      stringEntrada.remove(stringEntrada.length() - 1);
    oledShowInput();
    return;
  }

  // Tecla 'd': inicia modo de entrada de degrau
  if (tecla == 'd') {
    stringEntrada      = "";
    modoDegrauPergunta = true;
    oledShow2Lines("Entrada Degrau", "Qual PWM?");
    return;
  }

  // Demais teclas: entrada numérica
  stringEntrada += tecla;
  oledShowInput();
}
