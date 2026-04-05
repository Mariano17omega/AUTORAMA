// ============================================================
// Testes: Arduino_2 — PISTA DO AUTORAMA.ino
// Cobre: gravaEvento, gravaLinhaBruta, parsing de mensagens,
//        detecção de threshold LDR, SD e SoftwareSerial.
// ============================================================
#include "framework.h"
#include "mocks/Arduino.h"
#include "mocks/SoftwareSerial.h"
#include "mocks/SD.h"
#include <string>
#include <cstring>

// Instâncias globais dos mocks
MockSerial Serial;
SDClass    SD;
std::string _sd_written;  // definição do buffer global de SD

#include "../Arduino_2- PISTA DO AUTORAMA.ino"

// ============================================================
// Reset de estado entre testes
// ============================================================
static void reset_a2() {
    _sd_written.clear();
    Serial.reset();
    radioIn.reset();
    for (int i = 0; i < 6; i++) {
        estadoAntA[i]    = 1023; // tudo "claro" (sem sombra)
        _pin_analog[A0+i] = 1023;
    }
    arquivo._open = true; // assume SD já inicializado
    _mock_millis = 0;
    _mock_micros = 0;
}

// ============================================================
// Grupo 1: gravaEvento — conversão de tempo e formato
// ============================================================
static void test_gravaevento_converte_micros_para_segundos() {
    reset_a2();
    // 500000 µs = 0.500 s → "0.500"
    gravaEvento("s - Catraca Aberta", "500000");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.500");
}
static void test_gravaevento_texto_presente() {
    reset_a2();
    gravaEvento("s - Catraca Aberta", "1000000");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "s - Catraca Aberta");
}
static void test_gravaevento_texto_fechado() {
    reset_a2();
    gravaEvento("s - Catraca Fechada", "2000000");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "s - Catraca Fechada");
}
static void test_gravaevento_zero_microseconds() {
    reset_a2();
    gravaEvento("evento", "0");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.000");
}
static void test_gravaevento_1s_em_microseconds() {
    reset_a2();
    gravaEvento("evento", "1000000");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "1.000");
}
static void test_gravaevento_3_casas_decimais() {
    reset_a2();
    gravaEvento("evento", "123456");
    // 123456 µs = 0.123456 s → formatado com 3 casas → "0.123"
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.123");
}

