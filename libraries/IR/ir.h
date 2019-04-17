#ifndef IR_H
#define IR_H

void IRrecvInit();
bool IRread();
unsigned long IRgetValue();
void IRresume();

#endif
