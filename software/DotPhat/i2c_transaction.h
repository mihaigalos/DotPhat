#pragma once

#include <Wire.h>

void readI2CBytes(const uint8_t destination_address,
                                const uint16_t register_address,
                                const uint8_t count, uint8_t *output) {
  uint8_t transmission_status = 255;

  do {
    Wire.beginTransmission(destination_address);
    Wire.write(static_cast<uint8_t>(register_address >> 8));
    Wire.write(static_cast<uint8_t>(register_address));
    transmission_status = Wire.endTransmission(false);
  } while (0 != transmission_status);

  // Ask the I2C device for data
  Wire.requestFrom(destination_address, count);
  while (!Wire.available());
  for (uint8_t i = 0; i < count; ++i) {
    if (Wire.available()) {
      output[i] = Wire.read();
    }
  }
  Wire.end();
}

uint8_t writeI2CByte(const uint8_t destination_address,
                                   const uint16_t register_address,
                                   const uint8_t data) {
  uint8_t transmission_status = 0x0f;
  Wire.beginTransmission(destination_address);

  Wire.write(static_cast<uint8_t>((register_address >> 8) & 0xFF));
  Wire.write(static_cast<uint8_t>(register_address & 0xFF));
  Wire.write(data);

  uint32_t begin_timestamp = millis();
  do {
    transmission_status = Wire.endTransmission(true);
  } while (millis() - begin_timestamp < 500 && transmission_status != 0);

  return transmission_status;
}

void init_wire(){
  Wire.setClock(400000);
  Wire.begin();
}
