//CODIGO ARDUINO 1 DA PISTA

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// RÁDIO RF24
RF24 radio(9, 10); // CE, CSN

const byte enderecoPista[6] = "FGH789";  // escuta
const byte enderecoCarro[6]    = "ABC123";  // escreve

// SERVO (CATRACA)
Servo servoMotor;
const int SERVO_PIN = 4;

// LEDS

const int LEDYELLOW    = 6;
const int LEDRED      = 7;
const int LEDGREEN    = 8;


// SERIAL PARA ARDUINO PISTA 2
SoftwareSerial serialPista2(A3, A2); // RX, TX
String msgRadio  = "";
// SENSORES ANALÓGICOS DA PISTA 1 → S7, S8, S9
int analogPins[3] = {A0, A1, A4};
int estadoAntA[3];

// CONTROLE
// ============================================================
char mensagem[32];

int anguloAtual  = 83;   // posição inicial da catraca
bool ledBlueState = false;

char carroID[5];          // ex: "C1"
float velCarro = 0.0;     // velocidade vinda do carro
float RotDelta = 0;        // pulsos/RPM desde último envio
float RotCarroAtual = 0;
float limitador = 10; //não considera valores exagerados de rotação 
float limitadorvel = 5.0; //não considera valores exagerados de rotação 
long RotUltimoSensor = 0; // RPM acumulado no último sensor

// Controle de sensores
int ultimoSensorGravado = -1;
int ultimoSensorDetectado = -1;
float ultimaPosicaoRegistrada = 0.000;
float UltimoTrecho = 0.000;
float PosProxSensor = 0.000;
float PosProxSensorLinear = 0.000;
float DistProxSensor = 0.000;
float deltaS = 0.000;
float deltaT = 0.000;
float deltaV = 0.000;
float difRof = 0;
float RotCarroAnt = 0;
float RotInc = 0;
int prox = 0;
int Agr = 0;

int inicio = 0;

float poslinearsensor = 0.000;
float distanciaTotal = 0.000;
float ultimaVelocidadeCalculada = 0.000;
float ultimaVelocidadeCalculadaAnt = 0.000;

unsigned long tUltimoPontoSensor = 0;
unsigned long tAgora = 0;
int ultimoSensorAcelPos = 0;  //Armazena o último sensor onde a aceleração foi positiva
float TrechoAgr = 0.000; //---------------
int qtd = 0;

float TrechoAnt = 0.0; //---------------

float distTrecho[9] = {
  0.355,  // S1→S2
  0.280,  // S2→S3  
  0.275,  // S3→S4
  0.280,  // S4→S5
  0.280,  // S5→S6
  0.750,  // S6→S7
  0.700,  // S7→S8
  0.590,  // S8→S9
  0.775   // S9→S1
};

float pos[9] = {
  4.285,  // S1
  0.355,  // S2
  0.635,  // S3
  0.910,  // S4
  1.190,  // S5
  1.470,  // S6
  2.220,  // S7
  2.920,  // S8
  3.510   // S9
};

// Controle de tempo e PWM
unsigned long tInicio = 0;


bool testeHabilitado = false;
int ultimoPWM = 0;
char ultimoTipoPWM = '-';


#define TAM_BUFFER 50

char buffer[TAM_BUFFER];

char iniciale[TAM_BUFFER];
char nomeSensor[4];
byte idx = 0;
// ============================================================
// SETUP
// ============================================================
void setup() {

  Serial.begin(9600);
  serialPista2.begin(9600);

  for (int i = 0; i < 6; i++) {
    pinMode(analogPins[i], INPUT); // Garante que os pinos são entradas
    estadoAntA[i] = analogRead(analogPins[i]); // Tira a "foto" inicial da luminosidade
  }


  // LEDs
  pinMode(LEDRED, OUTPUT);
  pinMode(LEDGREEN, OUTPUT);
  pinMode(LEDYELLOW, OUTPUT);

  digitalWrite(LEDGREEN, HIGH);
  digitalWrite(LEDRED, LOW); 
  digitalWrite(LEDYELLOW, LOW);

  // Rádio
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.openReadingPipe(1, enderecoPista); // controle → pista
  radio.openReadingPipe(2, enderecoCarro);  // carro → pista (se precisar no futuro)
  radio.startListening();
  Serial.println("Arduino Pista 1 pronto. Aguardando RF...");
}


// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {

