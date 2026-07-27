#include "button_handler.h"

void ButtonHandler::begin() {
    buttons[0].pin = PIN_BTN_UP;
    buttons[1].pin = PIN_BTN_DOWN;
    buttons[2].pin = PIN_BTN_SELECT;
    buttons[3].pin = PIN_BTN_BACK;
    
    for (int i = 0; i < 4; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
        buttons[i].lastState = true;
        buttons[i].currentState = true;
        buttons[i].lastDebounce = 0;
        buttons[i].pressStart = 0;
        buttons[i].longFired = false;
    }
}

int ButtonHandler::checkButton(int idx) {
    bool reading = digitalRead(buttons[idx].pin);
    int result = 0;
    
    if (reading != buttons[idx].lastState) {
        buttons[idx].lastDebounce = millis();
    }
    
    if ((millis() - buttons[idx].lastDebounce) > BTN_DEBOUNCE_MS) {
        if (reading != buttons[idx].currentState) {
            buttons[idx].currentState = reading;
            
            if (buttons[idx].currentState == LOW) { // Pressed
                buttons[idx].pressStart = millis();
                buttons[idx].longFired = false;
            } else { // Released
                if (!buttons[idx].longFired) {
                    result = 1; // Short press
                }
            }
        }
    }
    
    if (buttons[idx].currentState == LOW && !buttons[idx].longFired) {
        if ((millis() - buttons[idx].pressStart) > BTN_LONG_PRESS_MS) {
            buttons[idx].longFired = true;
            result = 2; // Long press
        }
    }
    
    buttons[idx].lastState = reading;
    return result;
}

ButtonEvent ButtonHandler::poll() {
    for (int i = 0; i < 4; i++) {
        int state = checkButton(i);
        if (state == 1) {
            if (i == 0) return ButtonEvent::PRESS_UP;
            if (i == 1) return ButtonEvent::PRESS_DOWN;
            if (i == 2) return ButtonEvent::PRESS_SELECT;
            if (i == 3) return ButtonEvent::PRESS_BACK;
        } else if (state == 2) {
            if (i == 2) return ButtonEvent::LONG_SELECT;
            if (i == 3) return ButtonEvent::LONG_BACK;
        }
    }
    return ButtonEvent::NONE;
}

bool ButtonHandler::anyPressed() {
    for (int i = 0; i < 4; i++) {
        if (digitalRead(buttons[i].pin) == LOW) {
            return true;
        }
    }
    return false;
}
