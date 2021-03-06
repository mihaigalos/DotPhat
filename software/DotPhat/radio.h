#pragma once

typedef struct {
    uint8_t* payload{nullptr};
    uint8_t payload_length{0};
    uint8_t current_send_count{0};
    uint16_t send_repeatX100{0};
    int8_t send_repeatCount{0};
    uint32_t start_timestamp{0};
} SendMetadata;

SendMetadata send_metadata;

extern void send_radio(const char* payload, char length);

void sendDemo()
{
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

void radio_send_logic(){

  if (-1 == send_metadata.send_repeatCount ||
            (send_metadata.current_send_count <
             static_cast<uint8_t>(send_metadata.send_repeatCount))) {
            if (millis() - send_metadata.start_timestamp > send_metadata.send_repeatX100 * 100) {
                send_radio(&send_metadata.payload[0], send_metadata.payload_length);
                if (-1 != send_metadata.current_send_count) {
                    ++send_metadata.current_send_count;
                }
                send_metadata.start_timestamp = millis();
            }
        }
        else {
            app_status = ApplicationsStatus::Idle;
        }
}
