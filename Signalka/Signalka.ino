/*
 * Прошивка для ардуино сигнализации.
 * Перед прошивкой настроить файл
 * https://github.com/wisenheimer/Signaling-Blynk/blob/master/libraries/main_type/settings.h
 *
 * @Author: wisenheimer
 * @Date:   2019-03-29 12:30:00
 * @Last Modified by:   
 * @Last Modified time:
 */

#include "modem.h"

// Переменные, создаваемые процессом сборки,
// когда компилируется скетч
extern int __bss_end;
extern void *__brkval;

// Функция, возвращающая количество свободного ОЗУ (RAM)
int memoryFree()
{
   int freeValue;
   if((int)__brkval == 0)
      freeValue = ((int)&freeValue) - ((int)&__bss_end);
   else
      freeValue = ((int)&freeValue) - ((int)__brkval);
   return freeValue;
}

static uint32_t msec = 0;
static uint8_t AlarmTime = 0;
extern uint8_t flags;

MODEM *phone;
MY_SENS *sensors = NULL;

// Размер массива должен равняться количеству датчиков!
# define SENS_NUM 4
// Сюда надо вписать свои датчики. Описание смотрите в 
// https://github.com/wisenheimer/Signaling-Blynk/blob/master/README.md
Sensor sens[SENS_NUM]={
  Sensor(DOOR_PIN, DIGITAL_SENSOR,"DOOR", HIGH,0),
  Sensor(DOOR_PIN, DS18B20,       "18B20",LOW, 10, 45),
  Sensor(A7,       TERMISTOR,     "TERM", LOW, 10, 45),
  Sensor(6,        DIGITAL_SENSOR,"RADAR",LOW),
#if RF_ENABLE // Датчик с Wi-Fi модулем nRF24L01
  Sensor(          RF24_SENSOR,   "nRF_0",RF0_CODE),
  Sensor(          RF24_SENSOR,   "nRF_1",RF1_CODE)
#endif
};

#define ALARM_MAX_TIME 60 // продолжительность тревоги в секундах, после чего счётчики срабатываний обнуляются
// активируем флаг тревоги для сбора информации и отправки e-mail
#define ALARM_ON  if(!GET_FLAG(ALARM)){SET_FLAG_ONE(ALARM);AlarmTime=ALARM_MAX_TIME;RING_TO_ADMIN(phone->admin.index) \
                  phone->email_buffer->AddText_P(PSTR(" ALARM!"));sensors->GetInfo(phone->email_buffer); \
                  DEBUG_PRINTLN(phone->email_buffer->GetText());}
// по окончании времени ALARM_MAX_TIME обнуляем флаг тревоги и отправляем e-mail с показаниями датчиков
#define ALARM_OFF {SET_FLAG_ZERO(ALARM);if(GET_FLAG(GUARD_ENABLE)){phone->email_buffer->AddText_P(PSTR(" ALL:")); \
                  sensors->GetInfo(phone->email_buffer);sensors->Clear();}}

void power()
{
  SET_FLAG_ONE(INTERRUPT);
}

void setup()
{
#if WTD_ENABLE
  wdt_disable();
  wdt_enable(WDTO_8S);
#endif

  pinMode(RING_PIN, INPUT);
  digitalWrite(RING_PIN, LOW);
  pinMode(POWER_PIN, INPUT);
  digitalWrite(POWER_PIN, LOW);
  pinMode(BOOT_PIN, OUTPUT);
  digitalWrite(BOOT_PIN, HIGH);

  // Прерывание для POWER_PIN
  attachInterrupt(1, power, CHANGE);

  phone = new MODEM();

  phone->init();

  if(sensors = new MY_SENS(sens, SENS_NUM))
  {
    DEBUG_PRINT(F("RAM free:"));
    DEBUG_PRINTLN(memoryFree()); // печать количества свободной оперативной памяти
  }

// Wi-Fi модуль nRF24L01
#if RF_ENABLE
  nRF24init(1);
#endif

// ИК- приёмник
#if IR_ENABLE
  IRrecvInit();
#endif
}

void timer(uint16_t time)
{
#if WTD_ENABLE
    wdt_reset();
#endif 
  if(millis() - msec >= time)
  {
    DEBUG_PRINT('.');
    msec = millis();

    if(GET_FLAG(GUARD_ENABLE))
    { /// Опрос датчиков ///
      if(sensors->SensOpros())
      {
        ALARM_ON // режим тревога вкл.                            
      }       
    }
    if(GET_FLAG(ALARM))
    {
      if(AlarmTime) AlarmTime--;
      else // По истечении заданного времени ALARM_MAX_TIME 
      {
        ALARM_OFF; // Выключаем режим тревоги и отправляем e-mail. Очищаем статистику датчиков.            
      }
    }        
  }
}

void loop()
{
  while(1)
  { 
    phone->wiring(); // слушаем модем
    timer(1000);
  }    
}
