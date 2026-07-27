#pragma once

#include "config.h"

class ButtonHandler {
public:
    void begin();                          // Configure GPIO pins with INPUT_PULLUP
    ButtonEvent poll();                    // Read buttons, return debounced event
    bool anyPressed();                     // True if any button currently held
    
private:
    struct ButtonState {
        uint8_t pin;
        bool lastState = true;             // HIGH = not pressed (pull-up)
        bool currentState = true;
        unsigned long lastDebounce = 0;
        unsigned long pressStart = 0;
        bool longFired = false;
    };
    
    ButtonState buttons[4];
    int checkButton(int idx);              // Returns: 0=none, 1=short press, 2=long press
};
