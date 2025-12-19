#include "Arduino.h"
#include "meshway.h"

/********************************* Setup & Loop *********************************************/
void setup() {
  meshwayInit(1);
}

void loop() {
  meshwayRecv();
}