// No Arduino 1
if (serialPista2.available() > 0) {
    String entrada = "";
    while (serialPista2.available() > 0) {
        char c = serialPista2.read();
        if (c == '\n') break;
        if (c != '\r') entrada += c;
        delay(1); // Pequeno delay para o buffer serial encher
    }
    
    entrada.trim();
    if (entrada.length() > 0) {
        //Serial.print("--- RECEBIDO: ");
        //Serial.println(entrada);

        if (entrada.startsWith("S")) {
          //Serial.print(entrada);
            int n = entrada.substring(1).toInt();
            if (n >= 1 && n <= 6) processaSensor(n - 1);
        } 
    }
}
// FINALIZA AQUI OS RECEBIMENTOS DO OUTRO ARDUINO
  for (int i = 0; i < 3; i++) {
    int leitura = analogRead(analogPins[i]);

    if (leitura <= 700 && estadoAntA[i] > 700) {
      
      int idSensor = i + 7; // Mapeia 0,1,2 para S7, S8, S9
    
      processaSensor(idSensor); 
    }
    
    // Atualiza o estado anterior para a próxima volta do loop
    estadoAntA[i] = leitura;
  }

// 2) RECEBE RF DO CONTROLE E REPASSA SEMPRE
if (radio.available()) {
  digitalWrite(LEDYELLOW, HIGH);
  memset(mensagem, 0, sizeof(mensagem));
  radio.read(mensagem, sizeof(mensagem));

  Serial.print("RF recebido: ");
  Serial.println(mensagem);
  
  // PACOTE DE TELEMETRIA DO CARRO (Cx,vel,rpm)
  if (mensagem[0] == 'C') {
      RecebeHall(mensagem);
  }
  
  else if (mensagem[0] == 'd') { // PWM DEGRAU 
    ultimoTipoPWM = 'd';
    String msg = String(mensagem);
    msg.trim();
    ultimoPWM = msg.substring(1).toInt();
    //Serial.print("ultimoTipoPWM");Serial.println(ultimoTipoPWM);
    //Serial.print("ultimoPWM");Serial.println(ultimoPWM);
  }
  else if (strcmp(mensagem, "GO") == 0) {
    servoMotor.attach(SERVO_PIN);

    if (anguloAtual == 100) {

      anguloAtual = 220;
      digitalWrite(LEDGREEN, LOW);
      digitalWrite(LEDRED, HIGH);
      
      serialPista2.println("Catraca aberta");
      //Serial.println("Catraca aberta");
      testeHabilitado = true;
      ultimaVelocidadeCalculada = 0.0;
      distanciaTotal = 0.0;
      ultimoSensorGravado = -1;
      ultimoSensorDetectado = -1;
      tInicio = micros(); //fixa o tempo inicial 
      tAgora = 0;

      snprintf(iniciale, sizeof(iniciale), "%lu;GO_aberto", tAgora);
      //Serial.println(iniciale);
      serialPista2.println(iniciale);
      


    } else {

      anguloAtual = 100;
      digitalWrite(LEDGREEN, HIGH);
      digitalWrite(LEDRED, LOW);
      tAgora = (micros() - tInicio) ;
      snprintf(iniciale, sizeof(iniciale), "%lu;GO_fechado", tAgora);
      //Serial.println(iniciale);
      serialPista2.println(iniciale);

      
    }

    servoMotor.write(anguloAtual);
    delay(100);
    servoMotor.detach();
  }
  digitalWrite(LEDYELLOW, LOW);
}
    
}


void RecebeHall(const String& mensagem) { // recebe do arduino1 itens iniciando com C
    Serial.print("mensagemmmmmmmm");Serial.print(mensagem);
    String msg = String(mensagem);
    ultimoPWM = msg.substring(1).toInt();
    msg.trim();
    
    //descobre onde estão as vírgulas
    int p1 = msg.indexOf(';'); // salva na variável p1 a posição da primeira vírgula
    int p2 = msg.indexOf(';', p1 + 1); //salva na variável p2 a posição da segunda vírgula

    //Serial.println(p1);
    //Serial.println(p2);


    if (p1 > 0 && p2 > p1) { //apenas pra filtrar lixos que as vezes vem bugado 

      String sID  = msg.substring(0, p1); //salva todos os digitos entre posição 0 e posição do p1 
      String sVel = msg.substring(p1 + 1, p2); //pega logo depois da primeira vírgula até antes da segunda.
      String sRPM = msg.substring(p2 + 1); //pega logo depois da segunda vírgula até o final.

      
      //Serial.print ("sID: "); Serial.println (sID);
      //Serial.print ("sVel: ");Serial.println (sVel);
     // Serial.print ("sRPM: "); Serial.println (sRPM);
      
      sID.toCharArray(carroID, sizeof(carroID)); // converte pra vetor(carroID) terminada em \0 
      velCarro = sVel.toFloat(); //converte em float (numero)
      RotCarroAtual = sRPM.toFloat()  - RotInc - 1 ;  // converte para numero inteiro (numero de rotações)
      difRof = RotCarroAtual  ;

      if (RotCarroAtual >= RotCarroAnt && difRof < limitador && velCarro < limitadorvel ) { //mede o PWM ao longo do tempo desconsiderando valor negativo 
      RotDelta = RotCarroAtual - RotUltimoSensor; //rotação desde o ultimo sensor      
      gravaPWM_C();

      }
      RotCarroAnt = RotCarroAtual;
      
    }
} 

