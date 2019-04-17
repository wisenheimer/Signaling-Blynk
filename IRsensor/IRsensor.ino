/*
 * Беспроводной ИК датчик.
 * IRsensor: sending IR codes with IRsend
 * An IR LED must be connected to Arduino PWM pin 3.
 *
 * Перед прошивкой настроить файл
 * /libraries/main_type/settings.h
 *
 * @Author: wisenheimer
 * @Date:   2019-04-17 8:30:00
 * @Last Modified by:   
 * @Last Modified time:
 */

#include <IRremote.h>
#include "my_sensors.h"

IRsend irsend;

// продолжительность отправки сообщения при срабатывании датчика, в секундах
#define ALARM_MAX_TIME 10

// активируем флаг тревоги для сбора информации и отправки ик кода на сигнализацию
#define ALARM_ON  if(!GET_FLAG(ALARM)){SET_FLAG_ONE(ALARM);AlarmTime=ALARM_MAX_TIME;}
// по окончании времени ALARM_MAX_TIME обнуляем флаг тревоги
#define ALARM_OFF {SET_FLAG_ZERO(ALARM);sensors->Clear();}

static uint32_t msec = 0;
static uint8_t AlarmTime = 0;
uint8_t flags = 0;

MY_SENS *sensors = NULL;

// Размер массива должен равняться количеству датчиков!
# define SENS_NUM 2
// Сюда надо вписать свои датчики. Описание смотрите в 
// https://github.com/wisenheimer/Signaling-Blynk/blob/master/README.md
Sensor sens[SENS_NUM]=
{
  Sensor(DOOR_PIN,DIGITAL_SENSOR, "DOOR", HIGH, 0),
  Sensor(5,       DIGITAL_SENSOR, "MOVE", LOW)
};

void setup()
{
#if WTD_ENABLE
  wdt_disable();
  wdt_enable(WDTO_8S);
#endif

  sensors = new MY_SENS(sens, SENS_NUM);  
}

void timer(uint16_t time)
{
#if WTD_ENABLE
  wdt_reset();
#endif
  if(millis() - msec >= time)
  {
    msec = millis();
 
    // Опрос датчиков
    if(sensors->SensOpros())
    {
      ALARM_ON // режим тревога вкл.
      irsend.sendNEC(IR_CODE, IR_CODE_BIT_SIZE);
    }
    if(GET_FLAG(ALARM))
    {
      if(AlarmTime) AlarmTime--;
      else // По истечении заданного времени ALARM_MAX_TIME 
      {
        ALARM_OFF; // Выключаем режим тревоги. Очищаем статистику датчиков.            
      }
    } 
  }
}

void loop()
{
  while(1)
  {
    timer(1000);
  }    
}
