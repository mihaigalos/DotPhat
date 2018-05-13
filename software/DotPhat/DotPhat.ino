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
  {// software_version
    .major = 1, .minor = 0, .patch = 0
  },
  {// sofware_timestamp_info
    .timezone_sign=1, .utc_offset=1, .is_daylight_saving_active=LOCATION_DAYLIGHT_SAVING, .is_china_time=0
  },
  {// software_version_last_updated_timestamp
    //0x5A, 0xF5, 0x9E, 0x7F
    static_cast<uint8_t>(UNIX_TIMESTAMP>>24), static_cast<uint8_t>(UNIX_TIMESTAMP>>16), static_cast<uint8_t>(UNIX_TIMESTAMP>>8), static_cast<uint8_t>(UNIX_TIMESTAMP)
  },
  {// hardware_version
    .major = 2, .minor = 0, .patch = 0
  },
  {// hardware_timestamp_info
    .timezone_sign=1, .utc_offset=8, .is_daylight_saving_active=1, // true if date is > last sunday of March and < Last sunday of October
    .is_china_time=1
  },
  { // hardware_version_timestamp : add 8 hours to the PCB manufactureing time in China
    0x5A, 0xBF, 0x78, 0x1D
  }
};

EEPROMMetadata e2prom_metadata;
RFM69 rf;
SoftwareUSB software_usb;
ApplicationsStatus app_status;

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
  if(current_configuration != e2prom_metadata)
  {
    EEPROM.put(kEEPROMMetadataAddress, current_configuration);
  }

}

void on_usb_data_receive(uint8_t* data, uint8_t length){
  
  switch (data[0]){
  
    case 's':
    send_radio(&data[1], length-1);  
    app_status = ApplicationsStatus::RadioSend;
    break;

    case 'r':
    app_status = ApplicationsStatus::RadioReceive;
    digitalWrite(kBlueLed, LOW);
    rf.receiveDone(100);
    digitalWrite(kBlueLed, HIGH);
    break;

    case 'x':
    app_status = ApplicationsStatus::Idle;
    break;

    case 'e':
    app_status = ApplicationsStatus::DumpEeprom;
    EEPROM.get(kEEPROMMetadataAddress, e2prom_metadata);
    String stringified_metadata = e2prom_metadata.to_hex();
    software_usb.copyToUSBBuffer(stringified_metadata.c_str(), stringified_metadata.length()); 
    digitalWrite(kGreenLed, LOW);

    break;
  }
}

void send_radio(const char * payload, char length){
  digitalWrite(kGreenLed, LOW);
  rf.send(0x01, payload, length);
  digitalWrite(kGreenLed, HIGH);
}

void on_radio_receive(){
  if (rf.ACKRequested())
  {
    rf.sendACK();
  }

  software_usb.copyToUSBBuffer(rf.DATA, RF69_MAX_DATA_LEN);
  
  digitalWrite(kRedLed, LOW);
  delay(300);
  digitalWrite(kRedLed, HIGH);
}

void loop() {
  software_usb.spin();
}
