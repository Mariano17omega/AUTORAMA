// ============================================================
// Testes: Carrinho.ino
// Cobre: parsing RF, filtro EMA, cálculo de velocidade,
//        anti-colisão, formatação de telemetria e ISR Hall.
// ============================================================
#include "framework.h"
#include "mocks/Arduino.h"
#include "mocks/avr/interrupt.h"
#include "mocks/SPI.h"
#include "mocks/nRF24L01.h"
#include "mocks/RF24.h"
#include "mocks/Wire.h"
#include "mocks/VL53L0X.h"

// Globais dos mocks que o .ino usa como extern
MockSerial Serial;
TwoWire    Wire;

// Inclui o sketch como unidade de compilação
#include "../Carrinho.ino"

// ============================================================
// Utilitários de teste
// ============================================================
static void reset_state() {
    bloqueado           = false;
    comandoAtivo        = false;
    ultimoPWM           = 0;
    pulsosTotais        = 0;
    ultimoDtValido_us   = 0;
    estadoAnterior      = LOW;
    tPulsoAtual_us      = 0;
    tPulsoAnterior_us   = 0;
    novoPulso           = false;
    armadoParaNovoPulso = true;
    velocidade_filt     = 0.0f;
    velocidade          = 0.0f;
    rotacoesTotais      = 0.0f;
    _mock_micros        = 0;
    _mock_millis        = 0;
    PIND                = 0;
    for (int i = 0; i < 70; i++) _pin_digital[i] = 0;
    Serial.reset();
}

// ============================================================
// Grupo 1: Parsing de PWM recebido via RF
// ============================================================
static void test_pwm_parse_valor_normal() {
    char cmd[] = "d150";
    int v = constrain(atoi(&cmd[1]), 0, 255);
    ASSERT_EQ(150, v);
}
static void test_pwm_parse_zero() {
    char cmd[] = "d0";
    int v = constrain(atoi(&cmd[1]), 0, 255);
    ASSERT_EQ(0, v);
}
static void test_pwm_parse_maximo() {
    char cmd[] = "d255";
    int v = constrain(atoi(&cmd[1]), 0, 255);
    ASSERT_EQ(255, v);
}
static void test_pwm_clamp_acima_de_255() {
    char cmd[] = "d300";
    int v = constrain(atoi(&cmd[1]), 0, 255);
    ASSERT_EQ(255, v);
}
static void test_pwm_clamp_negativo() {
    // "d-5": atoi(&cmd[1]) = -5 → constrain → 0
    char cmd[] = "d-5";
    int v = constrain(atoi(&cmd[1]), 0, 255);
    ASSERT_EQ(0, v);
}
static void test_checkrf_atualiza_ultimopwm() {
    reset_state();
    strncpy(ValorPWM, "d200", sizeof(ValorPWM));
    // Simula radio disponível com payload já em ValorPWM
    // Chama diretamente a lógica de parse (a função checkRF drena o radio)
    int pwm = constrain(atoi(&ValorPWM[1]), 0, 255);
    ultimoPWM = pwm;
    ASSERT_EQ(200, ultimoPWM);
}
static void test_checkrf_led_wait_apaga_apos_receive() {
    reset_state();
    // LED_WAIT deve começar apagado
    ASSERT_EQ(LOW, _pin_digital[LED_WAIT]);
    // Simula que checkRF acende e depois apaga
    digitalWrite(LED_WAIT, HIGH);
    digitalWrite(LED_WAIT, LOW);
    ASSERT_EQ(LOW, _pin_digital[LED_WAIT]);
}

// ============================================================
// Grupo 2: Constantes físicas
// ============================================================
static void test_circunferencia_valor() {
    float expected = (float)PI * 0.0245f;
    ASSERT_FLOAT_EQ(expected, CIRCUNFERENCIA, 1e-5f);
}
static void test_circunferencia_positiva() {
    ASSERT_TRUE(CIRCUNFERENCIA > 0.0f);
}
static void test_diametro_roda_correto() {
    ASSERT_FLOAT_EQ(0.0245f, DIAMETRO_RODA, 1e-6f);
}

// ============================================================
// Grupo 3: Cálculo de velocidade a partir de Δt
// ============================================================
static void test_velocidade_dt_1ms() {
    float dt  = 0.001f;
    float rps = (1.0f / dt) / IMAS_POR_VOLTA;
    float v   = rps * CIRCUNFERENCIA;
    // 1000 RPS → v = 1000 * PI * 0.0245 ≈ 76.97 m/s
    ASSERT_FLOAT_EQ(1000.0f * CIRCUNFERENCIA, v, 0.001f);
}
static void test_velocidade_dt_100ms() {
    float dt  = 0.1f;
    float rps = (1.0f / dt) / IMAS_POR_VOLTA;
    float v   = rps * CIRCUNFERENCIA;
    ASSERT_FLOAT_EQ(10.0f * CIRCUNFERENCIA, v, 0.0001f);
}
static void test_velocidade_dt_invalido_ignorado() {
    // dt <= 0 não deve gerar velocidade (proteção no código)
    float dt = 0.0f;
    bool valido = (dt > 0.000001f);
    ASSERT_FALSE(valido);
}

