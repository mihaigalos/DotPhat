#define F_CPU 16000000UL

#include "software_usb.h"
#include "ButtonMenu.h"

#include <RFM69.h>
#include <EEPROM.h>
#include <Wire.h>

#include "eeprom_metadata.h"
#include "unix_timestamp.h"
#include "own_voltage.h"



enum class ApplicationsStatus { Unknown, Idle, RadioSend, RadioSendPeriodic, RadioReceive, DumpEeprom, VoltageToLeds};

static constexpr auto kOwnId = 0x10;
static constexpr auto kMaxRfPower = 31;

static constexpr uint8_t kRedLed = 8;
static constexpr uint8_t kBlueLed = 0;
static constexpr uint8_t kGreenLed = 1;

static constexpr uint8_t kOutALed = 10;
static constexpr uint8_t kOutBLed = 7;

static constexpr uint8_t kInterruptPin = 3;

static constexpr EEPROMMetadata current_configuration{
  { //metadata_version_info
    .major = 1, .minor = 0, .patch = 0,
  },
  DeviceType::DotPhat,
  { // software_version
    .major = 1, .minor = 0, .patch = 0
  },
  { // sofware_timestamp_info
    .timezone_sign = 1, .utc_offset = 1, .is_daylight_saving_active = LOCATION_DAYLIGHT_SAVING, .is_china_time = 0
  },
  { // software_version_last_updated_timestamp
    static_cast<uint8_t>(UNIX_TIMESTAMP >> 24), static_cast<uint8_t>(UNIX_TIMESTAMP >> 16), static_cast<uint8_t>(UNIX_TIMESTAMP >> 8), static_cast<uint8_t>(UNIX_TIMESTAMP)
  },
  { // hardware_version
    .major = 2, .minor = 0, .patch = 0
  },
  { // hardware_timestamp_info
    .timezone_sign = 1, .utc_offset = 8, .is_daylight_saving_active = 1, // true if date is > last sunday of March and < Last sunday of October
    .is_china_time = 1
  },
  { // hardware_version_timestamp : add 8 hours to the PCB manufactureing time in China
    0x5A, 0xBF, 0x78, 0x1D
  }
};

EEPROMMetadata e2prom_metadata;
RFM69 rf;
SoftwareUSB software_usb;
ApplicationsStatus app_status;

typedef struct {
  uint8_t* payload {nullptr};
  uint8_t payload_length {0};
  uint8_t current_send_count {0};

  uint16_t send_repeatX100 {0};
  int8_t send_repeatCount{0};
  uint32_t start_timestamp {0};
} SendMetadata;

SendMetadata send_metadata;

/*static inline uint8_t readI2CByte(const uint8_t destination_address,
                               const uint16_t register_address) {
  uint8_t data = 0xAA;
  uint8_t transmission_status = 255;

  do {
    Wire.beginTransmission(destination_address);
    Wire.write(static_cast<uint8_t>(register_address >> 8));
    Wire.write(static_cast<uint8_t>(register_address));
    transmission_status = Wire.endTransmission();
  } while (0 != transmission_status);

  // Ask the I2C device for data
  Wire.requestFrom(destination_address, static_cast<uint8_t>(1));
  while (!Wire.available())
    ;
  if (Wire.available()) {
    data = Wire.read();
  }
  Wire.end();
  return data;
}*/


static inline void readI2CBytes(const uint8_t destination_address,
                               const uint16_t register_address, const uint8_t count, uint8_t *output) {
  uint8_t transmission_status = 255;

  do {
    Wire.beginTransmission(destination_address);
    Wire.write(static_cast<uint8_t>(register_address >> 8));
    Wire.write(static_cast<uint8_t>(register_address));
    transmission_status = Wire.endTransmission();
  } while (0 != transmission_status);

  // Ask the I2C device for data
  Wire.requestFrom(destination_address, count);
  while (!Wire.available())
    ;
  for(uint8_t i = 0 ; i < count; ++i){
    if (Wire.available()) {
      output[i] = Wire.read();
    }
  }
  Wire.end();
}

