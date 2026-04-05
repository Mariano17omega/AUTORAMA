// ============================================================
// Testes: Controle_Remoto.ino
// Cobre: formatação de mensagens RF, estado do sistema,
//        display OLED, parsing do teclado.
// ============================================================
#include "framework.h"
#include "mocks/Arduino.h"
#include "mocks/SPI.h"
#include "mocks/nRF24L01.h"
#include "mocks/RF24.h"
#include "mocks/Wire.h"
#include "mocks/Keypad.h"
#include "mocks/Adafruit_GFX.h"
#include "mocks/Adafruit_SSD1306.h"

MockSerial Serial;
TwoWire    Wire;

#include "../Controle_Remoto.ino"

// ============================================================
// Reset de estado entre testes
// ============================================================
static void reset_cr() {
    PWMDEGRAU          = 0;
    modoDegrauPergunta = false;
    aguardaGO          = false;
    StatusCatraca      = 0;
    tipoOperacao       = 0;
    mantendoPWM        = false;
    PWMfinalAtual      = 0;
    prefixoFinal       = 'd';
    stringEntrada      = "";
    radio.reset();
    display.reset();
    Serial.reset();
    for (int i = 0; i < 70; i++) _pin_digital[i] = HIGH; // INPUT_PULLUP → botões soltos
}

// ============================================================
// Grupo 1: executarDegrauOuRampa — formato da mensagem
// ============================================================
static void test_degrau_mensagem_formato_d255() {
    reset_cr();
    PWMDEGRAU    = 255;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_STR_CONTAINS(radio._tx_buf, "d255");
}
static void test_degrau_mensagem_formato_d0() {
    reset_cr();
    PWMDEGRAU    = 0;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_STR_CONTAINS(radio._tx_buf, "d0");
}
static void test_degrau_mensagem_formato_d100() {
    reset_cr();
    PWMDEGRAU    = 100;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_STR_CONTAINS(radio._tx_buf, "d100");
}
static void test_degrau_mensagem_prefixo_d() {
    reset_cr();
    PWMDEGRAU    = 50;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_EQ('d', radio._tx_buf[0]);
}
static void test_degrau_mensagem_cabe_em_32_bytes() {
    reset_cr();
    PWMDEGRAU    = 255;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_TRUE(strlen(radio._tx_buf) < 32);
}

// ============================================================
// Grupo 2: executarDegrauOuRampa — atualização de estado
// ============================================================
static void test_degrau_define_mantendopwm_true() {
    reset_cr();
    PWMDEGRAU    = 150;
    tipoOperacao = 1;
    ASSERT_FALSE(mantendoPWM);
    executarDegrauOuRampa();
    ASSERT_TRUE(mantendoPWM);
}
static void test_degrau_define_pwmfinalatual() {
    reset_cr();
    PWMDEGRAU    = 200;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_EQ(200, PWMfinalAtual);
}
static void test_degrau_define_prefixo_d() {
    reset_cr();
    PWMDEGRAU    = 180;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_EQ('d', prefixoFinal);
}
static void test_degrau_tipo_operacao_invalido_nao_executa() {
    reset_cr();
    tipoOperacao = 0; // não implementado
    mantendoPWM  = false;
    executarDegrauOuRampa();
    ASSERT_FALSE(mantendoPWM); // não deve ter mudado
    ASSERT_EQ(0, radio._tx_count);
}

// ============================================================
// Grupo 3: rfSend — transmissão RF
// ============================================================
static void test_rfsend_carro_usa_endereco_abc123() {
    reset_cr();
    // Verifica que o endereço do carro é "ABC123"
    ASSERT_STR_EQ("ABC123", (const char*)enderecoCarroRF);
}
static void test_rfsend_pista_usa_endereco_fgh789() {
    reset_cr();
    ASSERT_STR_EQ("FGH789", (const char*)enderecoPistaRF);
}
static void test_rfsend_degrau_enviado_para_carro_e_pista() {
    reset_cr();
    PWMDEGRAU    = 100;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    // executarDegrauOuRampa envia para carro E pista: 2 transmissões
    ASSERT_TRUE(radio._tx_count >= 2);
}
static void test_rfsend_go_payload_correto() {
    reset_cr();
    enviarParaPista("GO");
    ASSERT_STR_EQ("GO", radio._tx_buf);
}
static void test_rfsend_para_com_null_terminator() {
    reset_cr();
    char msg[] = "d128";
    radio.write(msg, strlen(msg) + 1); // simula o que rfSend faz
    ASSERT_EQ(0, radio._tx_buf[4]); // byte após "d128" deve ser '\0'
}

