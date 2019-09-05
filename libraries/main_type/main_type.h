#ifndef MAIN_TYPE_H
#define MAIN_TYPE_H

#include "Arduino.h"

#if WTD_ENABLE

  #include <avr/wdt.h>
  #include <stdint.h>

  uint8_t mcusr_mirror __attribute__ ((section (".noinit")));
  void get_mcusr(void)  \
  __attribute__((naked))\
  __attribute__((used)) \
  __attribute__((section(".init3")));

  void get_mcusr(void)
  {
    mcusr_mirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
  }

#endif

#define DELAY _delay_ms

#define SERIAL_BEGIN   Serial.begin(SERIAL_RATE)
#define SERIAL_PRINT   Serial.print
#define SERIAL_PRINTLN Serial.println
#define SERIAL_PRINTF  Serial.printf
#define SERIAL_FLUSH	 Serial.flush()
#define SERIAL_WRITE	 Serial.write

//-------------------------------------------------------
// Debug directives
//-------------------------------------------------------
#if DEBUG_MODE
#	define DEBUG_PRINT		Serial.print
#	define DEBUG_PRINTLN	Serial.println
#else
#	define DEBUG_PRINT(...)
#	define DEBUG_PRINTLN(...)
#endif

#if PDU_ENABLE
# define ALARM_ON_STR  " Тревога!"
# define ALARM_OFF_STR " Конец тревоги:"
# define VKL  " вкл. "
# define VIKL " выкл. "
# define SVET  " Свет"
# define SENSOR "Датчик "
# define ALARM_NAME   "Тревога"
# define GUARD_NAME   "Охрана"
# define SIREN_NAME   "Сирена"
# define SIREN2_NAME  "Строб"
# define EMAIL_NAME   "Почта(GPRS)"
# define SMS_NAME     "СМС"
# define RING_NAME    "Звонок"
#else
# define ALARM_ON_STR  " ALARM!"
# define ALARM_OFF_STR " ALARM END:"
# define VKL  " ON. "
# define VIKL " OFF. "
# define SVET " Light"
# define SENSOR "Sensor "
# define ALARM_NAME   "ALARM"
# define GUARD_NAME   "GUARD"
# define SIREN_NAME   "SIREN"
# define SIREN2_NAME  "STROB"
# define EMAIL_NAME   "EMAIL(GPRS)"
# define SMS_NAME     "SMS"
# define RING_NAME    "RING"
#endif

//-------------------------------------------------------
// Зарезервированные номера пинов (не изменять!!!)
#define	RING_PIN  2 // отслеживает вызовы с модема
#define	POWER_PIN 3 // отслеживает наличие питания
#define	DOOR_PIN  4 // датчик двери (геркон). Один конец на GND, второй на цифровой пин arduino.
#define	BOOT_PIN  5 // перезагрузка модема
#define RADAR_PIN 6 // микроволновый датчик движения RCWL-0516
// Пины для подключения модуля RF24L01 по SPI интерфейсу
#define	CE_PIN    9
#define	CSN_PIN   10
#define	MO_PIN    11
#define	MI_PIN    12
#define	SCK_PIN   13
// Для ИК-приёмника
#define RECV_PIN  11
// Зуммер (пищалка)
#define BEEP_PIN  A2
//-------------------------------------------------------
// Команды открытия двери
//-------------------------------------------------------
#if RELAY_ENABLE
// Выключение реле
# define RELAY_OFF  {digitalWrite(CLOSE_DOOR_RELAY_PIN,!DOOR_RELAY_TYPE);digitalWrite(OPEN_DOOR_RELAY_PIN,!DOOR_RELAY_TYPE);}
// Открываем дверь
# define OPEN_DOOR  {digitalWrite(CLOSE_DOOR_RELAY_PIN,!DOOR_RELAY_TYPE);digitalWrite(OPEN_DOOR_RELAY_PIN,DOOR_RELAY_TYPE); \
            digitalWrite(LED_BUILTIN,LOW);delay(DOOR_RELAY_TIME*1000);RELAY_OFF}
// Закрываем дверь
# define CLOSE_DOOR   if(digitalRead(DOOR_PIN)){digitalWrite(OPEN_DOOR_RELAY_PIN,!DOOR_RELAY_TYPE);digitalWrite(CLOSE_DOOR_RELAY_PIN,DOOR_RELAY_TYPE); \
            delay(DOOR_RELAY_TIME*1000);RELAY_OFF}
#else

# define OPEN_DOOR
# define CLOSE_DOOR 

#endif
//-------------------------------------------------------
            
#define FLAGS uint8_t flags=0;

enum common_flag {
  ALARM,        	// флаг тревоги при срабатывании датчиков
  GUARD_ENABLE, 	// вкл/откл защиту
#if SIRENA_ENABLE
  SIREN_ENABLE,   // вкл сирену
  SIREN2_ENABLE,  // вкл сирену 2
#endif
  EMAIL_ENABLE,		// отправка отчётов по e-mail
  SMS_ENABLE,   	// отправка отчётов по sms
  RING_ENABLE,  	// включает/выключает звонки  
  INTERRUPT    	// прерывание
};

#define CONNECT_ALWAYS 1 // не разрывать соединение после отправки e-mail

#define INV_FLAG(buf,flag)  (buf^=(flag)) // инвертировать бит

#define GET_FLAG(flag)      bitRead(flags,flag)
#define SET_FLAG_ONE(flag)  bitSet(flags,flag)
#define SET_FLAG_ZERO(flag) bitClear(flags,flag)
#define INVERT_FLAG(flag)   INV_FLAG(flags,1<<flag)

#define ARDUINO_I2C_ADDR 1
#define I2C_NAME		    "SENS:"
#define I2C_FLAG        "FLAG:"
#define I2C_DATA        0x9090
#define I2C_SENS_VALUE 	0x01
#define I2C_FLAGS       0x02
#define I2C_SENS_INFO 	0x03
#define I2C_FLAG_NAMES  0x04
#define I2C_DTMF        0x05
#define I2C_SENS_ENABLE 0x06

#define START_BYTE		  0x91
#define END_BYTE		    0x92

#define FLAG_COUNT	2

struct i2c_cmd
{
  uint16_t type;
  uint16_t index;
  uint8_t cmd;  
};

struct i2c_sens_value
{
  uint16_t type;
  uint8_t index;
  float value;
  bool enable; // флаг включения/отключения датчиков
};

#endif // MAIN_TYPE_H
