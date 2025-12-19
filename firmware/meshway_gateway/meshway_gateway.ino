#include "Arduino.h"
#include "meshway.h"

/********************************* Setup & Loop *********************************************/
void setup() {
  meshwayInit(GATEWAY_NODE);
}

void loop() {
  meshwayRecv();
}