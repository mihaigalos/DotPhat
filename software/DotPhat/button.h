#pragma once

#include "config.h"
#include "radio.h"

using TVoidVoid = void (*)(void);
TVoidVoid actions[] = {
    nullptr,
    []() { app_status = ApplicationsStatus::VoltageToLeds; },
    nullptr,
    []() { app_status = ApplicationsStatus::TemperatureToLeds; },
    nullptr,
    nullptr,
    nullptr,
    sendDemo,
    []() {
        pinMode(kTRXLed, OUTPUT);
        digitalWrite(kTRXLed, LOW);
        app_status = ApplicationsStatus::RadioReceive;
    }};

void doublePress()
{
    uint8_t currentStateIndex = static_cast<uint8_t>(ButtonMenu::get());
    if (nullptr != actions[currentStateIndex]) {
        actions[currentStateIndex]();
        digitalWrite(kRedLed, HIGH);
        digitalWrite(kGreenLed, HIGH);
        digitalWrite(kBlueLed, HIGH);
    }
}

void singlePress()
{
    ButtonMenu::changeState(kRedLed, kGreenLed, kBlueLed, kTRXLed);
}

void onButtonPress()
{
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis() / (1 << kClockPrescaler);

    if (interrupt_time - last_interrupt_time > 30 / (1 << kClockPrescaler) &&
        interrupt_time - last_interrupt_time < 300 / (1 << kClockPrescaler)) {
        app_status = ApplicationsStatus::Idle;
        doublePress();
    }
    else if (interrupt_time - last_interrupt_time >= 300 / (1 << kClockPrescaler) &&
             interrupt_time - last_interrupt_time < 2000 / (1 << kClockPrescaler)) {
        app_status = ApplicationsStatus::Idle;
        singlePress();
    }
    last_interrupt_time = interrupt_time;
}
