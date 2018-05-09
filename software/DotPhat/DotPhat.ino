#include "software_usb.h"

#include <RFM69.h>
#include <EEPROM.h>



enum class ApplicationsStatus { Unknown, Idle, RadioSend, RadioSendPeriodic, RadioReceive};


static constexpr auto kEEPROMMetadataAddress = 0;
static constexpr auto kOwnId = 0x10;
static constexpr auto kMaxRfPower = 31;

static constexpr uint8_t kRedLed = 8;
static constexpr uint8_t kBlueLed = 0;
static constexpr uint8_t kGreenLed = 1;

static constexpr uint8_t kOutALed = 10;
static constexpr uint8_t kOutBLed = 7;

typedef struct SEEPROMMetadata {
  uint8_t software_version[3];
  uint8_t software_version_last_updated_timestamp[4];
  uint8_t hardware_version[3];
  uint8_t hardware_version_timestamp[4];

  bool operator!=(const struct SEEPROMMetadata & rhs) const{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&rhs);
    for( uint16_t i =0 ; i< sizeof(*this) ; ++i ){
      if(*(p + i) != *(reinterpret_cast<const uint8_t*>(this) + i)) return true;
    }
    return false;
  }

  bool is_valid_sw_timestamp(){
    return  0xFF != software_version_last_updated_timestamp[0] && 0x00 != software_version_last_updated_timestamp[0] &&
            0xFF != software_version_last_updated_timestamp[1] && 0x00 != software_version_last_updated_timestamp[1] &&
            0xFF != software_version_last_updated_timestamp[2] && 0x00 != software_version_last_updated_timestamp[2] &&
            0xFF != software_version_last_updated_timestamp[3] && 0x00 != software_version_last_updated_timestamp[3];
  }

}EEPROMMetadata;

static constexpr EEPROMMetadata current_configuration{
  {1,0,2},
  {0x5A, 0xF3, 0x5A, 0x8B},
  {3, 1, 1},
  {0x5A, 0xBE, 0x94, 0xB2}
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
    if(e2prom_metadata.is_valid_sw_timestamp()) EEPROM.put(kEEPROMMetadataAddress, e2prom_metadata);
    else EEPROM.put(kEEPROMMetadataAddress, current_configuration);
      digitalWrite(kGreenLed, LOW);
      while(1);
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
    break;

    case 'x':
    app_status = ApplicationsStatus::Idle;
    break;
  }
}

void send_radio(const char * payload, char length){
  digitalWrite(1, LOW);
  rf.send(0x01, payload, length);
  digitalWrite(1, HIGH);
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

  if(ApplicationsStatus::RadioReceive == app_status){
    digitalWrite(kBlueLed, LOW);
    rf.receiveDone(100);
    digitalWrite(kBlueLed, HIGH);
  }
}
