#ifndef RF_H
#define RF_H

#include "RF24.h"
#include "nRF24L01.h"

void nRF24init(byte mode);
byte nRFread(void);
bool nRFwrite(byte sendByte);
byte nRFgetValue(void);
void nRFclear(void);

#endif