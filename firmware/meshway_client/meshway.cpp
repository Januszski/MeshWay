#include "meshway.h"
#include "LoRaWan_APP.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h"

/********************************* Globals *********************************************/
uint8_t txpacket[BUFFER_SIZE];
uint8_t rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;

volatile bool receiveFlag = false;
int16_t lastRssi;
int8_t lastSnr;
uint16_t lastSize;

destinationDist destination_table[DESTINATION_TABLE_MAX_SIZE];
uint8_t destination_table_size;

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
    // Copy message into txpacket
    memset(txpacket, 0, BUFFER_SIZE);
    size_t copyLen = len > BUFFER_SIZE ? BUFFER_SIZE : len;
    memcpy(txpacket, message, copyLen);

    // Send packet
    sendPacket();
}

void sendPacket() {
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
    delay(250);
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
    }
    String line = "";
    for (int i = 0; i < destination_table_size; i++) {
        line += String(destination_table[i].destination_id, HEX) + " ";
        line += String(destination_table[i].hop_count, HEX) + " ";
    }
    factoryDisplay.drawString(0, 6, line);
}

void meshwayInit(int gateway) {
    Serial.begin(115200);

    VextON();
    initDisplay();
    showLogo();
    delay(500);

    lora_init();

    if (gateway) {
        destination_table[0] = {1, 0};
        destination_table_size = 1;
    } else {
        prepareRouteRequestPacket();
        sendPacket();
    }

    Radio.Rx(0);
}

void meshwayRecv() {
    if (receiveFlag) {
        receiveFlag = false;

        String line = "";
        for (int i = 0; i < lastSize && i < 16; i++) {
        if (rxpacket[i] < 0x10) line += "0";
        line += String(rxpacket[i], HEX) + " ";
        }
        factoryDisplay.clear();
        factoryDisplay.drawString(0, 0, line);
        factoryDisplay.display();
        /*
        int msg_type = rxpacket[0];
        if (msg_type == ROUTE_REQUEST) {
            //Radio.Standby();
            prepareRouteReplyPacket();
            //sendPacket();
        } else if (msg_type == ROUTE_REPLY) {
            updateDestinationTable();
        } */

        Radio.Rx(0);
    }

  Radio.IrqProcess();
}
