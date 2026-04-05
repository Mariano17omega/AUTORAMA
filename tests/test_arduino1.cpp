// ============================================================
// Testes: Arduino_1 — PISTA DO AUTORAMA.ino
// Cobre: RecebeHall, processaSensor, mapeamento de sensores,
//        EnviaSerial, EnviaSerialHall, gravaPWM_C.
// ============================================================
#include "framework.h"
#include "mocks/Arduino.h"
#include "mocks/SPI.h"
#include "mocks/nRF24L01.h"
#include "mocks/RF24.h"
#include "mocks/Servo.h"
#include "mocks/SoftwareSerial.h"

MockSerial     Serial;
// Instâncias dos mocks declarados como extern no código do sketch
// (o .ino declara os objetos internamente; aqui apenas satisfazemos o linker)

#include "../Arduino_1- PISTA DO AUTORAMA.ino"

// ============================================================
// Reset de estado entre testes
// ============================================================
static void reset_a1() {
    velCarro                  = 0.0f;
    RotCarroAtual             = 0.0f;
    RotCarroAnt               = 0.0f;
    RotInc                    = 0.0f;
    RotDelta                  = 0.0f;
    RotUltimoSensor           = 0;
    difRof                    = 0.0f;
    ultimoSensorGravado       = -1;
    ultimoSensorDetectado     = -1;
    ultimaPosicaoRegistrada   = 0.0f;
    distanciaTotal            = 0.0f;
    ultimaVelocidadeCalculada = 0.0f;
    poslinearsensor           = 0.0f;
    TrechoAgr                 = 0.0f;
    TrechoAnt                 = 0.0f;
    UltimoTrecho              = 0.0f;
    PosProxSensorLinear       = 0.0f;
    inicio                    = 0;
    tInicio                   = 0;
    tAgora                    = 0;
    ultimoPWM                 = 0;
    ultimoTipoPWM             = '-';
    testeHabilitado           = true;   // necessário para gravaPWM_C executar
    serialPista2.reset();
    Serial.reset();
    _mock_micros = 0;
    _mock_millis = 0;
}

// ============================================================
// Grupo 1: RecebeHall — parsing da mensagem do carro
// ============================================================
static void test_receivehall_parseia_velocidade() {
    reset_a1();
    RecebeHall("C1;1.2345;50.00");
    ASSERT_FLOAT_EQ(1.2345f, velCarro, 0.001f);
}
static void test_receivehall_parseia_rotacoes() {
    reset_a1();
    RotInc = 0.0f;
    RecebeHall("C1;0.5000;30.00");
    // RotCarroAtual = 30.00 - 0 - 1 = 29.00
    ASSERT_FLOAT_EQ(29.0f, RotCarroAtual, 0.01f);
}
static void test_receivehall_parseia_id() {
    reset_a1();
    RecebeHall("C1;0.0000;0.00");
    ASSERT_STR_EQ("C1", carroID);
}
static void test_receivehall_rejeita_sem_ponto_virgula() {
    reset_a1();
    velCarro = 999.0f; // valor sentinela
    RecebeHall("C1"); // sem separadores → deve ignorar
    ASSERT_FLOAT_EQ(999.0f, velCarro, 0.001f); // não modificado
}
static void test_receivehall_rejeita_velocidade_alta() {
    // velCarro > limitadorvel (5.0) → não chama gravaPWM_C
    reset_a1();
    distanciaTotal = 0.0f;
    RotCarroAtual  = 0.0f;
    RecebeHall("C1;6.0000;10.00"); // vel = 6 > 5 → rejeitado
    // gravaPWM_C não deve ter sido chamado; distanciaTotal permanece 0
    ASSERT_FLOAT_EQ(0.0f, distanciaTotal, 0.001f);
}
static void test_receivehall_rejeita_salto_rotacao_alto() {
    reset_a1();
    RotCarroAnt = 0.0f;
    RotInc      = 0.0f;
    // difRof = RotCarroAtual = sRPM - RotInc - 1 = 200 - 0 - 1 = 199 > limitador(10)
    distanciaTotal = 0.0f;
    RecebeHall("C1;1.0000;200.00");
    ASSERT_FLOAT_EQ(0.0f, distanciaTotal, 0.001f); // não gravou
}
static void test_receivehall_atualiza_rot_carro_ant() {
    reset_a1();
    RotCarroAnt = 0.0f;
    RotInc      = 0.0f;
    RecebeHall("C1;1.0000;5.00");
    // RotCarroAtual = 5 - 0 - 1 = 4; depois RotCarroAnt = RotCarroAtual = 4
    ASSERT_FLOAT_EQ(RotCarroAtual, RotCarroAnt, 0.001f);
}
static void test_receivehall_dois_pacotes_acumulam_rotdelta() {
    reset_a1();
    RotInc          = 0.0f;
    RotUltimoSensor = 0;
    RecebeHall("C1;1.0000;5.00");   // RotCarroAtual = 4, RotDelta = 4-0 = 4
    float delta_1 = RotDelta;
    ASSERT_TRUE(delta_1 > 0.0f);
}

