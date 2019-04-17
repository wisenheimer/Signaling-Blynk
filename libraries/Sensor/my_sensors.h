#ifndef MY_SENS_H
#define MY_SENS_H

#include "settings.h"
#include "sensor.h"
#include "text.h"

class MY_SENS
{
  public:
    uint8_t tmp;
    uint8_t size;

    MY_SENS(Sensor* sens, uint8_t num);
    ~MY_SENS();
    void GetInfo(TEXT *buf);
    void GetInfoAll(TEXT *buf);
    float GetValueByIndex(uint8_t index);
    char* GetNameByIndex(uint8_t index);
    void Clear();
    uint8_t SensOpros();

    void TimeReset();

  private:

	Sensor* sensors;  
};

#endif // MY_SENS_H
