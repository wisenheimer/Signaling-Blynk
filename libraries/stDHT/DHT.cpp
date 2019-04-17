#include "stDHT.h"

DHT::DHT(uint8_t type,uint8_t pin)
{
  _type = type;
  _pin = pin; 
  _lastreadtime = 0;
  _lastresult = 0;
  _maxcycles = microsecondsToClockCycles(1000);
  bit = digitalPinToBitMask(pin);
  
  do{
    port = digitalPinToPort(pin);
  }while(port == NOT_A_PIN);

  out = portOutputRegister(port);
  reg = portModeRegister(port);
}

int8_t DHT::readTemperature(void) 
{      
  int8_t f = NAN;

//  if (port == NOT_A_PIN) return f;

  if(read()) 
  {
    switch (_type) 
    {
    case DHT11:
      f = data[2];
      break;
      
    case DHT22:
    case DHT21:
      f = data[2] & 0x7F;
      f <<= 8;
      f += data[3];
      f /= 10;
      if(data[2] & 0x80) 
      {
        f *= -1;
      }
      break;
    }
  }
  
  return f;
}

int8_t DHT::readHumidity(void) 
{
  int8_t f = NAN;

//  if (port == NOT_A_PIN) return f;
  
  if(read()) 
  {
    switch (_type) 
    {
      case DHT11:
        f = data[0];
      break;
        
      case DHT22:
      case DHT21:
        f = data[0];
        f <<= 8;
        f += data[1];
        f /= 10;
      break;
    }
  }
    
  return f;
 }

boolean DHT::read(void) 
{
  uint32_t currenttime = millis();
    
  if((currenttime - _lastreadtime) < 2000)
  {
    return _lastresult; 
  }
    
  _lastreadtime = currenttime;

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  *out |= bit; // digitalWrite(_pin, HIGH);
  delay(50);//////////////////////// Изначально это значение было - 250. Если что-то будет работать "криво",
  ////////////////////////////////// попробуйте увеличивать значение с 50-ти до 60 и т.д.

  *reg |= bit;  //pinMode(_pin, OUTPUT);
  *out &= ~bit; //digitalWrite(_pin, LOW);
  delay(20); 

  InterruptLock lock;
  *out |= bit;  //digitalWrite(_pin, HIGH);
  delayMicroseconds(40);
  *reg &= ~bit; //pinMode(_pin, INPUT);
  delayMicroseconds(10);  

  if(!expectPulse(LOW) || !expectPulse(HIGH)) 
  {
    _lastresult = false;
    return _lastresult;
  }

  for(int i=0; i<40; ++i) 
  {
    uint16_t lowCycles  = expectPulse(LOW);
    uint16_t highCycles = expectPulse(HIGH);
    
    if(!lowCycles || !highCycles) 
    {
      _lastresult = false;
      return _lastresult;
    }
    data[i>>3] <<= 1;

    if(highCycles > lowCycles) 
    {
      data[i>>3] |= 1;
    }
  }

  if(data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) 
  {
    _lastresult = true;
    return _lastresult;
  }   
  else 
  {
    _lastresult = false;
    return _lastresult;
  }
}

uint16_t DHT::expectPulse(bool level) 
{
  uint16_t count = 0;
  uint8_t portState = level ? bit : 0;
  while((*portInputRegister(port) & bit) == portState) 
  {
    if(count++ >= _maxcycles) return 0;
  }

  return count;
}
