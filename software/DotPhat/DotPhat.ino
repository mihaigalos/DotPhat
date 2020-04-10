#define F_CPU 16000000UL

#include "ButtonMenu.h"
#include "software_usb.h"

#include <prescaler.h>

#include <EEPROM.h>
#include <RFM69.h>

#include "supercapacitor.h"
#include "unix_timestamp.h"

#include "button.h"
#include "config.h"
#include "eeprom_config.h"
#include "i2c_transaction.h"
#include "temperature_to_leds.h"

#include "parse_usb_command.h"

RFM69 rf;
SoftwareUSB software_usb;

void setup()
{
    setClockPrescaler(kClockPrescaler); // needed for smooth running under 3V.

    init_wire();

    pinMode(kRedLed, OUTPUT);
    pinMode(kBlueLed, OUTPUT);
    pinMode(kGreenLed, OUTPUT);

    software_usb.callback_on_usb_data_receive_ = on_usb_data_receive;

    rf.initialize(RF69_868MHZ, kOwnId,
                  0xFF); // TODO: reenable node address in library
    // rf.setHighSensitivity(true);
    rf.promiscuous(true);
    rf.setPowerLevel(kMaxRfPower);
    rf.setCallback(on_radio_receive);

    digitalWrite(kRedLed, HIGH);
    digitalWrite(kBlueLed, HIGH);
    digitalWrite(kGreenLed, HIGH);

    update_eeprom_config(current_configuration, e2prom_metadata);

    pinMode(kInterruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(kInterruptPin), onButtonPress, LOW);
    SoftwareUSB::handler_i2c_read_ = readI2CBytes;
    SoftwareUSB::handler_i2c_write_ = writeI2CByte;
}

void loop()
{
    software_usb.spin();
    application_spin();
}

void on_usb_data_receive(uint8_t* data, uint8_t usb_data_length)
{
    switch (data[0]) {
    case 's':
        if (':' == data[1]) {
            parse_send_command_from_usb(data, usb_data_length);
            app_status = ApplicationsStatus::RadioSend;
        }
        break;

    case 'r':
        app_status = ApplicationsStatus::RadioReceive;
        break;

    case 'x':
        app_status = ApplicationsStatus::Idle;
        break;

    case 'e':
        app_status = ApplicationsStatus::DumpEeprom;
        break;
    }
}

void send_radio(const char* payload, char length)
{
    pinMode(kTRXLed, OUTPUT);
    digitalWrite(kTRXLed, HIGH);
    rf.send(0x01, payload, length);
    pinMode(kTRXLed, INPUT);
}

void on_radio_receive(SoftwareUSB& software_usb)
{
    pinMode(kTRXLed, INPUT);

    if (rf.ACKRequested()) {
        rf.sendACK();
    }

    software_usb.copyToUSBBuffer(rf.DATA, RF69_MAX_DATA_LEN);
    delay(100 / (1 << kClockPrescaler));
    pinMode(kTRXLed, OUTPUT);
    digitalWrite(kTRXLed, LOW);
}
void application_spin()
{
    if (ApplicationsStatus::RadioReceive == app_status) {
        rf.receiveDone(); // TODO : remove time arguments for receiveDone(), not
                          // needed anymore since interrupts occur asynchronously.
    }
    else if (ApplicationsStatus::Idle == app_status) {
        rf.sleep();
    }
    else if (ApplicationsStatus::RadioSend == app_status) {
        radio_send_logic();
    }
    else if (ApplicationsStatus::DumpEeprom == app_status) {
        EEPROM.get(kEEPROMMetadataAddress, e2prom_metadata);
        String stringified_metadata = e2prom_metadata.to_hex();

        software_usb.copyToUSBBuffer(stringified_metadata.c_str(),
                                     stringified_metadata.length());
        app_status = ApplicationsStatus::Idle;
    }

    if (ApplicationsStatus::VoltageToLeds == app_status)
        voltageToLeds();
    else if (ApplicationsStatus::TemperatureToLeds == app_status)
        temperatureToLeds();
}

void voltageToLeds()
{
    auto supercap = Supercapacitor3V(kRedLed, kGreenLed, kBlueLed);
    supercap.voltageToLeds();
}