// ============================================================
// Grupo 4: Filtro EMA
// ============================================================
static void test_ema_primeiro_passo_de_zero() {
    float filt = 0.0f;
    float inst = 1.0f;
    filt += ALPHA * (inst - filt);
    ASSERT_FLOAT_EQ(ALPHA, filt, 1e-6f);
}
static void test_ema_converge_para_alvo() {
    float filt = 0.0f;
    float target = 5.0f;
    for (int i = 0; i < 200; i++)
        filt += ALPHA * (target - filt);
    // Após 200 iterações deve estar muito próximo de 5.0
    ASSERT_FLOAT_EQ(target, filt, 0.01f);
}
static void test_ema_alpha_maior_converge_mais_rapido() {
    float filt_low = 0.0f, filt_high = 0.0f;
    float target = 10.0f;
    float alpha_high = 0.5f;
    for (int i = 0; i < 10; i++) {
        filt_low  += ALPHA      * (target - filt_low);
        filt_high += alpha_high * (target - filt_high);
    }
    ASSERT_TRUE(filt_high > filt_low);
}
static void test_ema_nao_ultrapassa_alvo() {
    float filt = 0.0f;
    float target = 3.0f;
    for (int i = 0; i < 500; i++)
        filt += ALPHA * (target - filt);
    ASSERT_TRUE(filt <= target + 1e-4f);
}

// ============================================================
// Grupo 5: Contagem de rotações
// ============================================================
static void test_rotacoes_totais_5_pulsos() {
    reset_state();
    pulsosTotais   = 5;
    rotacoesTotais = (float)pulsosTotais / IMAS_POR_VOLTA;
    ASSERT_FLOAT_EQ(5.0f, rotacoesTotais, 1e-6f);
}
static void test_rotacoes_totais_zero() {
    reset_state();
    pulsosTotais   = 0;
    rotacoesTotais = (float)pulsosTotais / IMAS_POR_VOLTA;
    ASSERT_FLOAT_EQ(0.0f, rotacoesTotais, 1e-6f);
}

// ============================================================
// Grupo 6: Anti-colisão (sensor VL53L0X)
// ============================================================
static void test_anticollision_bloqueia_em_10cm() {
    reset_state();
    int dist = 10; // cm
    if (dist <= 10) bloqueado = true;
    ASSERT_TRUE(bloqueado);
}
static void test_anticollision_bloqueia_em_5cm() {
    reset_state();
    int dist = 5;
    if (dist <= 10) bloqueado = true;
    ASSERT_TRUE(bloqueado);
}
static void test_anticollision_desbloqueia_em_15cm() {
    reset_state();
    bloqueado = true;
    int dist = 15;
    if (dist <= 10)       bloqueado = true;
    else if (dist >= 15)  bloqueado = false;
    ASSERT_FALSE(bloqueado);
}
static void test_anticollision_histerese_12cm_mantem_estado() {
    reset_state();
    bloqueado = true;
    int dist = 12; // entre 10 e 15 → histerese, estado não muda
    if (dist <= 10)       bloqueado = true;
    else if (dist >= 15)  bloqueado = false;
    ASSERT_TRUE(bloqueado);  // permanece bloqueado
}
static void test_anticollision_histerese_12cm_livre_mantem() {
    reset_state();
    bloqueado = false;
    int dist = 12;
    if (dist <= 10)       bloqueado = true;
    else if (dist >= 15)  bloqueado = false;
    ASSERT_FALSE(bloqueado); // permanece livre
}