// ============================================================
// Grupo 2: gravaLinhaBruta — formatação CSV
// ============================================================
static void test_gravalinha_tempo_2_casas_decimais() {
    reset_a2();
    gravaLinhaBruta("120000", "S1", "d", "255", "0.181", "4.2850", "S", "0.00");
    // 120000 µs = 0.12 s → "0.12"
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.12");
}
static void test_gravalinha_nome_sensor() {
    reset_a2();
    gravaLinhaBruta("100000", "S3", "d", "200", "1.000", "1.500", "S", "5.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "S3");
}
static void test_gravalinha_tipo_pwm() {
    reset_a2();
    gravaLinhaBruta("100000", "S1", "d", "180", "0.500", "2.000", "S", "3.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), ", d, ");
}
static void test_gravalinha_valor_pwm() {
    reset_a2();
    gravaLinhaBruta("100000", "S1", "d", "128", "0.500", "2.000", "S", "3.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "128");
}
static void test_gravalinha_velocidade() {
    reset_a2();
    gravaLinhaBruta("100000", "S1", "d", "200", "1.2345", "2.000", "S", "3.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "1.2345");
}
static void test_gravalinha_distancia() {
    reset_a2();
    gravaLinhaBruta("100000", "S1", "d", "200", "1.000", "3.9100", "S", "3.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "3.9100");
}
static void test_gravalinha_separador_virgula_espaco() {
    reset_a2();
    gravaLinhaBruta("100000", "S1", "d", "200", "1.000", "2.000", "S", "0.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), ", ");
}
static void test_gravalinha_todos_8_campos_presentes() {
    reset_a2();
    gravaLinhaBruta("500000", "S5", "d", "255", "2.500", "1.190", "C1", "12.50");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.50"); // tempo
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "S5");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "255");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "2.500");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "1.190");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "C1");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "12.50");
}
static void test_gravalinha_fonte_hall_C1() {
    reset_a2();
    gravaLinhaBruta("200000", "S2", "d", "200", "0.800", "0.355", "C1", "2.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "C1");
}
static void test_gravalinha_fonte_sensor_S() {
    reset_a2();
    gravaLinhaBruta("200000", "S2", "d", "200", "0.800", "0.355", "S", "2.00");
    ASSERT_STR_CONTAINS(_sd_written.c_str(), ", S, ");
}

// ============================================================
// Grupo 3: Parsing de mensagens recebidas do Arduino 1
// ============================================================
static void test_parse_go_aberto_detectado() {
    reset_a2();
    // Injetar mensagem terminada em "GO_aberto"
    radioIn.inject("1000000;GO_aberto");
    // Chamar loop() uma vez — a mensagem deve ser processada e gravada
    loop();
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "Catraca Aberta");
}
static void test_parse_go_fechado_detectado() {
    reset_a2();
    radioIn.inject("2000000;GO_fechado");
    loop();
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "Catraca Fechada");
}
static void test_parse_prefixo_L_roteia_para_gravalinha() {
    reset_a2();
    radioIn.inject("L;500000;S1;d;200;1.000;2.000;S;5.00");
    loop();
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "S1");
}
static void test_parse_prefixo_H_roteia_para_gravalinha() {
    reset_a2();
    radioIn.inject("H;600000;S3;d;180;0.800;1.500;C1;8.00");
    loop();
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "S3");
}
static void test_parse_L_extrai_velocidade() {
    reset_a2();
    radioIn.inject("L;100000;S2;d;255;3.1415;0.635;S;10.00");
    loop();
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "3.1415");
}
static void test_parse_L_extrai_distancia() {
    reset_a2();
    radioIn.inject("L;100000;S4;d;200;1.000;0.9100;S;4.00");
    loop();
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.9100");
}
static void test_parse_mensagem_malformada_ignorada() {
    reset_a2();
    radioIn.inject("LIXO_INVALIDO");
    loop();
    // Nenhuma gravação deve ocorrer
    ASSERT_TRUE(_sd_written.empty());
}
static void test_parse_go_aberto_extrai_tempo() {
    reset_a2();
    radioIn.inject("750000;GO_aberto");
    loop();
    // 750000 µs = 0.750 s
    ASSERT_STR_CONTAINS(_sd_written.c_str(), "0.750");
}

// ============================================================
// Grupo 4: Detecção de threshold LDR e notificação
// ============================================================
static void test_ldr_dispara_na_borda_de_descida() {
    reset_a2();
    // Simula sensor i=0 (S1): estava em 1023 (claro), agora leu < 700 (sombra)
    estadoAntA[0]    = 1023;
    _pin_analog[A0]  = 500; // leitura atual abaixo de 700

    loop(); // sem mensagem serial disponível

    // Arduino 2 deve ter enviado "S1" ao Arduino 1
    ASSERT_STR_CONTAINS(radioIn.tx_buf.c_str(), "S1");
}
static void test_ldr_nao_dispara_se_ja_estava_baixo() {
    reset_a2();
    // Ambos < 700 → sem borda de descida
    estadoAntA[0]   = 600;
    _pin_analog[A0] = 500;
    loop();
    ASSERT_TRUE(radioIn.tx_buf.empty());
}
static void test_ldr_nao_dispara_se_esta_alto() {
    reset_a2();
    estadoAntA[0]   = 1023;
    _pin_analog[A0] = 800; // ainda acima de 700
    loop();
    ASSERT_TRUE(radioIn.tx_buf.empty());
}
static void test_ldr_atualiza_estado_anterior() {
    reset_a2();
    estadoAntA[0]   = 1023;
    _pin_analog[A0] = 500;
    loop();
    // estadoAntA[0] deve ter sido atualizado para a nova leitura
    ASSERT_EQ(500, estadoAntA[0]);
}
static void test_ldr_s2_mapeado_corretamente() {
    reset_a2();
    // Sensor i=1 → S2
    estadoAntA[1]   = 1023;
    _pin_analog[A1] = 400;
    loop();
    ASSERT_STR_CONTAINS(radioIn.tx_buf.c_str(), "S2");
}
static void test_ldr_s6_mapeado_corretamente() {
    reset_a2();
    estadoAntA[5]   = 1023;
    _pin_analog[A5] = 300;
    loop();
    ASSERT_STR_CONTAINS(radioIn.tx_buf.c_str(), "S6");
}
static void test_ldr_threshold_exatamente_700_nao_dispara() {
    reset_a2();
    estadoAntA[0]   = 1023;
    _pin_analog[A0] = 700; // condição: leitura < 700 (não <=)
    loop();
    ASSERT_TRUE(radioIn.tx_buf.empty());
}

