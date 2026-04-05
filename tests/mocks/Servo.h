#pragma once

class Servo {
public:
    int  _angle      = 0;
    bool _attached   = false;

    void attach(int pin)    { _attached = true; }
    void detach()           { _attached = false; }
    void write(int angle)   { _angle = angle; }
    int  read() const       { return _angle; }
};
