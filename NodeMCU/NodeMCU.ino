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
uint8_t wifi_mode = 0;

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

// Задаём максимальное количество датчиков
#define SENS_NUM_MAX  10
// Число флагов, выводимых на экран
uint8_t FLAGS_NUM;
// Названия флагов. Эти строки будут выведены на экран телефона
char* table_flags[] = {
  "ALARM",
  "GUARD",
  "EMAIL",
// Строки для SIM800L. Параметр MODEM_ENABLE задаётся в settings.h
#if MODEM_ENABLE
  "GPRS",
  "SMS",
  "RING"
#endif
};

char* table_sens[SENS_NUM_MAX]; // список имён датчиков
float values[SENS_NUM_MAX]; // хранит последнее значение датчика

char in_buffer[IN_BUF_SIZE]; // сюда складываются байты из шины I2C
uint8_t in_index = 0;
uint8_t sens_index = 0;
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
      
        if(values[sens_value.index] != sens_value.value)
        {
          values[sens_value.index] = sens_value.value;
          //Blynk.virtualWrite(values[sens_value.index].v_pin, values[sens_value.index].value);
          table.updateRow(sens_value.index+FLAGS_NUM, table_sens[sens_value.index], values[sens_value.index]);
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
        parser();
      } 
      continue;      
    }
    
    if(start_flag)
    {
      in_buffer[in_index++] = inChar;
      if(inChar == '\n' || in_index==IN_BUF_SIZE)
      {
        char* pos=READ_COM_FIND(I2C_NAME);
        
        if(pos!=NULL)
        {
          char* p=pos;
          uint8_t num = 0;
          if(!sens_num)
          {   
            num=strlen(I2C_NAME);
            p+=num;
            char buf[32];            
            while(*p==' ' && sens_num<SENS_NUM_MAX)
            {
              uint8_t i = 0;
              p++;
              while(isprint(*p) && *p!=' ' && i<31) buf[i++] = *p++;
              buf[i++] = 0;
              num+=i;
              if(table_sens[sens_num]!=NULL)
              {
                delete[] table_sens[sens_num];
              }        
              table_sens[sens_num] = (char*)malloc(strlen(buf)+1);
              strcpy(table_sens[sens_num], buf);
              table.addRow(FLAGS_NUM+sens_num, buf, 0);
              sens_num++;
            }
          }
          else num = in_index;
          //BUF_DEL_BYTES(in_buffer-pos, num);
        }
        if(!parser()) continue;
#if DEBUG_MODE
        PRINT_IN_BUFFER(F("PRINT:"))
#endif
        terminal.write(in_buffer);
        terminal.flush();
        EMAIL(in_buffer);
        memset(in_buffer, 0, IN_BUF_SIZE);
        in_index = 0;
      }
    }
    else if(count==2)
    {
      start_flag = true;
      // читаем flags
      if(flags != inChar)
      {
        flags = inChar;
      
        Blynk.virtualWrite(ALARM_PIN, GET_FLAG(ALARM));

        if(GET_FLAG(GUARD_ENABLE)==0)
        {
          while(sens_num--){
             values[sens_num] = 0;
          }          
        }

        for(uint8_t k = 0; k < FLAGS_NUM; k++)
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
  if(Wire.requestFrom(ARDUINO_I2C_ADDR, BUFFER_LENGTH))
  {
    i2c_read();
  }    
}

#define I2C_SET_COMMAND(str,i,command) {str.type=I2C_DATA;str.index=i;str.cmd=command;}
#define I2C_SEND_COMMAND(index,cmd) {struct i2c_cmd str;I2C_SET_COMMAND(str,index,cmd);I2C_WRITE((uint8_t*)&str,sizeof(str));}
#define GET_SENS_VALUE(i) if(GET_FLAG(GUARD_ENABLE))if(sens_num){if(i>=sens_num)i=0;I2C_SEND_COMMAND(i++,I2C_SENS_VALUE);}else I2C_SEND_COMMAND(i,I2C_SENS_INFO);

// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
void SensEvent()
{
  GET_SENS_VALUE(sens_index); 
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
	uint8_t i;
	
  Wire.begin(D1, D2);

  FLAGS_NUM = sizeof(table_flags)/sizeof(table_flags[0]);

  // Debug console
	SERIAL_BEGIN;

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into 
  // a 512-byte-array cache in RAM
  EEPROM.begin(sizeof(wifi_data));
  
  delay(2000);

  if(digitalRead(D3)==LOW)
  {
    Serial.println(F("\nEnter SSID like SSID=your_ssid"));
    Serial.println(F("Enter Password like PASS=your_password"));
    Serial.println(F("Enter TOKEN like TOKEN=your_token"));
    //memset(&wifi_data, 0, sizeof(wifi_data));
    while(1)
    {
      delay(3000);
      read_com();
    }
  }

  EEPROM.get(0,wifi_data);

  DEBUG_PRINTLN("\nSSID: "+String(wifi_data.ssid)+"\nPASS: "+String(wifi_data.pass)+"\nTOKEN: "+String(wifi_data.token));
 
 	Blynk.begin(wifi_data.token, wifi_data.ssid, wifi_data.pass);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 80);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);

  // This will print Blynk Software version to the Terminal Widget when
  // your hardware gets connected to Blynk Server
 	terminal.println("Blynk v" BLYNK_VERSION ": Device started");

  	// Setup a function to be called every second
 	timer.setInterval(2000L, myTimerEvent);
  timer.setInterval(1000L, SensEvent);

  	// Setup table event callbacks
 	table.onOrderChange([](int indexFrom, int indexTo)
 	{
   	terminal.print("Reordering: ");
   	terminal.print(indexFrom);
   	terminal.print(" => ");
   	terminal.print(indexTo);
    terminal.flush();
 	});

  for(uint8_t i = 0; i < SENS_NUM_MAX; i++) table_sens[i] = NULL;

 	table.onSelectChange([](int index, bool selected)
 	{
    if(index < FLAGS_NUM)
   	{
   		uint8_t tmp = flags;

   		selected ? SET_FLAG_ONE(index) : SET_FLAG_ZERO(index);  
   		I2C_SEND_COMMAND(flags,I2C_FLAGS);   		
   		flags = tmp;
 	  }
    else
    {
      index-=FLAGS_NUM;
      DEBUG_PRINT(F("index="));
      DEBUG_PRINTLN(index);
      GET_SENS_VALUE(index);
    }
  });

	//clean table at start
  table.clear();

  // adding rows to table
  for(i = 0; i < FLAGS_NUM; i++)
  {
   	table.addRow(i, table_flags[i], GET_FLAG(i));
   	ROW_SELECT(i, GET_FLAG(i));
	}
	terminal.print(F("Ready "));  //  "Готово"
	terminal.print(F("IP address: "));  //  "IP-адрес: "
	terminal.println(WiFi.localIP());
	terminal.flush();

  EMAIL("Signalka is online.");
}

void loop()
{
	Blynk.run();
	timer.run(); // Initiates BlynkTimer  	
}
