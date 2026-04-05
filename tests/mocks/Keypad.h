#pragma once
#include "Arduino.h"

inline char* makeKeymap(char keys[][4]) { return (char*)keys; }

class Keypad {
public:
    char _next_key = 0;

    Keypad(char*, byte*, byte*, byte, byte) {}

    char getKey() {
        char k   = _next_key;
        _next_key = 0;
        return k;
    }

    void inject(char k) { _next_key = k; }
};
