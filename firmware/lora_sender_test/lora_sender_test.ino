#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h"

/********************************* LoRa Config *********************************************/
#define RF_FREQUENCY        915000000 // Hz
#define TX_OUTPUT_POWER     10        // dBm
#define LORA_BANDWIDTH      0         // 125 kHz
#define LORA_SPREADING_FACTOR 7       // SF7
#define LORA_CODINGRATE     1         // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON        false
#define BUFFER_SIZE         222       // Number of bytes to send
#define TEAM_NUMBER 0x28

/********************************* Globals *********************************************/
uint8_t txpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;

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

/********************************* LoRa Helpers *********************************************/
void lora_init() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
}

void preparePacket() {
    // First byte is TEAM_NUMBER
    txpacket[0] = TEAM_NUMBER;

    // Fill the rest of the packet with 'a' (ASCII 0x61)
    for (int i = 1; i < BUFFER_SIZE; i++) {
        txpacket[i] = 0x61; // 'a'
    }
}

void sendPacket() {
    // Print to Serial
    Serial.print("Sending packet bytes: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        Serial.print(txpacket[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Display on OLED
    String line = "";
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (txpacket[i] < 0x10) line += "0";
        line += String(txpacket[i], HEX) + " ";
    }

    factoryDisplay.clear();
    factoryDisplay.drawString(0, 0, line); 
    factoryDisplay.display();

    // Send over LoRa
    Radio.Send(txpacket, BUFFER_SIZE);
}

/********************************* Setup & Loop *********************************************/
void setup() {
    Serial.begin(115200);

    VextON();
    initDisplay();
    showLogo();
    delay(1000);

    lora_init();

    preparePacket();
    sendPacket();
}

void loop() {
    // Nothing here, sending only once
}
