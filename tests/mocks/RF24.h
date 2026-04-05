#pragma once
#include "Arduino.h"
#include "nRF24L01.h"
#include <string.h>

// ============================================================
// Mock da classe RF24
// ============================================================
class RF24 {
public:
    bool  _listening      = false;
    bool  _packet_ready   = false;
    char  _rx_buf[33]     = {};   // payload a ser "recebido"
    char  _tx_buf[33]     = {};   // último payload "enviado"
    int   _tx_count       = 0;
    int   _channel        = 0;
    int   _pa_level       = 0;
    int   _data_rate      = 0;
    const byte* _write_addr = nullptr;

    RF24(int ce, int csn) {}

    void begin()                              {}
    void setPALevel(int level)                { _pa_level  = level; }
    void setDataRate(int rate)                { _data_rate = rate; }
    void setChannel(int ch)                   { _channel   = ch; }
    void openReadingPipe(int n, const byte*)  {}
    void openWritingPipe(const byte* addr)    { _write_addr = addr; }
    void startListening()                     { _listening = true; }
    void stopListening()                      { _listening = false; }

    bool available() { return _packet_ready; }

    bool read(void* buf, int len) {
        if (!_packet_ready) return false;
        memcpy(buf, _rx_buf, len < 32 ? len : 32);
        ((char*)buf)[len-1] = '\0';   // garante terminação
        _packet_ready = false;
        return true;
    }

    bool write(const void* buf, int len) {
        int n = (len < 32 ? len : 32);
        memcpy(_tx_buf, buf, n);
        _tx_buf[n] = '\0';
        _tx_count++;
        return true;
    }

    // Helpers para os testes injetarem pacotes
    void inject(const char* msg) {
        strncpy(_rx_buf, msg, 32);
        _rx_buf[32] = '\0';
        _packet_ready = true;
    }
    void reset() {
        _packet_ready = false;
        _tx_count     = 0;
        memset(_rx_buf, 0, sizeof(_rx_buf));
        memset(_tx_buf, 0, sizeof(_tx_buf));
    }
};
