#ifndef MESHWAY_H
#define MESHWAY_H

#include <cstdint>
#include <cstddef>
#include <Arduino.h>

/********************************* LoRa Config *********************************************/
#define RF_FREQUENCY        915000000 // Hz
#define TX_OUTPUT_POWER     10        // dBm
#define LORA_BANDWIDTH      0         // 125 kHz
#define LORA_SPREADING_FACTOR 7       // SF7
#define LORA_CODINGRATE     1         // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON        false
#define RX_TIMEOUT_VALUE    1000
#define BUFFER_SIZE         222       // Number of bytes to send
#define MAX_HOP_LEN         255

//message types
#define ROUTE_REQUEST 0
#define ROUTE_REPLY 1

//packet fields
#define TYPE_FIELD 0
#define DESTINATION_FIELD 1
#define HOP_FIELD 2

#define CLIENT_NODE 0
#define GATEWAY_NODE 1

#define DESTINATION_TABLE_MAX_SIZE 8

const uint8_t SERIAL_START[] = {0xAA, 0xBB, 0xCC};
const uint8_t SERIAL_END[]   = {0xDD, 0xEE, 0xFF};
const uint8_t SUBSTART[] = {0x11, 0x22};
const uint8_t SUBEND[]   = {0x33, 0x44};

typedef struct {
  uint8_t destination_id;
  uint8_t hop_count;
} destinationDist;

void sendSubfieldRaw(const char* label, uint8_t* data, uint16_t size);
void sendSubfieldText(const char* label, const char* value);
void sendSubfieldStats(uint16_t len, int16_t rssi, int8_t snr);
void sendPacket();
void meshwayInit(int gateway);
void prepareLoRaBroadcast(char* message, size_t len);
void meshwayRecv();
void sendLoRaMessage(const String& message);

#endif
