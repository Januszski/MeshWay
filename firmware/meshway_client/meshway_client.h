#ifndef MESHWAY_CLIENT_H
#define MESHWAY_CLIENT_H

#include <Arduino.h>

void meshwayClientInit();  // Initialize Wi-Fi AP + web server
void meshwayClientLoop();  // Call repeatedly in loop

#endif