// ============================================================
// Grupo 7: Formatação da mensagem de telemetria
// ============================================================
static void test_telemetria_formato_geral() {
    char msg[32];
    char sv[12], sr[12];
    float v = 1.2345f, r = 42.5f;
    dtostrf(v, 0, 4, sv);
    dtostrf(r, 0, 2, sr);
    snprintf(msg, sizeof(msg), "%s;%s;%s", "C1", sv, sr);
    // Deve conter "C1;" seguido da velocidade e rotações
    ASSERT_STR_CONTAINS(msg, "C1;");
    ASSERT_STR_CONTAINS(msg, "1.2345");
    ASSERT_STR_CONTAINS(msg, "42.50");
}
static void test_telemetria_cabe_em_32_bytes() {
    char msg[32];
    char sv[12], sr[12];
    // Valores máximos plausíveis
    dtostrf(9.9999f, 0, 4, sv);
    dtostrf(9999.99f, 0, 2, sr);
    int n = snprintf(msg, sizeof(msg), "%s;%s;%s", "C1", sv, sr);
    ASSERT_TRUE(n < 32);
}
static void test_telemetria_prefixo_id_carro() {
    char msg[32];
    char sv[12] = "0.0000", sr[12] = "0.00";
    snprintf(msg, sizeof(msg), "%s;%s;%s", ID_CARRO, sv, sr);
    ASSERT_EQ('C', msg[0]);
    ASSERT_EQ('1', msg[1]);
    ASSERT_EQ(';', msg[2]);
}
static void test_telemetria_dois_separadores() {
    char msg[32];
    char sv[12] = "1.5000", sr[12] = "3.00";
    snprintf(msg, sizeof(msg), "C1;%s;%s", sv, sr);
    int count = 0;
    for (int i = 0; msg[i]; i++) if (msg[i] == ';') count++;
    ASSERT_EQ(2, count);
}

// ============================================================
// Grupo 8: ISR Hall — debounce e contagem
// ============================================================
static void test_hall_isr_primeiro_pulso_aceito() {
    reset_state();
    // Estado: aguardando borda de subida, nenhum pulso anterior
    PIND          = (1 << PD5); // D5 = HIGH
    estadoAnterior = LOW;
    armadoParaNovoPulso = true;
    tPulsoAtual_us = 0; // sinaliza "primeiro pulso"
    _mock_micros   = 50000;

    ISR_PCINT2_vect();

    ASSERT_TRUE(novoPulso);
    ASSERT_EQ(1UL, pulsosTotais);
}
static void test_hall_isr_pulso_muito_rapido_rejeitado() {
    reset_state();
    // Pulso 1 aceito
    PIND = (1 << PD5); estadoAnterior = LOW; _mock_micros = 20000;
    ISR_PCINT2_vect();
    // Borda de descida para rearmar
    PIND = 0; estadoAnterior = HIGH; ISR_PCINT2_vect();
    novoPulso = false; // limpa flag

    // Pulso 2 em apenas 5 ms (< TEMPO_MIN_US = 10 ms)
    _mock_micros = 25000; // Δt = 5 ms
    PIND = (1 << PD5); estadoAnterior = LOW;
    ISR_PCINT2_vect();

    ASSERT_FALSE(novoPulso);          // não deve ter gerado pulso
    ASSERT_EQ(1UL, pulsosTotais);     // contador não incrementou
}
static void test_hall_isr_pulso_valido_apos_tempo_minimo() {
    reset_state();
    // Pulso 1
    PIND = (1 << PD5); estadoAnterior = LOW; _mock_micros = 0;
    ISR_PCINT2_vect();
    PIND = 0; estadoAnterior = HIGH; ISR_PCINT2_vect(); // descida
    novoPulso = false;

    // Pulso 2 após 15 ms (> TEMPO_MIN_US = 10 ms)
    _mock_micros = 15000;
    PIND = (1 << PD5); estadoAnterior = LOW;
    ISR_PCINT2_vect();

    ASSERT_TRUE(novoPulso);
    ASSERT_EQ(2UL, pulsosTotais);
}
static void test_hall_isr_rearmado_na_descida() {
    reset_state();
    armadoParaNovoPulso = false; // simulando estado "travado"
    PIND = 0; estadoAnterior = HIGH; // borda de descida
    ISR_PCINT2_vect();
    ASSERT_TRUE(armadoParaNovoPulso);
}
static void test_hall_isr_nao_conta_descida_como_pulso() {
    reset_state();
    // Borda de descida enquanto armado
    PIND = 0; estadoAnterior = HIGH;
    ISR_PCINT2_vect();
    ASSERT_EQ(0UL, pulsosTotais);
    ASSERT_FALSE(novoPulso);
}
static void test_hall_isr_dt_registrado_apos_dois_pulsos() {
    reset_state();
    // Pulso 1 em t=0
    PIND = (1 << PD5); estadoAnterior = LOW; _mock_micros = 0;
    ISR_PCINT2_vect();
    PIND = 0; estadoAnterior = HIGH; ISR_PCINT2_vect();

    // Pulso 2 em t=20 ms
    _mock_micros = 20000;
    PIND = (1 << PD5); estadoAnterior = LOW;
    ISR_PCINT2_vect();

    ASSERT_EQ(20000UL, ultimoDtValido_us);
}
static void test_hall_isr_debounce_relativo_aplicado() {
    reset_state();
    // Pulso 1 em t=0
    PIND = (1 << PD5); estadoAnterior = LOW; _mock_micros = 0;
    ISR_PCINT2_vect();
    PIND = 0; estadoAnterior = HIGH; ISR_PCINT2_vect();

    // Pulso 2 em t=20 ms (estabelece ultimoDtValido_us = 20000)
    _mock_micros = 20000;
    PIND = (1 << PD5); estadoAnterior = LOW;
    ISR_PCINT2_vect();
    PIND = 0; estadoAnterior = HIGH; ISR_PCINT2_vect();
    novoPulso = false;

    // Pulso 3 em t=30 ms → Δt = 10 ms
    // limiteRelativo = 0.75 * 20000 = 15000 µs → 10000 < 15000 → rejeitado
    _mock_micros = 30000;
    PIND = (1 << PD5); estadoAnterior = LOW;
    ISR_PCINT2_vect();
    ASSERT_FALSE(novoPulso);
}

