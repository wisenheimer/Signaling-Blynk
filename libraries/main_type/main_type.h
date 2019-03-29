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

//-------------------------------------------------------
// Зарезервированные номера пинов (не изменять!!!)
#define	RING_PIN  2 // отслеживает вызовы с модема
#define	POWER_PIN 3 // отслеживает наличие питания
#define	DOOR_PIN  4 // датчик двери (геркон). Один конец на GND, второй на цифровой пин arduino.
#define	BOOT_PIN  5 // перезагрузка модема
// Пины для подключения модуля RF24L01 по SPI интерфейсу
#define	CE_PIN    9
#define	CSN_PIN   10
#define	MO_PIN    11
#define	MI_PIN    12
#define	SCK_PIN   13
// Для ИК-приёмника
#define RECV_PIN  11
//-------------------------------------------------------

#define FLAGS uint8_t flags=0;

enum common_flag {
  ALARM,        	// флаг тревоги при срабатывании датчиков
  GUARD_ENABLE, 	// вкл/откл защиту
  EMAIL_ENABLE,		// отправка отчётов по e-mail
  CONNECT_ALWAYS,	// не разрывать соединение после отправки e-mail
  SMS_ENABLE,   	// отправка отчётов по sms
  RING_ENABLE,  	// включает/выключает звонки  
  INTERRUPT,    	// прерывание
  MODEM_NEED_INIT
};

#define INV_FLAG(buf,flag)  (buf^=(flag)) // инвертировать бит

#define GET_FLAG(flag)      bitRead(flags,flag)
#define SET_FLAG_ONE(flag)  bitSet(flags,flag)
#define SET_FLAG_ZERO(flag) bitClear(flags,flag)
#define INVERT_FLAG(flag)   INV_FLAG(flags,1<<flag)

#define ALARM_BEGIN " ALARM!"
#define ALARM_END	" ALL:"

#define ARDUINO_I2C_ADDR 	1
#define I2C_NAME	     "NAME:"
#define I2C_DATA			 0x9090
#define I2C_SENS_VALUE 0x01
#define I2C_FLAGS		   0x02
#define I2C_SENS_INFO  0x03

#define START_BYTE	   0x91
#define END_BYTE	     0x92

#define FLAG_COUNT	2

struct i2c_cmd
{
  uint16_t type;
  uint8_t index;
  uint8_t cmd;  
};

struct i2c_sens_value
{
  uint16_t type;
  uint8_t index;
  float value;
};

#endif // MAIN_TYPE_H
