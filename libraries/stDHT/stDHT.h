#ifndef DHT_H
#define DHT_H

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#define DHT11 11
#define DHT21 21
#define DHT22 22

class DHT{
  public:
   DHT(uint8_t type,uint8_t pin);
   void begin(void);
   int8_t readTemperature(void);
   int8_t readHumidity(void);
   boolean read(void);

 private:
  uint8_t data[6];
  uint8_t _type, _pin, bit, port;
  uint32_t _lastreadtime;
  uint16_t _maxcycles;
  bool _lastresult;
  volatile uint8_t *reg, *out;

  uint16_t expectPulse(bool level);

};

class InterruptLock {
  public:
   InterruptLock() {
    noInterrupts();
   }
   ~InterruptLock() {
    interrupts();
   }

};

#endif
