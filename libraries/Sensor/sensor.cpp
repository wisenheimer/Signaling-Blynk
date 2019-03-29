#include "sensor.h"
#include "termistor.h"

Sensor::Sensor( // типы датчиков перечислены в "sensor.h"
                uint8_t _type,
                // имя датчика
                char* sens_name,
                // ик код
                uint32_t ir_code)
{
  type = _type;
  name = (char*)malloc(strlen(sens_name)+1);
  strcpy(name, sens_name);
  alarm_value = ir_code;
  count = 0;
}

Sensor::Sensor( // пин ардуино
                uint8_t _pin,
                // типы датчиков перечислены в "sensor.h"
                uint8_t _type,
                // имя датчика
                char* sens_name,
                // уровень на пине датчика в спокойном режиме (LOW или HIGHT)
                uint8_t pinLevel = LOW,
                // время на подготовку датчика при старте
                uint8_t start_time_sec = 10,
                // значение срабатывания аналогового датчика
                uint32_t alarm_val = 200)
{
  pin = _pin;
  type = _type;
  name = (char*)malloc(strlen(sens_name)+1);
  strcpy(name, sens_name);
  
  start_time = start_time_sec;
  end_time   = start_time_sec;
    
  alarm_value = alarm_val;
  count = 0;
  check = false;

  if(
#if DHT_ENABLE
  (type >= DHT11 && type <= DHT22) ||
#endif
  type == TERMISTOR || type == DS18B20) return;

  pinMode(pin, INPUT);
  digitalWrite(pin, LOW);
  level = pinLevel;
  prev_pin_state = pinLevel;   
}

Sensor::~Sensor()
{
  free(name);
}

bool Sensor::get_pin_state()
{
  bool state = digitalRead(pin);

  if(level!=state)
  {
    if(level==prev_pin_state)
    {
      // если сенсор сработал, исключаем ложное срабатывание датчика
      if(type==CHECK_DIGITAL_SENSOR)
      {
        start_time = 10; // опросим повторно через 10 сек
        check = true;
      } 
      else count++;
    }
    else if(check)
    {
      check = false;
      count++;
    }  
  }

  prev_pin_state = state;
  
  return state;
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

uint8_t Sensor::get_count()  
{
  switch (type)
  {
#if IR_ENABLE
    case IR_SENSOR:
      value = IRgetValue();
      if(alarm_value == value) count++;
      break;
#endif

#if RF_ENABLE
    case RF24_SENSOR:
      value = nRFgetValue();
      if(alarm_value == value)
      {
        count++;
        nRFclear();
      }
      break;
#endif

    case DIGITAL_SENSOR:
    case CHECK_DIGITAL_SENSOR:
      value = get_pin_state();
      break;
    default:
      start_time = 10;

      // если показание аналогового датчика превысило пороговое значение
#if DHT_ENABLE
      
      if(type >= DHT11 && type <= DHT22)
      {
        dht = new DHT(type, pin);
        value = dht->readTemperature();
        delete dht;
      }
      else
#endif

#if DS18_ENABLE
      if(type == DS18B20)
      {
        OneWire oneWire(pin);
        // Pass our oneWire reference to Dallas Temperature. 
        DallasTemperature ds18b20(&oneWire);
        // arrays to hold device addresses
        DeviceAddress Thermometer;
        ds18b20.begin();  
        // search for devices on the bus and assign based on an index.
        if (!ds18b20.getAddress(Thermometer, 0))
        {
          DEBUG_PRINTLN(F("Did`t find address ds18b20"));
          return count;
        }  
        // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
        ds18b20.setResolution(Thermometer, 9);
        ds18b20.requestTemperatures(); // Send the command to get temperatures
        float tempC = ds18b20.getTempC(Thermometer);
        if(tempC == -127) return count;
        value = tempC;
      }
      else
#endif

      value = analogRead(pin);

#if TERM_ENABLE
      if(type == TERMISTOR) TERM_ADC_TO_C(value,value)
#endif
        
      if(value >= alarm_value) count++;
  }   
    
  return count;
}

// Записываем в строку показания датчика
void Sensor::get_info(TEXT *str)
{
  str->AddText(name);
  str->AddChar(':');

  switch (type)
  {
    case ANALOG_SENSOR:
      str->AddInt(value);
      break;
    
#if DHT_ENABLE
    case DHT11:
    case DHT21:
    case DHT22:
#endif
    case TERMISTOR:
    case DS18B20:
      str->AddText_P(PSTR("t="));
      str->AddInt(value);
      str->AddChar('C');
      break;
    default:
      // добавляем число срабатываний датчика
      str->AddInt(count);
#if (!RF_ENABLE) && (!RF_ENABLE)
      str->AddChar('(');
      // добавляем текущее состояние пина (0 или 1)
      str->AddChar(value + '0');
      str->AddChar(')');
#endif
  }
  str->AddChar(' ');
}
