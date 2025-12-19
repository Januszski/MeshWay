#include "Arduino.h"
#include "meshway.h"

/********************************* Setup & Loop *********************************************/
void setup() {
  meshwayInit(CLIENT_NODE);
}

void loop() {
  meshwayRecv();
}