static inline uint8_t writeI2CByte(const uint8_t destination_address,
                                const uint16_t register_address, const uint8_t data) {
  uint8_t transmission_status = 0x0f;
  Wire.beginTransmission(destination_address);
  Wire.write(static_cast<uint8_t>((register_address >> 8)& 0xFF));
  Wire.write(static_cast<uint8_t>(register_address& 0xFF));
  Wire.write(data);

  uint32_t begin_timestamp = millis();
  do{
    transmission_status = Wire.endTransmission(true);
  }while(millis() - begin_timestamp<500 && transmission_status !=0 );

  //Wire.end();
  //if(destination_address == 0x50 && register_address == 0x12 && data == 0x34)
  //  digitalWrite(kBlueLed,false);
  return transmission_status;
}

// the setup function runs once when you press reset or power the board
void setup() {
  Wire.setClock(400000);
  Wire.begin();

  pinMode(kRedLed, OUTPUT);
  pinMode(kBlueLed, OUTPUT);
  pinMode(kGreenLed, OUTPUT);

  pinMode(kOutALed, OUTPUT);
  pinMode(kOutBLed, OUTPUT);
  software_usb.callback_on_usb_data_receive_ = on_usb_data_receive;

  rf.initialize(RF69_868MHZ, kOwnId, 0xFF); // TODO: reenable node address in library
  rf.setHighSensitivity(true);
  rf.promiscuous(true);
  rf.setPowerLevel(kMaxRfPower);
  rf.setCallback(on_radio_receive);

  digitalWrite(kRedLed, HIGH);
  digitalWrite(kBlueLed, HIGH);
  digitalWrite(kGreenLed, HIGH);

  digitalWrite(kOutALed, LOW);
  digitalWrite(kOutBLed, LOW);

  EEPROM.get(kEEPROMMetadataAddress, e2prom_metadata);
  if (current_configuration != e2prom_metadata)
  {
    EEPROM.put(kEEPROMMetadataAddress, current_configuration);
  }
  pinMode(kInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(kInterruptPin), onButtonPress, LOW);
  SoftwareUSB::handler_i2c_read_ = readI2CBytes;
  SoftwareUSB::handler_i2c_write_ = writeI2CByte;
}


void sendDemo(){
  digitalWrite(kBlueLed, HIGH);
  digitalWrite(kGreenLed, HIGH);

  send_metadata.send_repeatCount = -1;
  send_metadata.send_repeatX100 = 5;
  send_metadata.start_timestamp = millis();
  static uint8_t myData[11];
  memcpy(&myData[0],"HelloWorld", 11);
  send_metadata.payload = myData;
  send_metadata.payload_length = 11;
  send_metadata.current_send_count = 0;

  app_status = ApplicationsStatus::RadioSend;
}

