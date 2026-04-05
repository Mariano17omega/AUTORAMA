//CODIGO ARDUINO 2 DA PISTA

#include <SoftwareSerial.h>
#include <SD.h>

SoftwareSerial radioIn(8, 9);  // RX=8 (Pista1), TX=9 (Maneira de usar 2 arduinos)
const int chipSelect = 4;

// Sensores locais S1–S5 (A0–A4)
int analogPins[6] = { A0, A1, A2, A3, A4, A5 };
int estadoAntA[6];


// Controle de tempo e PWM
unsigned long tInicio = 0;


bool testeHabilitado = false;
int ultimoPWM = 0;
char ultimoTipoPWM = '-';

// SD
File arquivo;

// LED
const int LEDBLUE_PIN = 5;
bool ledBlueState = false;

// SETUP
// ======================================================================
void setup() {
  Serial.begin(9600);
  radioIn.begin(9600);

  for (int i = 0; i < 6; i++)
    estadoAntA[i] = analogRead(analogPins[i]);

  pinMode(LEDBLUE_PIN, OUTPUT);

  if (!SD.begin(chipSelect)) {
    Serial.println("Falha no SD");
    digitalWrite(LEDBLUE_PIN, LOW);
  } else {
    Serial.println("SD OK");
    digitalWrite(LEDBLUE_PIN, HIGH);
  }
}

// LOOP
// ======================================================================
void loop() {
  if (radioIn.available()) { // RECEBE DADOS DO ARDUINO PISTA 1
  String msg = radioIn.readStringUntil('\n');
  msg.trim();
  //Serial.println(msg);
  //Serial.print("aaaa:");Serial.println(msg.endsWith("GO_aberto"));
  if (msg.endsWith("GO_aberto")) { //a msg termina com 'GO_aberto'?
    
    int p1 = msg.indexOf(';'); //procura posição ;
      if (p1 != -1) {  //segurança se ; n for encontrado 
        String tempoVindo = msg.substring(0, p1);  // começa do primeiro caractere(0) e vai até a posição p1  
       
        gravaEvento("s - Catraca Aberta", tempoVindo); // Chama a função passando o texto e o tempo extraído
      }
    }

    else if (msg.endsWith("GO_fechado")) {
      int p1 = msg.indexOf(';'); //procura posição ;
      if (p1 != -1) {  //segurança se ; n for encontrado 
        String tempoVindo = msg.substring(0, p1);  // começa do primeiro caractere(0) e vai até a posição p1  
       
        gravaEvento("s - Catraca Fechada", tempoVindo); // Chama a função passando o texto e o tempo extraído
      }
    }  
    // 1. Unificamos a verificação: Se começa com L OU se começa com H
    else if (msg.startsWith("L;") || msg.startsWith("H;")) {
    int p1 = msg.indexOf(';');
    int p2 = msg.indexOf(';', p1 + 1);
    int p3 = msg.indexOf(';', p2 + 1);
    int p4 = msg.indexOf(';', p3 + 1);
    int p5 = msg.indexOf(';', p4 + 1);
    int p6 = msg.indexOf(';', p5 + 1);
    int p7 = msg.indexOf(';', p6 + 1);
    int p8 = msg.indexOf(';', p7 + 1);

    if (p1 > 0 && p2 > p1 && p3 > p2 && p4 > p3 && p5 > p4 && p6 > p5 && p7 > p6 && p8 > p7) {
        // Extraímos TUDO como String para não perder formatação
        String sTempo = msg.substring(p1 + 1, p2);
        String sNomeS = msg.substring(p2 + 1, p3); // Recebe "S1"
        String sTipoP = msg.substring(p3 + 1, p4); // Recebe "d"
        String sValP  = msg.substring(p4 + 1, p5); // Recebe "333"
        String sVel   = msg.substring(p5 + 1, p6);
        String sDist  = msg.substring(p6 + 1, p7);
        String sTipoS = msg.substring(p7 + 1, p8); // Recebe "S" ou "CX"
        String sRot   = msg.substring(p8 + 1);

        // Chamamos a nova função que aceita tudo como String
        gravaLinhaBruta(sTempo, sNomeS, sTipoP, sValP, sVel, sDist, sTipoS, sRot);
    }
}}
  for (int i = 0; i < 6; i++) {

    int leitura = analogRead(analogPins[i]);
    //Serial.println(leitura);
      if (leitura < 700 && estadoAntA[i] >= 700) {
      int sensorID = i + 1;
      char buffer[8];
      sprintf(buffer, "S%d", sensorID);      
      radioIn.println(buffer);
      Serial.println(buffer);
}
estadoAntA[i] = leitura;
  }


}  //fim loop


// EVENTOS
void gravaEvento(String texto, String tempoString) { //chamado apenas quando a catrarca abre

  unsigned long tMicros = strtoul(tempoString.c_str(), NULL, 10);// Converte a String do tempo de volta para número (unsigned long)
  float tSeg = tMicros / 1000000.0;  //convert p numero com 3 casas dec 
  // Monta a linha para o SD
  String linha = String(tSeg , 3) + texto;
  Serial.println("SD-> " + linha);
  File arquivo = SD.open("dados.txt", FILE_WRITE);
  if (arquivo) {
    arquivo.println(linha);
    arquivo.close();
  }
}

void gravaLinhaBruta(String t, String nomeS, String tPWM, String vPWM, String vel, String dist, String tipo, String rot) {
    
    unsigned long tMicros = strtoul(t.c_str(), NULL, 10); // converte de string para unsigned
    float tSeg = tMicros / 1000000.0;// Converte apenas o tempo para segundos

    // Monta a linha EXATAMENTE com o que veio, sem somar +1 ou usar globais
    String linha = String(tSeg, 2) + ", "
                 + nomeS + ", "    // Já vem "S1"
                 + tPWM + ", "     // Já vem "d"
                 + vPWM + ", "     // Já vem "333"
                 + vel + ", "      // Já vem "0.181"
                 + dist + ", "     // Já vem "4.2850"
                 + tipo + ", "     // Já vem "S" ou "C1"
                 + rot;            // Já vem "0.00"

    Serial.println("SD-> " + linha);

    File arquivo = SD.open("dados.txt", FILE_WRITE);
    if (arquivo) {
        arquivo.println(linha);
        arquivo.close();
    }
}
