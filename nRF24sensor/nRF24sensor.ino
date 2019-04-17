/*
 * Беспроводной датчик с передающим радиомодулем nRF24L01.
 * 
 * Перед прошивкой настроить файл
 * libraries/main_type/settings.h
 *
 * @Author: wisenheimer
 * @Date:   2019-04-17 8:30:00
 * @Last Modified by:   
 * @Last Modified time:
 */

#include <SPI.h>
#include "nRF24.h"
#include "my_sensors.h"
#include "text.h"

// код для отправки в эфир при тревоге
#define RF_CODE RF0_CODE
// продолжительность отправки сообщения при срабатывании датчика, в секундах
#define ALARM_MAX_TIME 10

byte counter;

TEXT* text;

// активируем флаг тревоги для сбора информации и отправки кода на сигнализацию
#define ALARM_ON  if(!GET_FLAG(ALARM)){SET_FLAG_ONE(ALARM);AlarmTime=ALARM_MAX_TIME;sensors->GetInfo(text);DEBUG_PRINTLN(text->GetText());}
// по окончании времени ALARM_MAX_TIME обнуляем флаг тревоги
#define ALARM_OFF {SET_FLAG_ZERO(ALARM);sensors->GetInfo(text);DEBUG_PRINTLN(text->GetText());text->Clear();sensors->Clear();}

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

  SERIAL_BEGIN; // открываем порт для связи с ПК
  
  sensors = new MY_SENS(sens, SENS_NUM);
  text = new TEXT(255);
  nRF24init(0);
}

void timer(uint16_t time)
{
#if WTD_ENABLE
  wdt_reset();
#endif

  if(millis() - msec >= time)
  {
    msec = millis();

    DEBUG_PRINT('.');
  
    // Опрос датчиков ///
    if(sensors->SensOpros())
    {
      ALARM_ON // режим тревога вкл.
      
      if(nRFwrite(RF_CODE)) AlarmTime = 0; 
    }
    if(GET_FLAG(ALARM))
    {
      if(AlarmTime) AlarmTime--;
      else // По истечении заданного времени ALARM_MAX_TIME 
      { // Выключаем режим тревоги. Очищаем статистику датчиков.
        ALARM_OFF;            
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
