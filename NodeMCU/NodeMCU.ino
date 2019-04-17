/*************************************************************
  Download latest Blynk library here:
    https://github.com/blynkkk/blynk-library/releases/latest

  Blynk is a platform with iOS and Android apps to control
  Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build graphic interfaces for all your
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: http://www.blynk.cc
    Sketch generator:           http://examples.blynk.cc
    Blynk community:            http://community.blynk.cc
    Follow us:                  http://www.fb.com/blynkapp
                                http://twitter.com/blynk_app

  Blynk library is licensed under MIT license
  This example code is in public domain.

 *************************************************************
  This example runs directly on NodeMCU.

  Note: This requires ESP8266 support package:
    https://github.com/esp8266/Arduino

  Please be sure to select the right NodeMCU module
  in the Tools -> Board menu!

  For advanced settings please follow ESP examples :
   - ESP8266_Standalone_Manual_IP.ino
   - ESP8266_Standalone_SmartConfig.ino
   - ESP8266_Standalone_SSL.ino

  Change WiFi ssid, pass, and Blynk auth token to run :)
  Feel free to apply it to any other example. It's simple!

  Wi-Fi модем Blynk.
 * 
 * Для включения/отключения вывода отладочных сообщений
 * в Serial отредактируйте файл
 * libraries/main_type/settings.h
 *
 * Прошивать как есть
 * @Author: wisenheimer
 * @Date:   2019-04-17 8:30:00
 * @Last Modified by:   
 * @Last Modified time:
 *************************************************************/

/* Comment this out to disable prints and save space */

#define BLYNK_PRINT Serial

#include <Wire.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>
#include "settings.h"

// Виртуальные пины виджетов
#define TERMINAL_PIN  V0
#define TABLE_PIN     V1
#define ALARM_PIN     V2

// размер in_buffer
#define IN_BUF_SIZE 255

#define ROW_SELECT(index, selected) Blynk.virtualWrite(TABLE_PIN, selected ? "select" : "deselect", index);

#define I2C_WRITE(buf,len) {Wire.beginTransmission(ARDUINO_I2C_ADDR);Wire.write(buf,len);Wire.endTransmission();}

#define EMAIL(text) if(GET_FLAG(EMAIL_ENABLE))Blynk.email("","ESP8266",text)

#define READ_COM_FIND(str) strstr(in_buffer,str)

#define PRINT_IN_BUFFER(str)  Serial.println();Serial.print(str);Serial.print(in_buffer);for(uint8_t i=0;i<in_index;i++){\
                              Serial.print(' ');Serial.print(*(in_buffer+i),HEX);}Serial.println();

#define I2C_SET_COMMAND(str,i,command) {str.type=I2C_DATA;str.index=i;str.cmd=command;}
#define I2C_SEND_COMMAND(index,cmd) {struct i2c_cmd str;I2C_SET_COMMAND(str,index,cmd);I2C_WRITE((uint8_t*)&str,sizeof(str));}
#define GET_SENS_VALUE(i) if(GET_FLAG(GUARD_ENABLE))if(sens_num){if(i>=sens_num)i=0;I2C_SEND_COMMAND(i++,I2C_SENS_VALUE);}else if(flags_num)I2C_SEND_COMMAND(i,I2C_SENS_INFO)

#define FLAGS_DELETE flags_num=0;
#define SENS_DELETE  while(sens_num){sens_num--;delete[] sensors[sens_num].name;}

// Задаём максимальное количество датчиков
#define SENS_NUM_MAX  10
// Задаём максимальное количество флагов
#define FLAGS_NUM_MAX 6

struct{
  char ssid[32];
  char pass[32];
  char token[33];
} wifi_data;

// Attach virtual serial terminal to Virtual Pin
WidgetTerminal terminal(TERMINAL_PIN);

WidgetTable table;
BLYNK_ATTACH_WIDGET(table, TABLE_PIN);

BlynkTimer timer;

FLAGS

// Названия флагов. Эти строки будут выведены на экран телефона
char table_flags[FLAGS_NUM_MAX][20]; // список имён датчиков

struct sens{
  char* name;
  float value;
} sensors[SENS_NUM_MAX];

char in_buffer[IN_BUF_SIZE]; // сюда складываются байты из шины I2C
uint8_t in_index = 0;
uint8_t sens_index = 0;
uint8_t flags_num = 0; // Число флагов
uint8_t sens_num = 0; // Число датчиков

#define BUF_DEL_BYTES(pos,num) {in_index-=num;for(uint8_t i=pos,j=pos+num;i<in_index;i++,j++){in_buffer[i]=in_buffer[j];} \
                                memset(in_buffer+in_index,0,IN_BUF_SIZE-in_index);}
