#pragma once
#include "Adafruit_GFX.h"
#include "Wire.h"
#include <string>

#define SSD1306_SWITCHCAPVCC 1
#define SSD1306_WHITE        1
#define SSD1306_BLACK        0

// ============================================================
// Mock do display OLED SSD1306
// ============================================================
class Adafruit_SSD1306 : public Adafruit_GFX {
public:
    std::string _content;   // tudo que foi enviado ao display

    Adafruit_SSD1306(int w, int h, TwoWire*, int rst = -1)
        : Adafruit_GFX(w, h) {}

    bool begin(int vcc, int addr) { return true; }
    void setRotation(int)         {}
    void clearDisplay()           { _content.clear(); }
    void display()                {}

    void setTextSize(int) override  {}
    void setTextColor(int) override {}
    void setCursor(int, int) override {}

    void print(const char* s) override    { _content += s; }
    void print(const String& s) override  { _content += s.c_str(); }
    void println(const char* s = "") override  { _content += s; _content += '\n'; }
    void println(const String& s) override     { _content += s.c_str(); _content += '\n'; }

    void reset() { _content.clear(); }
};
