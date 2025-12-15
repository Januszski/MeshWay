#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "images.h"

/********************************* LoRa Config *********************************************/
#define RF_FREQUENCY        915000000 // Hz
#define TX_OUTPUT_POWER     10        // dBm
#define LORA_BANDWIDTH      0         // 125 kHz
#define LORA_SPREADING_FACTOR 7       // SF7
#define LORA_CODINGRATE     1         // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE    1000
#define BUFFER_SIZE         222 // max payload for SF7/125kHz

/********************************* Globals *********************************************/
uint8_t rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;

volatile bool receiveFlag = false;
int16_t lastRssi;
int8_t lastSnr;
uint16_t lastSize;

SSD1306Wire factoryDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

/********************************* OLED Helpers *********************************************/
void VextON()  { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW);  }
void VextOFF() { pinMode(Vext, OUTPUT); digitalWrite(Vext, HIGH); }

void initDisplay() {
  factoryDisplay.init();
  factoryDisplay.clear();
  factoryDisplay.display();
}

void showLogo() {
  factoryDisplay.clear();
  factoryDisplay.drawXbm(0, 5, logo_width, logo_height, (const unsigned char *)logo_bits);
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

/********************************* LoRa Init *********************************************/
void lora_init(void) {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetRxConfig(
    MODEM_LORA,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    0,
    LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT,
    LORA_FIX_LENGTH_PAYLOAD_ON,
    0,
    true,
    0,
    0,
    LORA_IQ_INVERSION_ON,
    true
  );
}

/********************************* Setup & Loop *********************************************/
void setup() {
  Serial.begin(115200);
  delay(1000);

  VextON();
  initDisplay();
  showLogo();

  Serial.println("LoRa Receiver starting...");
  lora_init();
  Radio.Rx(0); // start receiving
}

void loop() {
  if (receiveFlag) {
    receiveFlag = false;

    // Print raw packet in HEX
    Serial.print("RAW: ");
    for (int i = 0; i < lastSize; i++) {
      if (rxpacket[i] < 0x10) Serial.print("0");
      Serial.print(rxpacket[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Print as ASCII text (safe up to lastSize)
    Serial.print("TXT: ");
    for (int i = 0; i < lastSize; i++) {
      char c = (char)rxpacket[i];
      Serial.print(c);
    }
    Serial.println();

    // Print metadata
    Serial.printf("LEN: %d RSSI: %d SNR: %d\n", lastSize, lastRssi, lastSnr);

    // Display first line on OLED
    String line = "";
    for (int i = 0; i < lastSize && i < 16; i++) { // display only first 16 bytes
      if (rxpacket[i] < 0x10) line += "0";
      line += String(rxpacket[i], HEX) + " ";
    }
    factoryDisplay.clear();
    factoryDisplay.drawString(0, 0, line);
    factoryDisplay.display();

    Radio.Rx(0); // go back to receive mode
  }

  Radio.IrqProcess();
}
