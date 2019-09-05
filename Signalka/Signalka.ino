/*
 * Прошивка для ардуино сигнализации.
 * Перед прошивкой настроить файл
 * libraries/main_type/settings.h
 *
 * @Author: wisenheimer
 * @Date:   2019-03-29 12:30:00
 * @Last Modified by: wisenheimer  
 * @Last Modified time: 2019-09-05 16:00:00
 */

#include "modem.h"

// Продолжительность тревоги в секундах, после чего счётчики срабатываний обнуляются.
// От 0 до 255 секунд
#define ALARM_MAX_TIME 60
// активируем флаг тревоги для сбора информации и отправки e-mail
#define ALARM_ON  if(GET_FLAG(GUARD_ENABLE) && !GET_FLAG(ALARM)){SET_FLAG_ONE(ALARM);AlarmTime=ALARM_MAX_TIME;RING_TO_ADMIN(phone->admin.index,phone->admin.phone[0]) \
                  phone->email_buffer->AddText_P(PSTR(ALARM_ON_STR));sensors->GetInfo(phone->email_buffer); \
                  DEBUG_PRINTLN(phone->email_buffer->GetText());SIREN_ON}
// включение/отключение сирены
#if SIRENA_ENABLE

#define SIREN_ON      if(GET_FLAG(SIREN_ENABLE)){digitalWrite(SIREN_RELAY_PIN,SIREN_RELAY_TYPE);}if(GET_FLAG(SIREN2_ENABLE)){digitalWrite(SIREN2_RELAY_PIN,SIREN2_RELAY_TYPE);}
#define SIREN_OFF     if(GET_FLAG(SIREN_ENABLE)){digitalWrite(SIREN_RELAY_PIN,!SIREN_RELAY_TYPE);}if(GET_FLAG(SIREN2_ENABLE)){digitalWrite(SIREN2_RELAY_PIN,!SIREN2_RELAY_TYPE);}

#else

#define SIREN_ON
#define SIREN_OFF

#endif

// по окончании времени ALARM_MAX_TIME обнуляем флаг тревоги и отправляем e-mail с показаниями датчиков
#define ALARM_OFF {SET_FLAG_ZERO(ALARM);phone->email_buffer->AddText_P(PSTR(ALARM_OFF_STR));sensors->GetInfo(phone->email_buffer);sensors->Clear();SIREN_OFF}

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
static uint8_t led_Time = 0;
extern uint8_t flags;

MODEM *phone;
MY_SENS *sensors = NULL;

#if IR_ENABLE
  MY_SENS *pult_sensors = NULL;
#endif

void alarm_on()
{
  ALARM_ON
}

void door_on()
{
  phone->DTMF[0] = GUARD_ON;
}

void door_off()
{
  phone->DTMF[0] = GUARD_OFF;
}

void power()
{
  SET_FLAG_ONE(INTERRUPT);
}

// Размер массива должен равняться количеству датчиков!
# define SENS_NUM 4
// Сюда надо вписать свои датчики. Описание смотрите в 
// https://github.com/wisenheimer/Signaling-Blynk/blob/master/README.md
Sensor sens[SENS_NUM]={
  // DOOR_PIN это 4 пин
  Sensor(DOOR_PIN,  DIGITAL_SENSOR, "Дверь",       HIGH,0,200, alarm_on),
  // RADAR_PIN это 6 пин
  Sensor(RADAR_PIN, DIGITAL_SENSOR, "Движение",    LOW,0,200, alarm_on),
  Sensor(DOOR_PIN,  DS18B20,        "Температура", LOW, 10, 45, alarm_on),  
  Sensor(A7, TERMISTOR, "Термистор", LOW, 10, 45, alarm_on),

#if RF_ENABLE // Датчик с радиомодулем nRF24L01
  Sensor(RF24_SENSOR, "Окно зал",     RF0_CODE, alarm_on),
  Sensor(RF24_SENSOR, "Окно спальня", RF1_CODE, alarm_on),
#endif
};

