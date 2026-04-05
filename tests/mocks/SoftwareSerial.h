#pragma once
#include "Arduino.h"
#include <string>

// ============================================================
// Mock da classe SoftwareSerial
// ============================================================
class SoftwareSerial {
public:
    std::string rx_buf;    // bytes que serão "recebidos" pelo código testado
    std::string tx_buf;    // bytes "transmitidos" pelo código testado
    long        _timeout = 1000;

    SoftwareSerial(int rx, int tx) {}

    void begin(long baud)    {}
    void setTimeout(long ms) { _timeout = ms; }
    void flush()             {}

    bool available() { return !rx_buf.empty(); }

    // Lê até o terminador ou fim do buffer
    String readStringUntil(char term) {
        size_t pos = rx_buf.find(term);
        std::string result;
        if (pos != std::string::npos) {
            result  = rx_buf.substr(0, pos);
            rx_buf  = rx_buf.substr(pos + 1);
        } else {
            result  = rx_buf;
            rx_buf.clear();
        }
        return String(result.c_str());
    }

    int read() {
        if (rx_buf.empty()) return -1;
        int c = (unsigned char)rx_buf[0];
        rx_buf = rx_buf.substr(1);
        return c;
    }

    // println com várias sobrecargas (char*, String, int)
    size_t println(const char* s)    { tx_buf += s; tx_buf += '\n'; return strlen(s)+1; }
    size_t println(char* s)          { tx_buf += s; tx_buf += '\n'; return strlen(s)+1; }
    size_t println(const String& s)  { return println(s.c_str()); }
    size_t println(int n)            { char b[32]; snprintf(b,32,"%d",n); return println(b); }
    size_t print(const char* s)      { tx_buf += s; return strlen(s); }
    size_t print(const String& s)    { return print(s.c_str()); }

    // Injetar mensagem no buffer de recepção
    void inject(const char* msg) { rx_buf += msg; rx_buf += '\n'; }

    void reset() { rx_buf.clear(); tx_buf.clear(); }
};
