#pragma once

#include "radio.h"

void radio_send_from_usb(uint8_t *data, uint8_t usb_data_length){
  
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
    send_metadata.payload_length = usb_data_length - payload_start;
    send_metadata.current_send_count = 0;
       
      // software_usb.copyToUSBBuffer(bytecount.c_str(), bytecount.length());
}