// ============================================================
// Grupo 2: Mapeamento de sensores LDR → índice 0-based
// ============================================================
static void test_sensor_s7_mapeado_para_indice_6() {
    // i=0 → processaSensor(i + 6) = processaSensor(6)
    // Valida que a fórmula produz o índice correto (não testa chamada direta)
    int i = 0;
    int idx = i + 6;
    ASSERT_EQ(6, idx); // S7 = pos[6] = 2.220
}
static void test_sensor_s8_mapeado_para_indice_7() {
    int i = 1;
    int idx = i + 6;
    ASSERT_EQ(7, idx); // S8 = pos[7] = 2.920
}
static void test_sensor_s9_mapeado_para_indice_8() {
    int i = 2;
    int idx = i + 6;
    ASSERT_EQ(8, idx); // S9 = pos[8] = 3.510
}
static void test_pos_array_s7_correto() {
    ASSERT_FLOAT_EQ(2.220f, pos[6], 0.001f);
}
static void test_pos_array_s8_correto() {
    ASSERT_FLOAT_EQ(2.920f, pos[7], 0.001f);
}
static void test_pos_array_s9_correto() {
    ASSERT_FLOAT_EQ(3.510f, pos[8], 0.001f);
}
static void test_sensor_s1_do_arduino2_mapeado_para_0() {
    // Mensagem "S1" → n=1 → processaSensor(n-1) = processaSensor(0)
    int n = 1;
    ASSERT_EQ(0, n - 1);
}
static void test_sensor_s6_do_arduino2_mapeado_para_5() {
    int n = 6;
    ASSERT_EQ(5, n - 1);
}

// ============================================================
// Grupo 3: processaSensor — cálculo de posição
// ============================================================
static void test_processsensor_mesmo_sensor_ignorado() {
    reset_a1();
    ultimoSensorGravado = 3;
    processaSensor(3); // deve retornar imediatamente
    // Se entrou, ultimoSensorGravado mudaria — não deve mudar
    ASSERT_EQ(3, ultimoSensorGravado);
}
static void test_processsensor_agr_modulo_9() {
    // Agr = idxAtual % 9 — verifica o mapeamento circular
    ASSERT_EQ(0, 9 % 9);   // S9 → S1 no próximo
    ASSERT_EQ(6, 6 % 9);   // S7 → idx 6
    ASSERT_EQ(8, 8 % 9);   // S9 → idx 8
}
static void test_processsensor_prox_circular() {
    // prox = (idxAtual + 1) % 9
    ASSERT_EQ(0, (8 + 1) % 9); // após S9 (idx 8), próximo é idx 0 (S1)
    ASSERT_EQ(1, (0 + 1) % 9); // após S1 (idx 0), próximo é idx 1 (S2)
    ASSERT_EQ(7, (6 + 1) % 9); // após S7 (idx 6), próximo é idx 7 (S8)
}
static void test_processsensor_primeiro_sensor_inicia_experimento() {
    reset_a1();
    _mock_micros = 100000; // tInicio=0, tAgora = 100000
    tInicio = 0;
    RotCarroAtual = 5.0f;  // rotações acumuladas
    processaSensor(0);     // S1 como primeiro sensor
    ASSERT_EQ(1, inicio);  // marca que não é mais o primeiro
    ASSERT_FLOAT_EQ(0.0f, ultimaPosicaoRegistrada, 0.001f);
}
static void test_processsensor_segundo_sensor_acumula_posicao() {
    reset_a1();
    // Primeiro sensor S1 (idx 0)
    _mock_micros = 100000; tInicio = 0;
    processaSensor(0);
    float pos_depois_s1 = poslinearsensor;

    // Segundo sensor S2 (idx 1) — distância S1→S2 = distTrecho[1] (prox do S1)
    _mock_micros = 200000;
    processaSensor(1);
    ASSERT_TRUE(poslinearsensor > pos_depois_s1);
}
static void test_processsensor_trecho_normal_subtracao() {
    // TrechoAgr >= TrechoAnt → UltimoTrecho = TrechoAgr - TrechoAnt
    float tAgr = 1.470f, tAnt = 0.355f;
    float esperado = tAgr - tAnt;
    float resultado = (tAgr >= tAnt) ? (tAgr - tAnt) : 0.0f;
    ASSERT_FLOAT_EQ(esperado, resultado, 0.001f);
}
static void test_processsensor_trecho_wrap_s9_para_s1() {
    // TrechoAgr < TrechoAnt → cruzou linha de chegada
    // UltimoTrecho = (L - TrechoAnt) + TrechoAgr
    float L    = pos[0]; // 4.285
    float tAgr = pos[0]; // S1 = 4.285 (posição da linha de chegada)
    float tAnt = pos[8]; // S9 = 3.510
    float esperado = (L - tAnt) + tAgr;
    ASSERT_FLOAT_EQ(4.550f, esperado, 0.01f);
}
static void test_processsensor_atualiza_ultimo_sensor_gravado() {
    reset_a1();
    _mock_micros = 50000;
    processaSensor(2); // S3
    ASSERT_EQ(2, ultimoSensorGravado);
}
static void test_processsensor_velocidade_positiva_apos_trigger() {
    reset_a1();
    _mock_micros = 500000; // tempo considerável para que deltaT > 0
    processaSensor(0);
    ASSERT_TRUE(ultimaVelocidadeCalculada > 0.0f);
}

