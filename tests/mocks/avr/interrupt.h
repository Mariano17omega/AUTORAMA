#pragma once
// Mock de avr/interrupt.h para compilação em host
// ISR(VECTOR) vira uma função C++ comum, chamável diretamente nos testes

#define ISR(vector) void ISR_##vector(void)

// Registros de interrupt (já definidos em Arduino.h, incluídos antes deste)
// PIND, PCICR, PCMSK2, PCIE2, PD5, PCINT21
