#include "Arduino.h"
#include "meshway.h"

/********************************* Setup & Loop *********************************************/
void setup() {
  meshwayInit(0);
}

void loop() {
  meshwayRecv();
}