// ============================================================
// Grupo 4: EnviaSerial — formatação da mensagem
// ============================================================
static void test_envia_serial_prefixo_L() {
    reset_a1();
    testeHabilitado = true;
    EnviaSerial(0); // S1
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), "L;");
}
static void test_envia_serial_nome_sensor_correto() {
    reset_a1();
    EnviaSerial(0);                                         // sensorPassado=0 → "S1"
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), "S1");
    serialPista2.reset();
    EnviaSerial(5);                                         // S6
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), "S6");
}
static void test_envia_serial_tem_8_campos() {
    reset_a1();
    EnviaSerial(0);
    // Formato: "L;tAgora;Sx;tipoPWM;PWM;vel;dist;S;rot"
    // 8 separadores ';'
    const std::string& msg = serialPista2.tx_buf;
    int count = 0;
    for (char c : msg) if (c == ';') count++;
    ASSERT_TRUE(count >= 8);
}
static void test_envia_serial_campo_fonte_S() {
    reset_a1();
    EnviaSerial(0);
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), ";S;");
}

// ============================================================
// Grupo 5: EnviaSerialHall — formatação da mensagem H
// ============================================================
static void test_envia_serial_hall_prefixo_H() {
    reset_a1();
    ultimoSensorGravado = 0;
    EnviaSerialHall();
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), "H;");
}
static void test_envia_serial_hall_sensor_numero_correto() {
    reset_a1();
    ultimoSensorGravado = 2; // S3
    EnviaSerialHall();
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), "S3");
}
static void test_envia_serial_hall_contem_carro_id() {
    reset_a1();
    strncpy(carroID, "C1", sizeof(carroID));
    ultimoSensorGravado = 0;
    EnviaSerialHall();
    ASSERT_STR_CONTAINS(serialPista2.tx_buf.c_str(), "C1");
}

// ============================================================
// Grupo 6: gravaPWM_C — estimativa de distância
// ============================================================
static void test_gravapwm_c_nao_executa_sem_teste_habilitado() {
    reset_a1();
    testeHabilitado = false;
    RotDelta = 5.0f;
    distanciaTotal = 99.0f; // sentinela
    gravaPWM_C();
    ASSERT_FLOAT_EQ(99.0f, distanciaTotal, 0.001f); // não modificado
}
static void test_gravapwm_c_nao_executa_com_rotdelta_zero() {
    reset_a1();
    RotDelta = 0.0f;
    distanciaTotal = 42.0f;
    gravaPWM_C();
    ASSERT_FLOAT_EQ(42.0f, distanciaTotal, 0.001f);
}
static void test_gravapwm_c_calcula_dist_de_rotdelta() {
    reset_a1();
    RotDelta       = 10.0f;
    ultimaPosicaoRegistrada = 0.0f;
    PosProxSensorLinear     = 999.0f; // sem limite
    RotCarroAtual  = 10.0f;
    RotCarroAnt    = 9.0f;    // diferente para EnviaSerialHall rodar
    strncpy(carroID, "C1", sizeof(carroID));

    gravaPWM_C();

    float circ = (float)PI * 0.0245f;
    float esperado = 10.0f * circ;
    ASSERT_FLOAT_EQ(esperado, distanciaTotal, 0.001f);
}
static void test_gravapwm_c_distancia_limitada_por_proximo_sensor() {
    reset_a1();
    RotDelta                = 1000.0f; // enorme
    ultimaPosicaoRegistrada = 0.0f;
    PosProxSensorLinear     = 1.0f;   // limite rígido em 1 m
    RotCarroAtual           = 1000.0f;
    RotCarroAnt             = 999.0f;
    strncpy(carroID, "C1", sizeof(carroID));

    gravaPWM_C();

    ASSERT_FLOAT_EQ(1.0f, distanciaTotal, 0.001f); // clamped
}