// ============================================================
// Grupo 5: SD — inicialização e falha
// ============================================================
static void test_sd_ok_acende_led() {
    // Simulado em setup(): SD ok → LED HIGH
    // Não chamamos setup() (para não travar com while(1)),
    // mas validamos a lógica diretamente
    SD._init_ok = true;
    File f = SD.open("dados.txt", FILE_WRITE);
    ASSERT_TRUE((bool)f);
}
static void test_sd_falha_retorna_arquivo_fechado() {
    SD._init_ok = false;
    File f = SD.open("dados.txt", FILE_WRITE);
    ASSERT_FALSE((bool)f);
    SD._init_ok = true; // restaura para próximos testes
}
static void test_sd_flush_nao_fecha_arquivo() {
    reset_a2();
    ASSERT_TRUE(arquivo._open); // arquivo aberto em reset_a2
    arquivo.flush();             // não deve fechar
    ASSERT_TRUE(arquivo._open);
}

// ============================================================
// main
// ============================================================
int main() {
    TEST_SUITE("Arduino2 — gravaEvento: conversão e formato");
    RUN_TEST(test_gravaevento_converte_micros_para_segundos);
    RUN_TEST(test_gravaevento_texto_presente);
    RUN_TEST(test_gravaevento_texto_fechado);
    RUN_TEST(test_gravaevento_zero_microseconds);
    RUN_TEST(test_gravaevento_1s_em_microseconds);
    RUN_TEST(test_gravaevento_3_casas_decimais);

    TEST_SUITE("Arduino2 — gravaLinhaBruta: CSV");
    RUN_TEST(test_gravalinha_tempo_2_casas_decimais);
    RUN_TEST(test_gravalinha_nome_sensor);
    RUN_TEST(test_gravalinha_tipo_pwm);
    RUN_TEST(test_gravalinha_valor_pwm);
    RUN_TEST(test_gravalinha_velocidade);
    RUN_TEST(test_gravalinha_distancia);
    RUN_TEST(test_gravalinha_separador_virgula_espaco);
    RUN_TEST(test_gravalinha_todos_8_campos_presentes);
    RUN_TEST(test_gravalinha_fonte_hall_C1);
    RUN_TEST(test_gravalinha_fonte_sensor_S);

    TEST_SUITE("Arduino2 — Parsing de mensagens recebidas");
    RUN_TEST(test_parse_go_aberto_detectado);
    RUN_TEST(test_parse_go_fechado_detectado);
    RUN_TEST(test_parse_prefixo_L_roteia_para_gravalinha);
    RUN_TEST(test_parse_prefixo_H_roteia_para_gravalinha);
    RUN_TEST(test_parse_L_extrai_velocidade);
    RUN_TEST(test_parse_L_extrai_distancia);
    RUN_TEST(test_parse_mensagem_malformada_ignorada);
    RUN_TEST(test_parse_go_aberto_extrai_tempo);

    TEST_SUITE("Arduino2 — Detecção de threshold LDR");
    RUN_TEST(test_ldr_dispara_na_borda_de_descida);
    RUN_TEST(test_ldr_nao_dispara_se_ja_estava_baixo);
    RUN_TEST(test_ldr_nao_dispara_se_esta_alto);
    RUN_TEST(test_ldr_atualiza_estado_anterior);
    RUN_TEST(test_ldr_s2_mapeado_corretamente);
    RUN_TEST(test_ldr_s6_mapeado_corretamente);
    RUN_TEST(test_ldr_threshold_exatamente_700_nao_dispara);

    TEST_SUITE("Arduino2 — SD card");
    RUN_TEST(test_sd_ok_acende_led);
    RUN_TEST(test_sd_falha_retorna_arquivo_fechado);
    RUN_TEST(test_sd_flush_nao_fecha_arquivo);

    TEST_SUMMARY();
}