bool parser()
{
  char* p;
  uint8_t len = sizeof(i2c_sens_value)-1;
  char i2c_data[3] = {0x00,0x00,0x00};

  *((uint16_t*)i2c_data) = I2C_DATA;

  while((p=READ_COM_FIND(i2c_data))!=NULL)
  { // Показания датчика
    if((in_index-(p-in_buffer))<len) return false;
    {        
      i2c_sens_value sens_value;
      memcpy(&sens_value, p, len);

      DEBUG_PRINT(F("sens_type:"));
      DEBUG_PRINTLN(sens_value.type, HEX);
      DEBUG_PRINT(F("sens_index:"));
      DEBUG_PRINTLN(sens_value.index);

      if(sens_value.index<sens_num)
      {
        char *pp = (char*)&(sens_value.value);
        for(uint8_t k = 3; k < len; k++)
        { 
          *pp++=*(p+k);
        }
        DEBUG_PRINT(F("sens_val:"));
        DEBUG_PRINTLN(sens_value.value);
      
        if(sensors[sens_value.index].value != sens_value.value)
        {
          sensors[sens_value.index].value = sens_value.value;
          //Blynk.virtualWrite(values[sens_value.index].v_pin, values[sens_value.index]);
          table.updateRow(sens_value.index+flags_num, sensors[sens_value.index].name, sensors[sens_value.index].value);
        }              
      }  
#if DEBUG_MODE
      PRINT_IN_BUFFER(F("START:"))
#endif      
      BUF_DEL_BYTES(p-in_buffer,len)

#if DEBUG_MODE
      PRINT_IN_BUFFER(F("END:"))
#endif     
    }  
  }

  return true; 
}

uint8_t flags_read_names(char* cmd, uint8_t shift)
{
  char* p;
  uint8_t i;
  uint8_t index = 0;
  
  char* pos=READ_COM_FIND(cmd);

  if(pos!=NULL)
  {
    p=pos+strlen(cmd);
              
    while(*p==' ')
    {
      i = 0;
      p++;
      while(isprint(*p) && *p!=' ' && i<19) table_flags[index][i++] = *p++;
      table_flags[index][i++] = 0;
         
      table.addRow(shift+index, table_flags[index], 0);
      ROW_SELECT(shift+index, 0);
      index++;      
    }
  }

  return index;
}

uint8_t sens_read_names(char* cmd, uint8_t shift)
{
  char* p;
  uint8_t i;
  uint8_t index = 0;
  char buf[32];

  char* pos=READ_COM_FIND(cmd);

  if(pos!=NULL)
  {
    p=pos+strlen(cmd);
              
    while(*p==' ')
    {
      i = 0;
      p++;
      while(isprint(*p) && *p!=' ' && i<31) buf[i++] = *p++;
      buf[i++] = 0;
                
      if ( (sensors[index].name = (char*)malloc(strlen(buf) + 1) )== NULL )
      {
        terminal.print("error: malloc");
        terminal.flush();
        exit(1);
      }
      
      strcpy(sensors[index].name, buf);
      table.addRow(shift+index, buf, 0);
      ROW_SELECT(shift+index, 0);
      index++;      
    }
  }

  return index;
}

void i2c_read()
{
  char inChar;
  bool start_flag = false; // найден пакет данных
  uint8_t count = 0;

  while (Wire.available()) {
    // получаем новый байт:
    inChar = (char)Wire.read();

    DEBUG_PRINT(' ');DEBUG_PRINT(inChar);DEBUG_PRINT('|');DEBUG_PRINT(inChar,HEX);

    if(inChar == START_BYTE)
    {
      count++;
      continue; 
    }

    if(inChar == END_BYTE)
    {
      count--;
      if(count == 0)
      {
        start_flag = false;

        if(!flags_num) flags_num=flags_read_names(I2C_FLAG, 0);
        else
        if(!sens_num)
        {
          sens_num=sens_read_names(I2C_NAME, flags_num);
          //if(sens_num) values = new float [sens_num];
        }        
        
        parser();
#if DEBUG_MODE
        PRINT_IN_BUFFER(F("PRINT:"))
#endif
        if(in_index==0) return;
        terminal.write(in_buffer);
        terminal.flush();
        EMAIL(in_buffer);
        memset(in_buffer, 0, IN_BUF_SIZE);
        in_index = 0;        
      } 
      continue;      
    }
    
    if(start_flag)
    {
      if(in_index < IN_BUF_SIZE) in_buffer[in_index++] = inChar;      
    }
    else if(count==2)
    {
      start_flag = true;
      // читаем flags
      if(flags != inChar)
      {
        flags = inChar;
      
        Blynk.virtualWrite(ALARM_PIN, GET_FLAG(ALARM));

        if(GET_FLAG(GUARD_ENABLE)==0) SENS_DELETE

        for(uint8_t k = 0; k < flags_num; k++)
        {
          table.updateRow(k, table_flags[k], GET_FLAG(k));
          ROW_SELECT(k, GET_FLAG(k));
        }
      }
    }        
  }

  DEBUG_PRINTLN();
}