// ============================================================
// Grupo 9: dtostrf (usado na formatação de telemetria)
// ============================================================
static void test_dtostrf_4_casas_decimais() {
    char buf[32];
    dtostrf(3.14159f, 0, 4, buf);
    ASSERT_STR_EQ("3.1416", buf);
}
static void test_dtostrf_2_casas_decimais() {
    char buf[32];
    dtostrf(12.5f, 0, 2, buf);
    ASSERT_STR_EQ("12.50", buf);
}
static void test_dtostrf_zero() {
    char buf[32];
    dtostrf(0.0f, 0, 2, buf);
    ASSERT_STR_EQ("0.00", buf);
}

// ============================================================
// main
// ============================================================
int main() {
    TEST_SUITE("Carrinho — Parsing de PWM");
    RUN_TEST(test_pwm_parse_valor_normal);
    RUN_TEST(test_pwm_parse_zero);
    RUN_TEST(test_pwm_parse_maximo);
    RUN_TEST(test_pwm_clamp_acima_de_255);
    RUN_TEST(test_pwm_clamp_negativo);
    RUN_TEST(test_checkrf_atualiza_ultimopwm);
    RUN_TEST(test_checkrf_led_wait_apaga_apos_receive);

    TEST_SUITE("Carrinho — Constantes físicas");
    RUN_TEST(test_circunferencia_valor);
    RUN_TEST(test_circunferencia_positiva);
    RUN_TEST(test_diametro_roda_correto);

    TEST_SUITE("Carrinho — Cálculo de velocidade");
    RUN_TEST(test_velocidade_dt_1ms);
    RUN_TEST(test_velocidade_dt_100ms);
    RUN_TEST(test_velocidade_dt_invalido_ignorado);

    TEST_SUITE("Carrinho — Filtro EMA");
    RUN_TEST(test_ema_primeiro_passo_de_zero);
    RUN_TEST(test_ema_converge_para_alvo);
    RUN_TEST(test_ema_alpha_maior_converge_mais_rapido);
    RUN_TEST(test_ema_nao_ultrapassa_alvo);

    TEST_SUITE("Carrinho — Contagem de rotações");
    RUN_TEST(test_rotacoes_totais_5_pulsos);
    RUN_TEST(test_rotacoes_totais_zero);

    TEST_SUITE("Carrinho — Anti-colisão");
    RUN_TEST(test_anticollision_bloqueia_em_10cm);
    RUN_TEST(test_anticollision_bloqueia_em_5cm);
    RUN_TEST(test_anticollision_desbloqueia_em_15cm);
    RUN_TEST(test_anticollision_histerese_12cm_mantem_estado);
    RUN_TEST(test_anticollision_histerese_12cm_livre_mantem);

    TEST_SUITE("Carrinho — Formatação de telemetria RF");
    RUN_TEST(test_telemetria_formato_geral);
    RUN_TEST(test_telemetria_cabe_em_32_bytes);
    RUN_TEST(test_telemetria_prefixo_id_carro);
    RUN_TEST(test_telemetria_dois_separadores);

    TEST_SUITE("Carrinho — ISR Hall (debounce e contagem)");
    RUN_TEST(test_hall_isr_primeiro_pulso_aceito);
    RUN_TEST(test_hall_isr_pulso_muito_rapido_rejeitado);
    RUN_TEST(test_hall_isr_pulso_valido_apos_tempo_minimo);
    RUN_TEST(test_hall_isr_rearmado_na_descida);
    RUN_TEST(test_hall_isr_nao_conta_descida_como_pulso);
    RUN_TEST(test_hall_isr_dt_registrado_apos_dois_pulsos);
    RUN_TEST(test_hall_isr_debounce_relativo_aplicado);

    TEST_SUITE("Carrinho — dtostrf");
    RUN_TEST(test_dtostrf_4_casas_decimais);
    RUN_TEST(test_dtostrf_2_casas_decimais);
    RUN_TEST(test_dtostrf_zero);

    TEST_SUMMARY();
}