void on_usb_data_receive(uint8_t* data, uint8_t length) {

  switch (data[0]) {

    case 's':
      if (':' == data[1]) {

        String command{reinterpret_cast<char*>(data)};

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

        //software_usb.copyToUSBBuffer(bytecount.c_str(), bytecount.length());

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

void send_radio(const char * payload, char length) {
  digitalWrite(kGreenLed, LOW);
  rf.send(0x01, payload, length);
  digitalWrite(kGreenLed, HIGH);
}

void on_radio_receive() {
  if (rf.ACKRequested())
  {
    rf.sendACK();
  }

  software_usb.copyToUSBBuffer(rf.DATA, RF69_MAX_DATA_LEN);

  digitalWrite(kRedLed, LOW);
  delay(50);
  digitalWrite(kRedLed, HIGH);
}

void application_spin() {
  if (ApplicationsStatus::RadioReceive == app_status) {
    digitalWrite(kBlueLed, LOW);
    rf.receiveDone();  // TODO : remove time arguments for receiveDone(), not needed anymore since interrupts occur assynchronously.
    digitalWrite(kBlueLed, HIGH);
  } else if (ApplicationsStatus::Idle == app_status) {
    rf.sleep();
  } else if (ApplicationsStatus::RadioSend == app_status) {
    if (-1 == send_metadata.send_repeatCount || (send_metadata.current_send_count < static_cast<uint8_t>(send_metadata.send_repeatCount))) {
      if (millis() - send_metadata.start_timestamp > send_metadata.send_repeatX100 * 100) {

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
    software_usb.copyToUSBBuffer(stringified_metadata.c_str(), stringified_metadata.length());
    app_status = ApplicationsStatus::Idle;
  }

  if(ApplicationsStatus::VoltageToLeds == app_status) voltageToLeds();
}

void flashLed(uint8_t led, uint8_t times){
  for(uint8_t i = 0; i<times; ++i){
    digitalWrite(led, LOW);
    delay(200);
    digitalWrite(led, HIGH);
    delay(200);
  }
}

void voltageToLeds(){
  digitalWrite(kRedLed, HIGH);
  digitalWrite(kGreenLed, HIGH);
  digitalWrite(kBlueLed, HIGH);

    uint16_t vcc = readVcc();
    uint8_t led =-1, times=0;
    if(vcc >= 2900UL) {
      led = kBlueLed; times=3;
    } else if(vcc >= 2750UL && vcc < 2900UL){
      led = kBlueLed; times=2;
    } else if(vcc >= 2600UL && vcc < 2750UL){
      led = kBlueLed; times=1;
    } else if(vcc >= 2450UL && vcc < 2600UL){
      led = kGreenLed; times=3;
    } else if(vcc >= 2300UL && vcc < 2450UL){
      led = kGreenLed; times=2;
    }else if(vcc >= 2150UL && vcc < 2300UL){
      led = kGreenLed; times=1;
    }else if(vcc >= 2000UL && vcc < 2150UL){
      led = kRedLed; times=3;
    }else if(vcc >= 1900UL && vcc < 2000UL){
      led = kRedLed; times=2;
    }else if(vcc < 1900UL){
      led = kRedLed; times=1;
    }

    flashLed(led, times);
    delay(1000);
}

using TVoidVoid = void(*)(void);
TVoidVoid actions[7] = {
  [](){pinMode(kOutBLed, OUTPUT);digitalWrite(kOutBLed, !digitalRead(kOutBLed));},
  [](){app_status = ApplicationsStatus::VoltageToLeds;},
  nullptr,
  [](){digitalWrite(kRedLed, HIGH); digitalWrite(kBlueLed, HIGH); digitalWrite(kGreenLed, HIGH);app_status = ApplicationsStatus::RadioReceive;},
  sendDemo,
  nullptr,
  [](){pinMode(kOutALed, OUTPUT);digitalWrite(kOutALed, !digitalRead(kOutALed));}
};

void doublePress(){
    uint8_t currentStateIndex = static_cast<uint8_t>(ButtonMenu::get());
    if(nullptr != actions[currentStateIndex]){
      actions[currentStateIndex]();
    }
}

void singlePress(){
    ButtonMenu::changeState(kRedLed, kGreenLed, kBlueLed);
}

void onButtonPress() {
 static unsigned long last_interrupt_time = 0;
 unsigned long interrupt_time = millis();

   if (interrupt_time - last_interrupt_time > 30 && interrupt_time - last_interrupt_time < 300){
      app_status = ApplicationsStatus::Idle;
      doublePress();
   }
   else if (interrupt_time - last_interrupt_time >= 300 && interrupt_time - last_interrupt_time < 2000)
   {
      app_status = ApplicationsStatus::Idle;
      singlePress();
   }
   last_interrupt_time = interrupt_time;
}

void loop() {
  software_usb.spin();
  application_spin();
}
