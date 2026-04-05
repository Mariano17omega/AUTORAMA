#pragma once
// ============================================================
// Mock do núcleo Arduino para testes em host (Linux/Mac/Win)
// ============================================================
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>

// ---- Tipos Arduino ----
typedef uint8_t  byte;
typedef uint8_t  boolean;
typedef uint16_t word;

// ---- Constantes ----
#define HIGH 1
#define LOW  0
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2

#ifndef PI
#  define PI 3.14159265358979323846f
#endif

// Pinos analógicos (equivalência Uno: A0=14 … A5=19)
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19

// ---- Registros AVR simulados (usados em Carrinho.ino) ----
static volatile uint8_t PIND   = 0;
static volatile uint8_t PCICR  = 0;
static volatile uint8_t PCMSK2 = 0;
#define PCIE2   2
#define PD5     5
#define PCINT21 21

// ---- Estado mockável de pinos e tempo ----
static int           _pin_digital[70]  = {};   // valores digitais
static int           _pin_analog[70]   = {};   // valores analógicos (indexados por pino absoluto)
static unsigned long _mock_millis = 0;
static unsigned long _mock_micros = 0;

inline void          pinMode(int, int)           {}
inline void          digitalWrite(int pin, int v) { _pin_digital[pin] = v; }
inline int           digitalRead(int pin)         { return _pin_digital[pin]; }
inline void          analogWrite(int pin, int v)  { _pin_digital[pin] = v; }
inline int           analogRead(int pin)          { return _pin_analog[pin]; }
inline unsigned long millis()                     { return _mock_millis; }
inline unsigned long micros()                     { return _mock_micros; }
inline void          delay(unsigned long ms)      { _mock_millis += ms; }
inline void          delayMicroseconds(unsigned long us) { _mock_micros += us; }
inline void          noInterrupts()               {}
inline void          interrupts()                 {}

// memset/memcpy/strcmp/strlen disponíveis via <string.h> ✓

// ---- dtostrf (AVR-only, não existe em POSIX) ----
inline char* dtostrf(float val, int width, int prec, char* buf) {
    if (width <= 0)
        snprintf(buf, 32, "%.*f", prec, (double)val);
    else
        snprintf(buf, 32, "%*.*f", width, prec, (double)val);
    return buf;
}

// ---- constrain / map ----
#ifndef constrain
#  define constrain(x, lo, hi) ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))
#endif
#ifndef map
#  define map(x,il,ih,ol,oh) ((x-il)*(long)(oh-ol)/(ih-il)+ol)
#endif
#ifndef abs
#  define abs(x) ((x)>0?(x):-(x))
#endif

// ============================================================
// Classe String — emula Arduino String sobre std::string
// ============================================================
class String {
    std::string d;
public:
    String() {}
    String(const char* s)   : d(s ? s : "") {}
    String(const std::string& s) : d(s) {}
    explicit String(char c)           { d += c; }
    explicit String(int n)            { char b[32]; snprintf(b,32,"%d",n); d=b; }
    explicit String(unsigned int n)   { char b[32]; snprintf(b,32,"%u",n); d=b; }
    explicit String(long n)           { char b[32]; snprintf(b,32,"%ld",n); d=b; }
    explicit String(unsigned long n)  { char b[32]; snprintf(b,32,"%lu",n); d=b; }
    explicit String(float f, int dec=2)  { char b[32],fmt[16]; snprintf(fmt,16,"%%.%df",dec); snprintf(b,32,fmt,(double)f); d=b; }
    explicit String(double f, int dec=2) { char b[32],fmt[16]; snprintf(fmt,16,"%%.%df",dec); snprintf(b,32,fmt,f); d=b; }

    void trim() {
        size_t s = d.find_first_not_of(" \t\r\n");
        size_t e = d.find_last_not_of(" \t\r\n");
        d = (s == std::string::npos) ? "" : d.substr(s, e-s+1);
    }
    bool startsWith(const char* p) const { return d.find(p) == 0; }
    bool startsWith(const String& p) const { return startsWith(p.c_str()); }
    bool endsWith(const char* s) const {
        size_t sl = strlen(s);
        return d.size() >= sl && d.compare(d.size()-sl, sl, s) == 0;
    }
    bool endsWith(const String& s) const { return endsWith(s.c_str()); }
    int indexOf(char c, int from=0) const {
        size_t p = d.find(c, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(const char* s, int from=0) const {
        size_t p = d.find(s, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    String substring(int start, int end=-1) const {
        if (end < 0) end = (int)d.size();
        if (start < 0) start = 0;
        if (start >= (int)d.size()) return String();
        return String(d.substr(start, end-start));
    }
    int     toInt()   const { return atoi(d.c_str()); }
    float   toFloat() const { return (float)atof(d.c_str()); }
    unsigned int length() const { return (unsigned int)d.size(); }
    void remove(int idx) { d = d.substr(0, idx); }
    const char* c_str() const { return d.c_str(); }
    void toCharArray(char* buf, int sz) const {
        strncpy(buf, d.c_str(), sz-1);
        buf[sz-1] = '\0';
    }

    String& operator+=(const String& r)  { d += r.d; return *this; }
    String& operator+=(const char* r)    { d += r;   return *this; }
    String& operator+=(char c)           { d += c;   return *this; }
    String& operator+=(int n)            { *this += String(n); return *this; }

    String operator+(const String& r) const { return String(d + r.d); }
    String operator+(const char* r)   const { return String(d + r); }
    String operator+(char c)          const { return String(d + c); }

    bool operator==(const String& r) const { return d == r.d; }
    bool operator==(const char* r)   const { return d == r; }
    bool operator!=(const String& r) const { return d != r.d; }
    bool operator!=(const char* r)   const { return d != r; }

    // Permite usar String em contextos que esperam const char*
    operator const char*() const { return d.c_str(); }

    friend String operator+(const char* l, const String& r) {
        return String(std::string(l) + r.d);
    }
};

// ============================================================
// Mock Serial
// ============================================================
class MockSerial {
public:
    std::string output;
    void begin(long) {}
    void print(const char* s) { output += s; }
    void print(const String& s) { output += s.c_str(); }
    void print(int n)           { char b[32]; snprintf(b,32,"%d",n);  output+=b; }
    void print(long n)          { char b[32]; snprintf(b,32,"%ld",n); output+=b; }
    void print(unsigned long n) { char b[32]; snprintf(b,32,"%lu",n); output+=b; }
    void print(float f)         { char b[32]; snprintf(b,32,"%f",(double)f); output+=b; }
    void println(const char* s="") { output += s; output += '\n'; }
    void println(const String& s)  { output += s.c_str(); output += '\n'; }
    void println(int n)            { print(n); output+='\n'; }
    void println(long n)           { print(n); output+='\n'; }
    void println(unsigned long n)  { print(n); output+='\n'; }
    void println(float f)          { print(f); output+='\n'; }
    void flush() {}
    // Limpa saída capturada entre testes
    void reset() { output.clear(); }
};

extern MockSerial Serial;
