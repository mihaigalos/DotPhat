#pragma once

#include "config.h"
#include <tmp112.h>

void temperatureToLeds()
{
    delay(3000 / (1 << kClockPrescaler));
    digitalWrite(kRedLed, HIGH);
    digitalWrite(kBlueLed, HIGH);
    digitalWrite(kGreenLed, HIGH);

    Tmp112 tmp112;
    float temperature = tmp112.getTemperature();
    uint8_t whole = static_cast<int>(temperature);

    float fraction = temperature - static_cast<float>(whole);

    uint8_t whole_digit_1 = whole / 10;
    uint8_t whole_digit_2 = whole % 10;
    uint8_t fraction_digit = static_cast<uint8_t>(fraction * 10);

    for (uint8_t i = 0; i < whole_digit_1; ++i) {
        digitalWrite(kGreenLed, LOW);
        delay(300 / (1 << kClockPrescaler));
        digitalWrite(kGreenLed, HIGH);
        delay(300 / (1 << kClockPrescaler));
    }
    delay(1000 / (1 << kClockPrescaler));
    for (uint8_t i = 0; i < whole_digit_2; ++i) {
        digitalWrite(kGreenLed, LOW);
        delay(300 / (1 << kClockPrescaler));
        digitalWrite(kGreenLed, HIGH);
        delay(300 / (1 << kClockPrescaler));
    }
    delay(1000 / kClockPrescaler);
    for (uint8_t i = 0; i < fraction_digit; ++i) {
        digitalWrite(kRedLed, LOW);
        delay(300 / (1 << kClockPrescaler));
        digitalWrite(kRedLed, HIGH);
        delay(300 / (1 << kClockPrescaler));
    }
}
