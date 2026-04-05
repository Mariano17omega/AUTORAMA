#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Keypad.h>

// OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 16
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= TECLADO =================
const byte LINHAS = 4;
const byte COLUNAS = 4;
char teclasHexa[LINHAS][COLUNAS] = {
  {'*','0','#','D'},
  {'7','8','9','t'},  
  {'4','5','6','r'},  
  {'1','2','3','d'}   // degrau
};

byte pinosLinhas[LINHAS]  = {2, 3, 4, 5};
byte pinosColunas[COLUNAS]= {A0, A1, A2, A3};

Keypad teclado = Keypad(makeKeymap(teclasHexa), pinosLinhas, pinosColunas, LINHAS, COLUNAS);
String stringEntrada = "";
String stringPWM = "";


// ================= RF =================
RF24 radio(9, 10);
const byte endereco1[6] = "ABC123"; // Carro
const byte endereco2[6] = "DEF456"; // Controle 
const byte endereco3[6] = "FGH789"; // Pista


// ================= BOTÕES =================
const int BOTAOstop = 7;  // encerra o envio contínuo do PWM final
const int BOTAOC    = 8;  // GO (catraca)

// ================= OLED =================
// ================= OLED =================
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

// ================= RF SEND =================
static inline void rfSend(const byte* addr, const char* msg) {
  radio.stopListening();
  radio.openWritingPipe(addr);
  radio.write(msg, strlen(msg) + 1); // inclui '\0'
  radio.startListening();
}

void enviarParaCarro(const char* msg) { rfSend(endereco1, msg); }
void enviarParaPista(const char* msg) { rfSend(endereco3, msg); }

// ================= CONTROLE =================

int PWMDEGRAU = 0;

bool modoDegrauPergunta = false;
bool aguardaGO    = false;
int StatusCatrarca = 0;
int  tipoOperacao = 0;  // 1=degrau | 2=rampa inc | 3=rampa tempo

// ======== REGIME PERMANENTE ========
bool mantendoPWM   = false;
int  PWMfinalAtual = 0;     // valor numérico mantido
char prefixoFinal  = 'd';   // 'r' (rampa) ou 'd' (degrau)

// ================= EXECUTOR =================
void executarDegrauOuRampa() {

  // -------- DEGRAU --------
  if (tipoOperacao == 1) {
    char msg[12];
    snprintf(msg, sizeof(msg), "d%d", PWMDEGRAU);

    enviarParaCarro(msg);
    enviarParaPista(msg);

    PWMfinalAtual = PWMDEGRAU;
    prefixoFinal  = 'd';
    mantendoPWM   = true;

    oledShow2Lines("Entrada Degrau.", "PWM Atual: " + String(PWMDEGRAU));
    return;
  }
}


// ================= SETUP =================
void setup() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) {}
  }
  display.setRotation(2);
  oledShow2Lines("Sistema", "Pronto");

  pinMode(BOTAOC, INPUT_PULLUP);
  pinMode(BOTAOstop, INPUT_PULLUP);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.stopListening();
  Serial.println("Arduino controle pronto. evioRF...");
  oledShowInput();
}

// ================= LOOP =================
void loop() {

  // ===== MANUTENÇÃO DO PWM FINAL (REGIME PERMANENTE) ========
 if (mantendoPWM) {

  // 1) GO: apenas alterna a catraca, NÃO encerra o teste
  if (digitalRead(BOTAOC) == LOW) {

    if (StatusCatrarca == 0) {
      enviarParaPista("GO");
      oledShow2Lines("Catrarca Aberta", "PWM Atual: " + String(PWMDEGRAU));
      StatusCatrarca = 1;
      delay(300);

    } else if (StatusCatrarca == 1) {
      enviarParaPista("GO");
      oledShow2Lines("Catrarca Fechada", "PWM Atual: " + String(PWMDEGRAU));
      StatusCatrarca = 0;
      delay(300);
    }

    while (digitalRead(BOTAOC) == LOW) {
      // espera soltar o botão
    }

    return;
  }

  // 2) STOP: encerra o envio contínuo e finaliza o teste
  if (digitalRead(BOTAOstop) == LOW) {
    mantendoPWM   = false;
    tipoOperacao  = 0;
    aguardaGO     = false;
    oledShow2Lines("Teste Finalizado", ":)");
    delay(800);
    oledShowInput();

    while (digitalRead(BOTAOstop) == LOW) {
      // espera soltar o botão
    }

    return;
  }

  return;
}
  

  // ===== Botão Catrarca =====
  if (digitalRead(BOTAOC) == LOW) {
    if (aguardaGO == true && tipoOperacao == 1) {
      enviarParaPista("GO");
      oledShow2Lines("Catrarca Aberta", "Inicio Corrida");
      StatusCatrarca = 1;
      executarDegrauOuRampa();
      delay(300);
      
    } else if (StatusCatrarca == 0) {
      enviarParaPista("GO");
      oledShow2Lines("Catrarca", "Fechada.");
      delay(300);
      StatusCatrarca = 1;
    } else if (StatusCatrarca == 1) {
      enviarParaPista("GO");
      oledShow2Lines("Catrarca", "Aberta." );
      StatusCatrarca = 0;
      delay(300);
    }
    return;
  }

  // ===== TECLADO =====
  char tecla = teclado.getKey();
  if (!tecla) return;

  // ---------------- TECLA "D" ----------------
  if (tecla == 'D') {

  
    // Degrau: salvar PWM e aguardar GO
    if (modoDegrauPergunta) {
      PWMDEGRAU = stringEntrada.toInt();
      stringEntrada = "";
      modoDegrauPergunta = false;

      tipoOperacao = 1;
      aguardaGO = true;

      oledShow2Lines("Entrada Degrau", "Aguard Inicio");
      return;
    }
  }

  // Backspace
  if (tecla == '#') {
    if (stringEntrada.length())
      stringEntrada.remove(stringEntrada.length() - 1);
    oledShowInput();
    return;
  }

  // Atalhos 
  if (tecla == 'd') { stringEntrada=""; modoDegrauPergunta=true; oledShow2Lines("Entrada Degrau","Qual PWM?"); return; }

  // Entrada normal (números)
  stringEntrada += tecla;
  oledShowInput();
}