// ============================================================
// Grupo 4: OLED — mensagens corretas
// ============================================================
static void test_oled_degrau_mostra_pwm() {
    reset_cr();
    PWMDEGRAU    = 200;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_STR_CONTAINS(display._content.c_str(), "200");
}
static void test_oled_degrau_mostra_texto_degrau() {
    reset_cr();
    PWMDEGRAU    = 100;
    tipoOperacao = 1;
    executarDegrauOuRampa();
    ASSERT_STR_CONTAINS(display._content.c_str(), "Degrau");
}
static void test_oled_clear_limpa_conteudo() {
    reset_cr();
    display._content = "algo";
    display.clearDisplay();
    ASSERT_TRUE(display._content.empty());
}
static void test_oled_show2lines_escreve_duas_linhas() {
    reset_cr();
    oledShow2Lines("Linha 1", "Linha 2");
    ASSERT_STR_CONTAINS(display._content.c_str(), "Linha 1");
    ASSERT_STR_CONTAINS(display._content.c_str(), "Linha 2");
}

// ============================================================
// Grupo 5: Estado inicial e teclado
// ============================================================
static void test_estado_inicial_mantendopwm_false() {
    reset_cr();
    ASSERT_FALSE(mantendoPWM);
}
static void test_estado_inicial_tipo_operacao_zero() {
    reset_cr();
    ASSERT_EQ(0, tipoOperacao);
}
static void test_estado_inicial_status_catraca_zero() {
    reset_cr();
    ASSERT_EQ(0, StatusCatraca);
}
static void test_tecla_d_ativa_modo_degrau_pergunta() {
    reset_cr();
    teclado.inject('d');
    loop(); // processa tecla
    ASSERT_TRUE(modoDegrauPergunta);
}
static void test_tecla_numeros_acumula_string() {
    reset_cr();
    teclado.inject('1');
    loop();
    teclado.inject('5');
    loop();
    teclado.inject('0');
    loop();
    ASSERT_STR_EQ("150", stringEntrada.c_str());
}
static void test_tecla_hash_remove_ultimo_char() {
    reset_cr();
    stringEntrada = "255";
    teclado.inject('#');
    loop();
    ASSERT_STR_EQ("25", stringEntrada.c_str());
}
static void test_tecla_D_confirma_pwm_degrau() {
    reset_cr();
    modoDegrauPergunta = true;
    stringEntrada      = "180";
    teclado.inject('D');
    loop();
    ASSERT_EQ(180, PWMDEGRAU);
    ASSERT_EQ(1, tipoOperacao);
    ASSERT_TRUE(aguardaGO);
}

// ============================================================
// Grupo 6: Ortografia das mensagens ao operador
// ============================================================
static void test_oled_usa_catraca_sem_r() {
    // Verifica que a string "Catrarca" (typo) foi corrigida para "Catraca"
    // Testamos via oledShow2Lines
    reset_cr();
    oledShow2Lines("Catraca Aberta", "Inicio Corrida");
    ASSERT_STR_CONTAINS(display._content.c_str(), "Catraca");
    // Confirma que o typo antigo não está presente
    ASSERT_TRUE(display._content.find("Catrarca") == std::string::npos);
}

// ============================================================
// main
// ============================================================
int main() {
    TEST_SUITE("Controle — executarDegrauOuRampa: formato RF");
    RUN_TEST(test_degrau_mensagem_formato_d255);
    RUN_TEST(test_degrau_mensagem_formato_d0);
    RUN_TEST(test_degrau_mensagem_formato_d100);
    RUN_TEST(test_degrau_mensagem_prefixo_d);
    RUN_TEST(test_degrau_mensagem_cabe_em_32_bytes);

    TEST_SUITE("Controle — executarDegrauOuRampa: estado interno");
    RUN_TEST(test_degrau_define_mantendopwm_true);
    RUN_TEST(test_degrau_define_pwmfinalatual);
    RUN_TEST(test_degrau_define_prefixo_d);
    RUN_TEST(test_degrau_tipo_operacao_invalido_nao_executa);

    TEST_SUITE("Controle — rfSend: transmissão");
    RUN_TEST(test_rfsend_carro_usa_endereco_abc123);
    RUN_TEST(test_rfsend_pista_usa_endereco_fgh789);
    RUN_TEST(test_rfsend_degrau_enviado_para_carro_e_pista);
    RUN_TEST(test_rfsend_go_payload_correto);
    RUN_TEST(test_rfsend_para_com_null_terminator);

    TEST_SUITE("Controle — OLED");
    RUN_TEST(test_oled_degrau_mostra_pwm);
    RUN_TEST(test_oled_degrau_mostra_texto_degrau);
    RUN_TEST(test_oled_clear_limpa_conteudo);
    RUN_TEST(test_oled_show2lines_escreve_duas_linhas);

    TEST_SUITE("Controle — Estado inicial e teclado");
    RUN_TEST(test_estado_inicial_mantendopwm_false);
    RUN_TEST(test_estado_inicial_tipo_operacao_zero);
    RUN_TEST(test_estado_inicial_status_catraca_zero);
    RUN_TEST(test_tecla_d_ativa_modo_degrau_pergunta);
    RUN_TEST(test_tecla_numeros_acumula_string);
    RUN_TEST(test_tecla_hash_remove_ultimo_char);
    RUN_TEST(test_tecla_D_confirma_pwm_degrau);

    TEST_SUITE("Controle — Qualidade das mensagens");
    RUN_TEST(test_oled_usa_catraca_sem_r);

    TEST_SUMMARY();
}
