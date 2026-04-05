#pragma once
#include "Arduino.h"
#include <string>

#define FILE_WRITE 1
#define FILE_READ  0

// Buffer global que captura tudo escrito no SD — inspecionado pelos testes
extern std::string _sd_written;

// ============================================================
// Mock da classe File
// ============================================================
class File {
public:
    bool _open = false;

    File() {}

    operator bool() const { return _open; }

    void println(const char* s) {
        if (_open) { _sd_written += s; _sd_written += '\n'; }
    }
    void println(const String& s) { println(s.c_str()); }
    void print(const char* s)     { if (_open) _sd_written += s; }
    void print(const String& s)   { print(s.c_str()); }
    void flush()  {}
    void close()  { _open = false; }
};

// ============================================================
// Mock da classe SDClass
// ============================================================
class SDClass {
public:
    bool _init_ok = true;

    bool begin(int) { return _init_ok; }

    File open(const char*, uint8_t = FILE_WRITE) {
        File f;
        f._open = _init_ok;
        return f;
    }
};

extern SDClass SD;