void myTimerEvent()
{
  // Запрашиваем данные в Ардуино
  if(Wire.requestFrom(ARDUINO_I2C_ADDR, 32))
  {
    i2c_read();    
  }
  GET_SENS_VALUE(sens_index)
  if(!flags_num)
  {
    I2C_SEND_COMMAND(0,I2C_FLAG_NAMES)
    return;
  } 

  if(flags_num<3)
  {
    FLAGS_DELETE
    SENS_DELETE
    table.clear();
  }
}

void send_dtmf(char* buf)
{
  while(*buf)
  {        
    Wire.beginTransmission(ARDUINO_I2C_ADDR);
    Wire.write("DTMF: ");
    Wire.write(*buf++);
    Wire.write('\n');
    Wire.endTransmission();
    delay(200);
  }  
}

// You can send commands from Terminal to your hardware. Just use
// the same Virtual Pin as your Terminal Widget
BLYNK_WRITE(TERMINAL_PIN)
{
  if(strstr((char*)param.getBuffer(), "#") != NULL)
  { // Это DTMF команда
    send_dtmf((char*)param.getBuffer());
  }
  else
  {
    I2C_WRITE((char*)param.getBuffer(), param.getLength());
  }
}

void enter(char* dst, char* name, char* buffer)
{
  char* p;
  uint8_t i;

  if((p=strstr(buffer,name))!=NULL)
  {
    p+=strlen(name);
    if(*p=='=')
    {
      p++;
      i = 0;
      while(*p && *p!=13) dst[i++] = *p++;
      dst[i] = 0;
      EEPROM.put(0,wifi_data);
      EEPROM.commit();
    }
    EEPROM.get(0,wifi_data);
    Serial.printf("%s: %s\n",name,dst);
  }
}

// Ввод данных через терминал
uint8_t read_com()
{
  char inChar;
  char buffer[64];
  uint8_t index = 0;
  
  while (Serial.available()) {
    // получаем новый байт:
    inChar = (char)Serial.read();    
  
    // если получили символ новой строки, оповещаем программу об этом,
    // чтобы она могла принять дальнейшие действия.
    if (inChar == '\n')
    {
      if(index)
      {
        enter(wifi_data.ssid, (char*)"SSID", buffer);
        enter(wifi_data.pass, (char*)"PASS", buffer);
        enter(wifi_data.token, (char*)"TOKEN", buffer);        
      }
      memset(buffer, 0, 64);
      index = 0;      
    }
    else
    { // добавляем его:
      buffer[index++] = inChar;
    }
  }  

  return index;
}

void setup()
{
  flags = 0;
	
  Wire.begin(D1, D2);

  // Debug console
	SERIAL_BEGIN;

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into 
  // a 512-byte-array cache in RAM
  EEPROM.begin(sizeof(wifi_data));
  
  delay(2000);

  // Попадаем сюда при удержании кнопки FLASH во время старта
  if(digitalRead(D3)==LOW)
  {
    // Выодим данные Wi-Fi точки и токен Blynk 
    Serial.println(F("\nEnter SSID like SSID=your_ssid"));
    Serial.println(F("Enter Password like PASS=your_password"));
    Serial.println(F("Enter TOKEN like TOKEN=your_token"));

    while(1)
    {
      delay(3000);
      read_com();
    }
  }

  EEPROM.get(0,wifi_data);

  DEBUG_PRINTLN("\nSSID: "+String(wifi_data.ssid)+"\nPASS: "+String(wifi_data.pass)+"\nTOKEN: "+String(wifi_data.token));
 
 	Blynk.begin(wifi_data.token, wifi_data.ssid, wifi_data.pass);

  // This will print Blynk Software version to the Terminal Widget when
  // your hardware gets connected to Blynk Server
 	terminal.println("Blynk v" BLYNK_VERSION ": Device started");

  // Setup a function to be called every second
 	timer.setInterval(2000L, myTimerEvent);

  // Setup table event callbacks
 	table.onOrderChange([](int indexFrom, int indexTo)
 	{
   	terminal.print("Reordering: ");
   	terminal.print(indexFrom);
   	terminal.print(" => ");
   	terminal.print(indexTo);
    terminal.flush();
 	});

 	table.onSelectChange([](int index, bool selected)
 	{
    if(index < flags_num)
   	{
   		uint8_t tmp = flags;

   		selected ? SET_FLAG_ONE(index) : SET_FLAG_ZERO(index);
      I2C_SEND_COMMAND(flags,I2C_FLAGS);   		
   		flags = tmp;
 	  }
    else
    {
      index-=flags_num;
      GET_SENS_VALUE(index)
    }
  });

	terminal.print(F("Ready "));
	terminal.print(F("IP address: "));
	terminal.println(WiFi.localIP());
	terminal.flush();

  //clean table at start
  table.clear();
}

void loop()
{
	Blynk.run();
	timer.run(); // Initiates BlynkTimer
}
