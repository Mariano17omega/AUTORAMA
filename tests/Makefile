# ============================================================
# Makefile — Testes do AUTORAMA
# Uso:
#   cd tests && make          # compila e executa todos
#   cd tests && make build    # só compila
#   cd tests && make clean    # remove binários
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wno-unused-parameter \
            -Imocks -I.

# Cada sketch tem seu próprio executável de teste
TARGETS := test_carrinho test_arduino1 test_arduino2 test_controle

.PHONY: all build clean

all: build run

build: $(TARGETS)

# ---------- Carrinho ----------
test_carrinho: test_carrinho.cpp framework.h \
               mocks/Arduino.h mocks/avr/interrupt.h \
               mocks/SPI.h mocks/nRF24L01.h mocks/RF24.h \
               mocks/Wire.h mocks/VL53L0X.h
	$(CXX) $(CXXFLAGS) -o $@ $<

# ---------- Arduino 1 ----------
# Nome com espaço requer escape
A1_SRC := "../Arduino_1- PISTA DO AUTORAMA.ino"

test_arduino1: test_arduino1.cpp framework.h \
               mocks/Arduino.h mocks/SPI.h mocks/nRF24L01.h \
               mocks/RF24.h mocks/SoftwareSerial.h mocks/Servo.h
	$(CXX) $(CXXFLAGS) -o $@ $<

# ---------- Arduino 2 ----------
test_arduino2: test_arduino2.cpp framework.h \
               mocks/Arduino.h mocks/SoftwareSerial.h mocks/SD.h
	$(CXX) $(CXXFLAGS) -o $@ $<

# ---------- Controle Remoto ----------
test_controle: test_controle.cpp framework.h \
               mocks/Arduino.h mocks/SPI.h mocks/nRF24L01.h \
               mocks/RF24.h mocks/Wire.h mocks/Keypad.h \
               mocks/Adafruit_GFX.h mocks/Adafruit_SSD1306.h
	$(CXX) $(CXXFLAGS) -o $@ $<

# ---------- Execução ----------
run: $(TARGETS)
	@echo ""
	@echo "=============================================="
	@echo "  Executando todos os testes AUTORAMA"
	@echo "=============================================="
	@total_pass=0; total_fail=0; \
	for t in $(TARGETS); do \
	    echo ""; \
	    ./$$t; \
	    code=$$?; \
	    if [ $$code -ne 0 ]; then total_fail=$$((total_fail+1)); \
	    else total_pass=$$((total_pass+1)); fi; \
	done; \
	echo ""; \
	echo "=============================================="; \
	echo "  Suites: $$total_pass OK, $$total_fail FALHA"; \
	echo "=============================================="; \
	exit $$total_fail

clean:
	rm -f $(TARGETS)