void gravaPWM_C() { //recebe um bloco de infos 

  //Serial.println(testeHabilitado);
  if (!testeHabilitado) return;
  tAgora = micros() - tInicio; //atualiza tempo 

  if (RotDelta <= 0) return;// Se não houve avanço real, não grava

  const float diametroRoda = 0.024;
  const float circRoda = PI * diametroRoda;

  float deltaDistRPM = (RotDelta) * circRoda; //calculando sistanccia percorrida de acordo com as rotações   
  float distanciaEstimativa = ultimaPosicaoRegistrada + deltaDistRPM; //considera a posição do ultimo sensor + dist percorrida estimada
  
  // Limite por sensor físico
  if (distanciaEstimativa > PosProxSensorLinear) {
    distanciaEstimativa = PosProxSensorLinear;
  }

  distanciaTotal = distanciaEstimativa;
  ultimaVelocidadeCalculada = velCarro;



  if(RotCarroAtual != RotCarroAnt){
    EnviaSerialHall();
  }
  RotCarroAnt = RotCarroAtual;
  
}

 

void processaSensor(int idxAtual) {
  
  if (idxAtual == ultimoSensorGravado)  // evita gravar o mesmo sensor duas vezes seguidas
    return;

  tAgora = micros() - tInicio;  // calculando tempo em ms
  //Serial.print("tAgora:");Serial.println(tAgora);
  Agr = idxAtual % 9;  //descubro qual sensor (antre 1-9 acabou de passar) 
  //Serial.print("tAgora:");Serial.println(tAgora);
  TrechoAgr = pos[Agr]; // ex: se foi o sensor 9, identifica que o resto de 9 resto 9 é 0 msm e retorna pos[0] = 4.280   
   //Serial.print("TrechoAgr:");Serial.println(TrechoAgr);
  float L = pos[0];  // comprimento total da pista (4.280) 
  //Serial.print("L:");Serial.println(L);
  //IF Abaixo serve de segurança para caso de falha de algum sensor 

  if (TrechoAgr >= TrechoAnt) { //1 - se o techo agr é maior que o anterior (padrão)
    UltimoTrecho = TrechoAgr - TrechoAnt; //basta subrair os trechos 
    TrechoAnt = TrechoAgr; //atualiza trecho ante
  } else {  // 2 - quando o agr é menor (ex: pos[1] - pos[0] ou pos[2] - pos[7])
    UltimoTrecho = (L - TrechoAnt) + TrechoAgr;  // atualiza o ultimo trecho passado
    //ex:  pos[2] - pos[7] =  0,355 - 2,215 = -1.86 ; o que ele vai fazer: (L - pos[7]) + pos[2] = (4.280 - 2.215 ) + 0.635 = 2.7 - valor ofc queele percorreu 
    TrechoAnt = TrechoAgr; //atualiza trecho ante
  }

  //Serial.print("TrechoAnt:");Serial.println(TrechoAnt);


  prox = (idxAtual + 1) % 9; //descubro pos provavel do prox sensor ativado 
  //Serial.print("prox:");Serial.println(prox);
  PosProxSensor = pos[prox];  // posição do próximo sensor (seu array de posições)
  //Serial.print("PosProxSensor:");Serial.println(PosProxSensor);
  DistProxSensor = distTrecho[prox];  // trecho até o próximo sensor considerando o sensor atual 
  //Serial.print("DistProxSensor:");Serial.println(DistProxSensor);
//Entrou no processaSensor, até aqui eu tenho (sensor ativado, dist do ssensor atual ao ultimo que foi ativado, previsibilidade para o prox sensor)
//Serial.print("iniciooooo:");Serial.println(inicio);
  // PRIMEIRO SENSOR DO TESTE
  if (inicio == 0) {  //verifica se é o primeiro sensor a ser ativado, se sim, inicia tudo no 0
    ultimaPosicaoRegistrada = 0.0;
    distanciaTotal = 0.0; 
    deltaT = tAgora/ 1e6;        //Atualiza tempo
    deltaS = 0.03;  // nesse caso é por padrão 3cm  (medição fisica)
    ultimaVelocidadeCalculada = deltaS / deltaT; //velocidade atual
    //Serial.print("ultimaVelocidadeCalculada:");Serial.println(ultimaVelocidadeCalculada);
    TrechoAnt = 0.0; //atualiza trecho ant 
    RotInc = RotCarroAtual + 1  ;

    EnviaSerial(Agr);

    tUltimoPontoSensor = tAgora;                                     //variável para  ultimo tempo atualizado
    UltimoTrecho = 0.0;                                              //variável para atualizar a posição do sensor que usei, neste caso do *inicio* sempre será a posição 0
        
    poslinearsensor = 0.0;      //atualiza que a variavel ultimavelocidade calculada                                     //iniciavariavel que acompanha trecho linear do SENSOR (é o primeiro sensor, ele dá inicio)
    PosProxSensorLinear = ultimaPosicaoRegistrada + pos[1];  //Atualiza a proxima posição do sensor
    inicio = 1;   
    //Serial.print("inicio2222:");Serial.println(inicio);                                                   //n é mais o primeiro sensor sensor
  }

  else {                                                 
    poslinearsensor = poslinearsensor + UltimoTrecho;    //define qual a posiçãolinear do sensor que acabou de passar
    deltaT = (tAgora - tUltimoPontoSensor) / 1e6;        //Atualiza variação do tempo
    deltaS = poslinearsensor - ultimaPosicaoRegistrada;  //calcula o Delta S -> dif entre posições
    ultimaVelocidadeCalculada = deltaS / deltaT; 

    distanciaTotal = poslinearsensor;  //independente do que aconteça a distância total sempre será a posição linear do sensor sempre será o valor da posição daquele sensor + trajeto anterior

    EnviaSerial(Agr);


    
    tUltimoPontoSensor = tAgora;
    
    PosProxSensorLinear = distanciaTotal + DistProxSensor;
    ultimaPosicaoRegistrada = distanciaTotal; //poslinearsensor;
  }

  ultimoSensorGravado = idxAtual; //para saber o ultimo sensor que passou RotDelta
  RotUltimoSensor = RotCarroAtual;
  RotDelta = 0;

}



