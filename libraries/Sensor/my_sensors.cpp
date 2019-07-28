
#include "my_sensors.h"

SENS_ENABLE

MY_SENS::MY_SENS(Sensor* sens, uint8_t num)
{
  sensors = sens;
  size = num;

#if IR_ENABLE
  IRrecvInit();
#endif
}

MY_SENS::~MY_SENS()
{
  
}

float MY_SENS::GetValueByIndex(uint8_t index)
{
  return sensors[index].value;
}

char* MY_SENS::GetNameByIndex(uint8_t index)
{
  return sensors[index].name;
}

void MY_SENS::GetInfo(TEXT *buf)
{
  for(uint8_t i=0;i<size;i++)
  {
    // Выводим только сработавшие датчики
    if(sensors[i].count)
    {
      sensors[i].get_info(buf);
    }
  }
  buf->AddChar('\n');
}

void MY_SENS::GetInfoAll(TEXT *buf)
{
  for(uint8_t i=0;i<size;i++)
  {
    sensors[i].get_info(buf);
  }
  buf->AddChar('\n');
}

void MY_SENS::Clear()
{
  for(uint8_t i=0;i<size;i++) sensors[i].count=0;
}

void MY_SENS::TimeReset()
{      
  for(uint8_t i = 0; i < size; i++)
  {
    sensors[i].start_time = sensors[i].end_time;
  }
}

void MY_SENS::SetEnable(uint8_t value)
{
  enable = value;
}

uint8_t MY_SENS::GetEnable()
{
  return enable;
}

uint8_t MY_SENS::SensOpros()
{
  uint8_t count = 0;

#if IR_ENABLE
  IRread();
#endif

#if RF_ENABLE
  nRFread();
#endif

  for(uint8_t i = 0; i < size; i++)
  {
    if(sensors[i].start_time) sensors[i].start_time--;

    if(!sensors[i].start_time)
    {
      if(sensors[i].get_count())
      {
        if(bitRead(enable,i)) count++;
      }       
    }
  }

#if IR_ENABLE

  IRresume();

#endif

  return count;
}