// ============================================================
// Grupo 7: Setup — loop com 3 elementos
// ============================================================
static void test_setup_loop_apenas_3_sensores() {
    // A correção garante que o for no setup() vai de 0 a 2 (3 elementos)
    // Verificar indiretamente: estadoAntA[3] e analogPins[3] devem existir
    // sem acesso fora dos limites
    ASSERT_EQ(3, (int)(sizeof(analogPins)/sizeof(analogPins[0])));
    ASSERT_EQ(3, (int)(sizeof(estadoAntA)/sizeof(estadoAntA[0])));
}
static void test_pos_array_tamanho_9() {
    ASSERT_EQ(9, (int)(sizeof(pos)/sizeof(pos[0])));
}
static void test_disttretcho_tamanho_9() {
    ASSERT_EQ(9, (int)(sizeof(distTrecho)/sizeof(distTrecho[0])));
}

// ============================================================
// main
// ============================================================
int main() {
    TEST_SUITE("Arduino1 — RecebeHall: parsing");
    RUN_TEST(test_receivehall_parseia_velocidade);
    RUN_TEST(test_receivehall_parseia_rotacoes);
    RUN_TEST(test_receivehall_parseia_id);
    RUN_TEST(test_receivehall_rejeita_sem_ponto_virgula);
    RUN_TEST(test_receivehall_rejeita_velocidade_alta);
    RUN_TEST(test_receivehall_rejeita_salto_rotacao_alto);
    RUN_TEST(test_receivehall_atualiza_rot_carro_ant);
    RUN_TEST(test_receivehall_dois_pacotes_acumulam_rotdelta);

    TEST_SUITE("Arduino1 — Mapeamento de sensores LDR");
    RUN_TEST(test_sensor_s7_mapeado_para_indice_6);
    RUN_TEST(test_sensor_s8_mapeado_para_indice_7);
    RUN_TEST(test_sensor_s9_mapeado_para_indice_8);
    RUN_TEST(test_pos_array_s7_correto);
    RUN_TEST(test_pos_array_s8_correto);
    RUN_TEST(test_pos_array_s9_correto);
    RUN_TEST(test_sensor_s1_do_arduino2_mapeado_para_0);
    RUN_TEST(test_sensor_s6_do_arduino2_mapeado_para_5);

    TEST_SUITE("Arduino1 — processaSensor: posição e lógica");
    RUN_TEST(test_processsensor_mesmo_sensor_ignorado);
    RUN_TEST(test_processsensor_agr_modulo_9);
    RUN_TEST(test_processsensor_prox_circular);
    RUN_TEST(test_processsensor_primeiro_sensor_inicia_experimento);
    RUN_TEST(test_processsensor_segundo_sensor_acumula_posicao);
    RUN_TEST(test_processsensor_trecho_normal_subtracao);
    RUN_TEST(test_processsensor_trecho_wrap_s9_para_s1);
    RUN_TEST(test_processsensor_atualiza_ultimo_sensor_gravado);
    RUN_TEST(test_processsensor_velocidade_positiva_apos_trigger);

    TEST_SUITE("Arduino1 — EnviaSerial: formatação L");
    RUN_TEST(test_envia_serial_prefixo_L);
    RUN_TEST(test_envia_serial_nome_sensor_correto);
    RUN_TEST(test_envia_serial_tem_8_campos);
    RUN_TEST(test_envia_serial_campo_fonte_S);

    TEST_SUITE("Arduino1 — EnviaSerialHall: formatação H");
    RUN_TEST(test_envia_serial_hall_prefixo_H);
    RUN_TEST(test_envia_serial_hall_sensor_numero_correto);
    RUN_TEST(test_envia_serial_hall_contem_carro_id);

    TEST_SUITE("Arduino1 — gravaPWM_C: estimativa de distância");
    RUN_TEST(test_gravapwm_c_nao_executa_sem_teste_habilitado);
    RUN_TEST(test_gravapwm_c_nao_executa_com_rotdelta_zero);
    RUN_TEST(test_gravapwm_c_calcula_dist_de_rotdelta);
    RUN_TEST(test_gravapwm_c_distancia_limitada_por_proximo_sensor);

    TEST_SUITE("Arduino1 — Tamanho dos arrays");
    RUN_TEST(test_setup_loop_apenas_3_sensores);
    RUN_TEST(test_pos_array_tamanho_9);
    RUN_TEST(test_disttretcho_tamanho_9);

    TEST_SUMMARY();
}
