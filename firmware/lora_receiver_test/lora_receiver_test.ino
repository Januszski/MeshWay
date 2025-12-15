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
#define BUFFER_SIZE         64

/********************************* Globals *********************************************/
char rxpacket[BUFFER_SIZE];
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
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  lastRssi = rssi;
  lastSnr = snr;
  lastSize = size;
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
  Radio.Rx(0);
}


/********************************* Setup & Loop *********************************************/

void manipulateIncomingPayload(uint8_t *data, uint16_t size, uint8_t key) {
  for (uint16_t i = 0; i < size; i++) {
    data[i] ^= key;
  }
}

void loop() {
  if (receiveFlag) {
    receiveFlag = false;

    Serial.print("RAW:");
    for (int i = 0; i < lastSize; i++) {
      if ((uint8_t)rxpacket[i] < 0x10) Serial.print("0");
      Serial.print((uint8_t)rxpacket[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    Serial.print("TXT:");
    Serial.println(rxpacket);

    Serial.printf("LEN:%d RSSI:%d SNR:%d\n", lastSize, lastRssi, lastSnr);

    Radio.Rx(0);
  }

  Radio.IrqProcess();
}
