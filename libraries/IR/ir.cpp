
#include "ir.h"
#include <IRremote.h>
#include "settings.h"

IRrecv irrecv(RECV_PIN);

decode_results results;

void IRrecvInit()
{
	irrecv.enableIRIn();
}

bool IRread()
{
	return irrecv.decode(&results);
}

unsigned long IRgetValue()
{
    return results.value;
}

void IRresume()
{
	results.value = 0;
	irrecv.resume();
}
