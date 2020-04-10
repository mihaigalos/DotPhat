#define F_CPU 16000000UL

#include "ButtonMenu.h"
#include "software_usb.h"

#include <prescaler.h>

#include <EEPROM.h>
#include <RFM69.h>

#include <tmp112.h>


#include "supercapacitor.h"
#include "unix_timestamp.h"

#include "eeprom_config.h"
#include "config.h"
#include "i2c_transaction.h"
#include "temperature_to_leds.h"


enum class ApplicationsStatus {
  Unknown,
  Idle,
  RadioSend,
  RadioSendPeriodic,
  RadioReceive,
  DumpEeprom,
  VoltageToLeds,
  TemperatureToLeds
};

RFM69 rf;
SoftwareUSB software_usb;
ApplicationsStatus app_status;

typedef struct {

  uint8_t *payload{nullptr};
  uint8_t payload_length{0};
  uint8_t current_send_count{0};
  uint16_t send_repeatX100{0};
  int8_t send_repeatCount{0};
  uint32_t start_timestamp{0};
} SendMetadata;

SendMetadata send_metadata;


void setup() {

  setClockPrescaler(kClockPrescaler); //needed for smooth running under 3V.

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


void sendDemo() {
  send_metadata.send_repeatCount = -1;
  send_metadata.send_repeatX100 = 5;
  send_metadata.start_timestamp = millis();
  static uint8_t myData[11];
  memcpy(&myData[0], "HelloWorld", 11);
  send_metadata.payload = myData;
  send_metadata.payload_length = 11;
  send_metadata.current_send_count = 0;

  app_status = ApplicationsStatus::RadioSend;
}


void on_usb_data_receive(uint8_t *data, uint8_t length) {

  switch (data[0]) {

  case 's':
    if (':' == data[1]) {

      String command{reinterpret_cast<char *>(data)};


      int posistion_separator_delay = command.indexOf(":", 2);
      uint8_t payload_start = 2;
      int posistion_separator_repeat_count = 0;


      if (-1 != posistion_separator_delay) { //:<number>:
          int posistion_separator_repeat_count = command.indexOf(":", posistion_separator_delay + 1);
          payload_start = posistion_separator_repeat_count + 1;
          send_metadata.send_repeatX100 = command.substring(2, posistion_separator_delay).toInt();
          if (-1 != posistion_separator_repeat_count) { //:<number>:<number>:
            send_metadata.send_repeatCount = command.substring(posistion_separator_delay + 1, posistion_separator_repeat_count).toInt();
          } else {
            send_metadata.send_repeatCount = -1;
          }
        } else {
          send_metadata.send_repeatX100 = 0;
          send_metadata.send_repeatCount = 1;
        }

      send_metadata.start_timestamp = millis();
      send_metadata.payload = &data[payload_start];
      send_metadata.payload_length = length - payload_start;
      send_metadata.current_send_count = 0;

        
      // software_usb.copyToUSBBuffer(bytecount.c_str(), bytecount.length());


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

void send_radio(const char *payload, char length) {
  pinMode(kTRXLed, OUTPUT);
  digitalWrite(kTRXLed, HIGH);
  rf.send(0x01, payload, length);
  pinMode(kTRXLed, INPUT);
}

void on_radio_receive() {

  pinMode(kTRXLed, INPUT);

  if (rf.ACKRequested()) {
    rf.sendACK();
  }

  software_usb.copyToUSBBuffer(rf.DATA, RF69_MAX_DATA_LEN);
  delay(100/(1<<kClockPrescaler));
  pinMode(kTRXLed, OUTPUT);
  digitalWrite(kTRXLed, LOW);
}

void application_spin() {
  if (ApplicationsStatus::RadioReceive == app_status) {

    rf.receiveDone(); // TODO : remove time arguments for receiveDone(), not
                      // needed anymore since interrupts occur asynchronously.
  } else if (ApplicationsStatus::Idle == app_status) {
    rf.sleep();
  } else if (ApplicationsStatus::RadioSend == app_status) {
    if (-1 == send_metadata.send_repeatCount ||
        (send_metadata.current_send_count <
         static_cast<uint8_t>(send_metadata.send_repeatCount))) {
      if (millis() - send_metadata.start_timestamp >
          send_metadata.send_repeatX100 * 100) {

        send_radio(&send_metadata.payload[0], send_metadata.payload_length);
        if (-1 != send_metadata.current_send_count) {
          ++send_metadata.current_send_count;
        }
        send_metadata.start_timestamp = millis();
      }
    } else {
      app_status = ApplicationsStatus::Idle;
    }
  } else if (ApplicationsStatus::DumpEeprom == app_status) {
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


void voltageToLeds() {
  auto supercap = Supercapacitor3V(kRedLed, kGreenLed, kBlueLed);
  supercap.voltageToLeds();
}


using TVoidVoid = void (*)(void);
TVoidVoid actions[] = {
  nullptr,
  [](){app_status = ApplicationsStatus::VoltageToLeds;},
  nullptr,
  [](){app_status = ApplicationsStatus::TemperatureToLeds;},
  nullptr,
  nullptr,
  nullptr,
  sendDemo,
  [](){pinMode(kTRXLed, OUTPUT); digitalWrite(kTRXLed, LOW);app_status = ApplicationsStatus::RadioReceive;}
};


void doublePress() {
  uint8_t currentStateIndex = static_cast<uint8_t>(ButtonMenu::get());
  if (nullptr != actions[currentStateIndex]) {
    actions[currentStateIndex]();
    digitalWrite(kRedLed, HIGH);
    digitalWrite(kGreenLed, HIGH);
    digitalWrite(kBlueLed, HIGH);
  }
}

void singlePress() {
  ButtonMenu::changeState(kRedLed, kGreenLed, kBlueLed, kTRXLed);
}

void onButtonPress() {
  static unsigned long last_interrupt_time = 0;
 unsigned long interrupt_time = millis()/(1<<kClockPrescaler );


  if (interrupt_time - last_interrupt_time > 30/(1<<kClockPrescaler ) &&
      interrupt_time - last_interrupt_time < 300/(1<<kClockPrescaler )) {
    app_status = ApplicationsStatus::Idle;
    doublePress();
  } else if (interrupt_time - last_interrupt_time >= 300/(1<<kClockPrescaler ) &&
             interrupt_time - last_interrupt_time < 2000/(1<<kClockPrescaler )) {
    app_status = ApplicationsStatus::Idle;
    singlePress();
  }
  last_interrupt_time = interrupt_time;
}

void loop() {
  software_usb.spin();
  application_spin();
}