#if IR_ENABLE
// Выполнение команд с ИК пульта управления
// Размер массива должен равняться количеству кнопок пульта!
# define PULT_NUM 2
Sensor pult[PULT_NUM]={
  //Sensor(IR_PULT, "", 0x40BFA857, door_on), // Открывает дверь
  //Sensor(IR_PULT, "", 0x40BF8877, door_off)  // Закрывает дверь
  Sensor(IR_PULT, "", 0x32A6A857, door_on), // Открывает дверь
  Sensor(IR_PULT, "", 0x32A638C7, door_off)  // Закрывает дверь
};
#endif

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

  // Пины реле
#if RELAY_ENABLE
  // Инициализируем пин включения реле открытия двери
  pinMode(OPEN_DOOR_RELAY_PIN, OUTPUT);
  digitalWrite(OPEN_DOOR_RELAY_PIN, !DOOR_RELAY_TYPE);
  // Инициализируем пин включения реле закрытия двери
  pinMode(CLOSE_DOOR_RELAY_PIN, OUTPUT);
  digitalWrite(CLOSE_DOOR_RELAY_PIN, !DOOR_RELAY_TYPE);
#endif

#if SIRENA_ENABLE
  // Инициализируем пины включения реле сирены
  pinMode(SIREN_RELAY_PIN, OUTPUT);
  digitalWrite(SIREN_RELAY_PIN, !SIREN_RELAY_TYPE);
  pinMode(SIREN2_RELAY_PIN, OUTPUT);
  digitalWrite(SIREN2_RELAY_PIN, !SIREN2_RELAY_TYPE);
  // разрешаем сирену
  SET_FLAG_ONE(SIREN_ENABLE);SET_FLAG_ONE(SIREN2_ENABLE);
#endif
  // Лампочка состояния флага охраны
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Прерывание для POWER_PIN
  attachInterrupt(1, power, CHANGE);

  if(sensors = new MY_SENS(sens, SENS_NUM))
  {
    DEBUG_PRINT(F("RAM free:"));
    DEBUG_PRINTLN(memoryFree()); // печать количества свободной оперативной памяти
  }
#if IR_ENABLE
  if(pult_sensors = new MY_SENS(pult, PULT_NUM))
  {
    DEBUG_PRINT(F("RAM free:"));
    DEBUG_PRINTLN(memoryFree()); // печать количества свободной оперативной памяти
  }
  // инициализируем ИК приёмник на 11 пине
  IRrecvInit();
#endif

  phone = new MODEM();

  phone->init();

// Wi-Fi модуль nRF24L01
#if RF_ENABLE
  nRF24init(1);
#endif

#if TM1637_ENABLE
  clock_init();
#endif
}

void timer(uint16_t time)
{
#if WTD_ENABLE
    wdt_reset();
#endif 
  if(millis() - msec >= time)
  {
#if TM1637_ENABLE
    clock();
#endif
    
    DEBUG_PRINT('.');
    msec = millis();

    /// Опрос датчиков ///
#if IR_ENABLE
    IRread();
#endif

    sensors->SensOpros();

#if IR_ENABLE
    
    pult_sensors->SensOpros();

    IRresume();
#endif

    if(GET_FLAG(ALARM))
    {
      if(!GET_FLAG(GUARD_ENABLE)) AlarmTime = 0;

      if(AlarmTime) AlarmTime--;
      else // По истечении заданного времени ALARM_MAX_TIME 
      {
        ALARM_OFF; // Выключаем режим тревоги и отправляем e-mail. Очищаем статистику датчиков.            
      }
    }

    // мигание светодиодом при постановке на охрану
    if(GET_FLAG(GUARD_ENABLE))
    {
      if(led_Time) led_Time--;
      else // По истечении заданного времени ALARM_MAX_TIME 
      {
        digitalWrite(LED_BUILTIN, HIGH);
        DELAY(20);
        digitalWrite(LED_BUILTIN, LOW);
        // учащённое мигание при тревоге
        led_Time=(GET_FLAG(ALARM)?1:LED_BLINK_PERIOD);        
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
