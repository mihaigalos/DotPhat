#include "software_usb.h"

#include <RFM69.h>
#include <EEPROM.h>

#include "eeprom_metadata.h"
#include "unix_timestamp.h"

enum class ApplicationsStatus { Unknown, Idle, RadioSend, RadioSendPeriodic, RadioReceive, DumpEeprom};

static constexpr auto kOwnId = 0x10;
static constexpr auto kMaxRfPower = 31;

static constexpr uint8_t kRedLed = 8;
static constexpr uint8_t kBlueLed = 0;
static constexpr uint8_t kGreenLed = 1;

static constexpr uint8_t kOutALed = 10;
static constexpr uint8_t kOutBLed = 7;

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

constexpr uint8_t interruptPin = 3;
uint8_t currentLedPin = kRedLed;
volatile uint8_t state = LOW;

// the setup function runs once when you press reset or power the board
void setup() {
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
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), blink, LOW);
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
}


void blink() {
 static unsigned long last_interrupt_time = 0;
 unsigned long interrupt_time = millis();

   if (interrupt_time - last_interrupt_time > 20 && interrupt_time - last_interrupt_time < 200){

    digitalWrite(currentLedPin, HIGH);
    if (kRedLed == currentLedPin) currentLedPin = kGreenLed ;
    else if ( kGreenLed == currentLedPin) currentLedPin = kRedLed;
    state = LOW;
   }
   else if (interrupt_time - last_interrupt_time > 200)
   {
    state = !state;
   }
   last_interrupt_time = interrupt_time;
}

void foobar(){
  digitalWrite(currentLedPin, state);
}

void loop() {
  software_usb.spin();
  application_spin();
  foobar();
}