void EnviaSerial(int sensorPassado){

  char sVel[10];
    char sDist[10];
    char sRot[10];

    dtostrf(ultimaVelocidadeCalculada, 1, 3, sVel);
    dtostrf(distanciaTotal, 1, 4, sDist);
    dtostrf(RotCarroAtual, 1, 2, sRot);
    sprintf(nomeSensor, "S%d", sensorPassado + 1);

    char buffer[100]; // Aumentei o tamanho para garantir que caiba tudo
    snprintf(buffer, sizeof(buffer), "L;%lu;%s;%c;%d;%s;%s;S;%s",
            tAgora,
            nomeSensor,
            ultimoTipoPWM,
            ultimoPWM,
            sVel,   
            sDist,  
            sRot);  

    Serial.print("buffer:");Serial.println(buffer);
    serialPista2.println(buffer);
}

void EnviaSerialHall() {
    // 1. Strings auxiliares para conversão de float (evitar o erro do '?')
    char sVel[10], sDist[10], sRot[10];
    dtostrf(ultimaVelocidadeCalculada, 1, 3, sVel);
    dtostrf(distanciaTotal, 1, 4, sDist);
    dtostrf(RotCarroAtual, 1, 2, sRot);

    // 2. Buffer para montagem da mensagem
    char buffer[120]; 

    // 3. Montagem seguindo o padrão:
    // H; tempo; ultimoSensor; tipoPWM; valorPWM; velocidade; distancia; IDCarro; rotação
    snprintf(buffer, sizeof(buffer), "H;%lu;S%d;%c;%d;%s;%s;%s;%s",
            tAgora,
            ultimoSensorGravado + 1, // Ex: S1, S2... baseado no último sensor que o carro passou
            ultimoTipoPWM,
            ultimoPWM,
            sVel,
            sDist,
            carroID, // String (ex: C1)
            sRot);

    // 4. Envio
    Serial.print("buffer Hall:"); Serial.println(buffer);
    serialPista2.println(buffer);
}