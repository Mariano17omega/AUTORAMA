#pragma once

// ============================================================
// Mock do sensor ToF VL53L0X (anti-colisão do Carrinho)
// ============================================================
class VL53L0X {
public:
    int _mock_mm = 1000; // distância padrão: 1000 mm = 100 cm (longe, sem colisão)

    void init()             {}
    void setTimeout(int)    {}
    void startContinuous()  {}

    // Retorna em milímetros (código divide por 10 para obter cm)
    int readRangeContinuousMillimeters() { return _mock_mm; }
};
