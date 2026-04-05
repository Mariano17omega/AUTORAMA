#pragma once
#include "Arduino.h"

// Classe base mínima — Adafruit_SSD1306 herda dela
class Adafruit_GFX {
public:
    Adafruit_GFX(int w, int h) {}
    virtual void setTextSize(int)  {}
    virtual void setTextColor(int) {}
    virtual void setCursor(int, int) {}
    virtual void print(const char*)  {}
    virtual void print(const String&) {}
    virtual void println(const char* = "") {}
    virtual void println(const String&)    {}
    virtual ~Adafruit_GFX() {}
};
