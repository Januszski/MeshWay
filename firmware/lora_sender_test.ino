#include "Arduino.h"
#include "images.h"
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
#define BUFFER_SIZE         17       // Max payload for SF7
#define FIRST_WRITABLE_PACKET_POSITION 1
#define TEAM_NUMBER 0x28
#define TRANSMITTAL_ITERATION 1

/********************************* Globals *********************************************/
uint8_t txpacket[BUFFER_SIZE] = {TEAM_NUMBER};
static RadioEvents_t RadioEvents;

SSD1306Wire factoryDisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
uint64_t chipid;

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
  factoryDisplay.drawXbm(0, 5, logo_width, logo_height, (const unsigned char *)logo_bits);
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

// Generate a random 6-character "geo" string
String randomStringBytes(int length) {
    String s = "";
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < length; i++) {
        s += charset[random(0, sizeof(charset) - 1)];
    }
    return s;
}

std::array<uint8_t, BUFFER_SIZE> generatePayloadBytes(uint8_t reserved[2]) {
    std::array<uint8_t, BUFFER_SIZE> payload;

    // Reserved bytes

    payload[1] = reserved[0];
    payload[2] = reserved[1];

    // Sensor data (scaled to 1 byte each)
    payload[3] = random(50, 101);   // Temp
    payload[4] = random(20, 91);    // Humidity
    payload[5] = random(0, 26);     // Wind speed
    payload[6] = random(0, 151);    // Air quality

/********************************* [INSERT FLAG HERE] *********************************************/
    String flag = randomStringBytes(10); //this will be done by ISEAGE
/********************************* [INSERT FLAG HERE] *********************************************/
    for (int i = 0; i < 10; i++) {
        payload[7 + i] = flag[i];
    }

    return payload;
}

/********************************* [COMPETITOR CODE] *********************************************/

void manipulateOutgoingPayloadData() {

    uint8_t reserved[2] = {0x01, 0x02};

    std::array<uint8_t, BUFFER_SIZE> payloadBytes = generatePayloadBytes(reserved);
    //The above will return something like 01 02 45 3e 0b 73 70 4a 66 72 44 45 56 4b 68 31 
    /**
    which would equate to:
    reserved[0] = 01
    reserved[1] = 02
    temp = 0x45 or 69
    humidity = 0x3e or 62
    wind speed = 0x0b or 11
    air quality = 0x73 or 115
    flag = 0x70 4a 66 72 44 45 56 4b 68 31 or pJfrDEVKh1
    */

    //txpacket is a global variable e.g. uint8_t txpacket[BUFFER_SIZE]; and will be what is ultimately sent over the air
    //The first byte contains the team number. The first byte must not be changed in any way so that the middleware knows which team to route the
    //packet to
    for (int i = FIRST_WRITABLE_PACKET_POSITION; i < BUFFER_SIZE; i++) {
        txpacket[i] = payloadBytes[i];
    }

    //So basically, if you want to for example apply a caesar cipher to the payload, you would loop through
    //each byte in payloadBytes, change the byte (apply the cipher), and then assign that byte to the correct
    //position of txpacket. Remember that you will then need to decrypt/decode after your weather
    //station receives it, so you could do this in mqtt_subscriber.py on weather station
    //or in the POST handler in weather-backend on WWW or anywhere else, it's up to you, as long as the
    //plaintext weather data gets saved in the WWW db and displayed on the website
}

int oledLine = 1; 

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

    factoryDisplay.drawString(0, oledLine * 8, line); 
    factoryDisplay.display();

    oledLine++;
    if (oledLine >= 8) { 
        factoryDisplay.clear();
        oledLine = 0;
    }

    // Send over LoRa
    Radio.Send(txpacket, BUFFER_SIZE);
}

/********************************* Setup & Loop *********************************************/
void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));

    VextON();
    initDisplay();
    showLogo(); delay(3000);
    factoryDisplay.clear();
    lora_init();

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    uint8_t reserved[3] = {0x01, 0x02, 0x03};
    manipulateOutgoingPayloadData();
    for (int i = 0; i < 3; i++) {
        delay(5000);
        sendPacket();
    }
    oledLine++;
    factoryDisplay.drawString(0, oledLine * 8, "Complete!"); 
    factoryDisplay.display();
}

void loop() {
}
