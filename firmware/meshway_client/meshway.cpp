#include "meshway.h"
#include "LoRaWan_APP.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h"

/********************************* Globals *********************************************/
uint8_t txpacket[BUFFER_SIZE];
uint8_t rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;

bool isGateway = false;
volatile bool receiveFlag = false;
int16_t lastRssi;
int8_t lastSnr;
uint16_t lastSize;

destinationDist destination_table[DESTINATION_TABLE_MAX_SIZE];
uint8_t destination_table_size;
uint8_t target_destination;
uint8_t hop_length;

SSD1306Wire factoryDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

/********************************* Hardware Helpers *********************************************/
void VextON()  { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW);  }
void VextOFF() { pinMode(Vext, OUTPUT); digitalWrite(Vext, HIGH); }

/********************************* OLED Helpers *********************************************/


void initDisplay() {
  factoryDisplay.init();
  factoryDisplay.clear();
  factoryDisplay.display();
}

void showLogo() {
  factoryDisplay.clear();
  factoryDisplay.display();
}

/********************************* Callbacks *********************************************/
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  lastSize = size > BUFFER_SIZE ? BUFFER_SIZE : size; // safety check
  memcpy(rxpacket, payload, lastSize);
  lastRssi = rssi;
  lastSnr = snr;
  receiveFlag = true;
  Radio.Sleep();
}

void OnTxDone(void) {}
void OnTxTimeout(void) {}

/********************************* LoRa Helpers *********************************************/
void lora_init() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH, LORA_SYMBOL_TIMEOUT,
                    LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0, LORA_IQ_INVERSION_ON,
                    true);
}

void prepareRouteRequestPacket() {
  txpacket[0] = ROUTE_REQUEST;
  for (int i = 1; i < BUFFER_SIZE; i++) {
    txpacket[i] = 0x00;
  }
}

void prepareRouteReplyPacket() {
    txpacket[0] = ROUTE_REPLY;
    txpacket[1] = destination_table_size;
    int idx = 0;
    for (int i = 0; i < destination_table_size; i++) {
        idx = i * 2 + 2;
        txpacket[idx] = destination_table[i].destination_id;
        txpacket[idx + 1] = destination_table[i].hop_count;
    }
    for (int i = idx + 2; i < BUFFER_SIZE; i++) {
        txpacket[i] = 0x00;
    }
}

void prepareLoRaBroadcast(char* message, size_t len) {
    memset(txpacket, 0, BUFFER_SIZE);
    size_t copyLen = len > BUFFER_SIZE ? BUFFER_SIZE : len;
    memcpy(txpacket, message, copyLen);

    sendPacket();
}

void prepareForwardPacket() {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        txpacket[i] = rxpacket[i];
    }
    txpacket[HOP_FIELD] = rxpacket[HOP_FIELD] - 1;
}

void sendPacket() {
    delay(500);
    Serial.print("Sending packet bytes: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        Serial.print(txpacket[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
    String line = "";
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (txpacket[i] < 0x10) line += "0";
        line += String(txpacket[i], HEX) + " ";
    }
    factoryDisplay.clear();
    factoryDisplay.drawString(0, 12, line); 
    factoryDisplay.display();

    Radio.Send(txpacket, BUFFER_SIZE);
    delay(500);
}

/********************************* Meshway Logic *********************************************/
void updateDestinationTable() {
    int size = rxpacket[1];
    for (int i = 0; i < size; i++) {
        int idx = i * 2 + 2;
        int destination_id = rxpacket[idx];
        int hop_count = rxpacket[idx + 1] + 1;
        int found_entry = 0;
        for (int j = 0; j < destination_table_size; j++) {
            if (destination_table[j].destination_id == destination_id) {
                if (destination_table[j].hop_count < hop_count) {
                    destination_table[j].hop_count = hop_count;
                }
                found_entry = 1;
                break;
            }
        }
        if (!found_entry && destination_table_size < DESTINATION_TABLE_MAX_SIZE) {
            destination_table[destination_table_size].destination_id = destination_id;
            destination_table[destination_table_size].hop_count = hop_count;
            destination_table_size++;
        }
        if (hop_count < hop_length) {
            hop_length = hop_count;
            target_destination = destination_id;
        }
    }
    String line = "";
    for (int i = 0; i < destination_table_size; i++) {
        line += String(destination_table[i].destination_id, HEX) + " ";
        line += String(destination_table[i].hop_count, HEX) + " ";
    }
    factoryDisplay.drawString(0, 8, line);
    factoryDisplay.display();
}


