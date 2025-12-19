#include "Arduino.h"
#include "meshway.h"
#include "meshway_client.h"

void setup() {
  meshwayInit(0);         // Start LoRa
  meshwayClientInit();    // Start Wi-Fi + web server
}

void loop() {
  meshwayRecv();          // LoRa receive
  meshwayClientLoop();    // Web server handler
}
