#include "software_usb.h"

#include <RFM69.h>

enum class ApplicationsStatus { Unknown, Idle, RadioSend, RadioSendPeriodic, RadioReceive};

static constexpr auto kOwnId = 0x10;
static constexpr auto kMaxRfPower = 31;

static constexpr uint8_t kRedLed = 8;
static constexpr uint8_t kBlueLed = 0;
static constexpr uint8_t kGreenLed = 1;

static constexpr uint8_t kOutALed = 10;
static constexpr uint8_t kOutBLed = 7;

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

  digitalWrite(kOutALed, HIGH);
  digitalWrite(kOutBLed, HIGH);
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
  
  //(char*)rf.DATA
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