void meshwayInit(int gateway) {
    Serial.begin(115200);

    VextON();
    initDisplay();
    showLogo();

    lora_init();

    isGateway = (gateway != 0);

    if (gateway) {
        destination_table[0] = {1, 0};
        destination_table_size = 1;
    } else {
        target_destination = 0;
        hop_length = MAX_HOP_LEN;
        prepareRouteRequestPacket();
        sendPacket();
    }

    Radio.Rx(0);
}

void meshwayRecv() {
    if (receiveFlag) {
        receiveFlag = false;

        if (isGateway) {
            Serial.write(SERIAL_START, sizeof(SERIAL_START));
            sendSubfieldRaw("RAW:", rxpacket, lastSize);
            sendSubfieldText("TXT:", (char*)rxpacket);
            sendSubfieldStats(lastSize, lastRssi, lastSnr);
            Serial.write(SERIAL_END, sizeof(SERIAL_END));
            Serial.flush();
        }

        int msg_type = rxpacket[TYPE_FIELD];
        if (msg_type == ROUTE_REQUEST) {
            prepareRouteReplyPacket();
            Radio.Standby();
            sendPacket();
        } else if (msg_type == ROUTE_REPLY) {
            updateDestinationTable();
        } else {
            int destination = rxpacket[DESTINATION_FIELD];
            int hop_count = rxpacket[HOP_FIELD];
            for (int i = 0; i < destination_table_size; i++) {
                if (destination_table[i].destination_id == destination && hop_count <= destination_table[i].hop_count) {
                    prepareForwardPacket();
                    Radio.Standby();
                    sendPacket();
                }
            }
        }

        Radio.Rx(0);
    }

    Radio.IrqProcess();
}


uint8_t simpleHash(const char* data, size_t len) {
    uint8_t hash = 0;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
    }
    return hash;
}


void sendLoRaMessage(const String& message) {
    if (destination_table_size == 0) return;
    
    uint8_t destination = destination_table[0].destination_id;
    uint8_t hop = hop_length > 0 ? hop_length - 1 : 0;

    memset(txpacket, 0, BUFFER_SIZE);
    txpacket[TYPE_FIELD] = 2;
    txpacket[DESTINATION_FIELD] = destination;
    txpacket[HOP_FIELD] = hop;
    txpacket[3] = simpleHash(message.c_str(), message.length());
    sendPacket();

    delay(500);

    memset(txpacket, 0, BUFFER_SIZE);
    txpacket[TYPE_FIELD] = 3;
    txpacket[DESTINATION_FIELD] = destination;
    txpacket[HOP_FIELD] = hop;

    size_t copyLen = message.length() > (BUFFER_SIZE - 3) ? (BUFFER_SIZE - 3) : message.length();
    memcpy(txpacket + 3, message.c_str(), copyLen);

    sendPacket();
}



void sendSubfieldRaw(const char* label, uint8_t* data, uint16_t size) {
    Serial.write(SUBSTART, sizeof(SUBSTART));
    Serial.print(label);
    for (uint16_t i = 0; i < size; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.write(SUBEND, sizeof(SUBEND));
    Serial.flush();
}

void sendSubfieldText(const char* label, const char* value) {
    Serial.write(SUBSTART, sizeof(SUBSTART));
    Serial.print(label);
    Serial.println(value);
    Serial.write(SUBEND, sizeof(SUBEND));
    Serial.flush();
}

void sendSubfieldStats(uint16_t len, int16_t rssi, int8_t snr) {
    Serial.write(SUBSTART, sizeof(SUBSTART));
    Serial.printf("LEN:%d RSSI:%d SNR:%d\n", len, rssi, snr);
    Serial.write(SUBEND, sizeof(SUBEND));
    Serial.flush();
}
