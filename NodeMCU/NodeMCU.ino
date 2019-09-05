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
 * @Last Modified by: wisenheimer 
 * @Last Modified time: 2019-09-05 16:00:00
 *************************************************************/

/* Comment this out to disable prints and save space */

#define BLYNK_PRINT Serial

#include <Wire.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiUdp.h>
#include "settings.h"
#include <time.h>

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
#define GET_SENS_VALUE(i) DEBUG_PRINT(F("sens_num="));DEBUG_PRINT(sens_num);DEBUG_PRINT(F(" i="));DEBUG_PRINTLN(i);if(sens_num){if(i>=sens_num)i=0;I2C_SEND_COMMAND(i++,I2C_SENS_VALUE);}else if(flags_num)I2C_SEND_COMMAND(i,I2C_SENS_INFO)

#define FLAGS_DELETE flags_num=0;

// Задаём максимальное количество датчиков
#define SENS_NUM_MAX  8
// Задаём максимальное количество флагов
#define FLAGS_NUM_MAX 7
// Максимальная длина имени флага или датчика, знаков
#define NAME_LEN 40

// ---- Синхронизация часов с сервером NTP
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov"; //"ru.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
unsigned int localPort = 2390;      // local port to listen for UDP packets
unsigned long epoch = 0; // время

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

// Хранит названия кнопок для вывода на экран телефона
char table_strings[FLAGS_NUM_MAX+SENS_NUM_MAX][NAME_LEN]; // список имён датчиков

float values[SENS_NUM_MAX];
bool enable[SENS_NUM_MAX];

char in_buffer[IN_BUF_SIZE];  // сюда складываются байты из шины I2C
uint8_t in_index   = 0;
uint8_t sens_index = 0;
uint8_t flags_num  = 0;       // Число флагов
uint8_t sens_num   = 0;       // Число датчиков

#define BUF_DEL_BYTES(pos,num) {in_index-=num;for(uint8_t i=pos,j=pos+num;i<in_index;i++,j++){in_buffer[i]=in_buffer[j];} \
                                memset(in_buffer+in_index,0,IN_BUF_SIZE-in_index);}
bool parser()
{
  char* p;
  uint8_t len = sizeof(i2c_sens_value)-4;
  char i2c_data[3] = {0x00,0x00,0x00};

  *((uint16_t*)i2c_data) = I2C_DATA;

  while((p=READ_COM_FIND(i2c_data))!=NULL)
  { // Показания датчика
    if((in_index-(p-in_buffer))<len) return false;
    {        
      i2c_sens_value sens_value;
      memcpy(&sens_value, p, len);

      DEBUG_PRINT(F("\ntype:"));
      DEBUG_PRINTLN(sens_value.type, HEX);
      DEBUG_PRINT(F("index:"));
      DEBUG_PRINTLN(sens_value.index);

      if(sens_value.index<sens_num)
      {
        char *pp = (char*)&(sens_value.value);
        for(uint8_t k = 3; k < len; k++)
        { 
          *pp++=*(p+k);
        }
        DEBUG_PRINT(F("value:"));
        DEBUG_PRINTLN(sens_value.value);
      
        if(values[sens_value.index] != sens_value.value)
        {
          values[sens_value.index] = sens_value.value;
          //Blynk.virtualWrite(values[sens_value.index].v_pin, values[sens_value.index]);
          table.updateRow(sens_value.index+flags_num, table_strings[sens_value.index+flags_num], values[sens_value.index]);
        }

        // enable
        DEBUG_PRINT(F("enable:"));
        DEBUG_PRINTLN(sens_value.enable);
      
        if(enable[sens_value.index] != sens_value.enable)
        {
          enable[sens_value.index] = sens_value.enable;
          terminal.print(F(SENSOR)); terminal.print(table_strings[sens_value.index+flags_num]);
          terminal.println(enable[sens_value.index] ? VKL : VIKL);
          terminal.flush();

          ROW_SELECT(sens_value.index+flags_num, enable[sens_value.index]);          
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

uint8_t read_names(char* cmd, uint8_t shift, bool selected)
{
  char* p;
  uint8_t i;
  uint8_t index = shift;
  
  p=READ_COM_FIND(cmd);

  if(p!=NULL)
  {
    p+=strlen(cmd);
          
    while(*p=='%')
    {
      i = 0; p++;
      
      while(*p && *p!='%' && *p!='\n' && i<NAME_LEN-2) table_strings[index][i++] = *p++;
      table_strings[index][i] = 0x00;
           
      table.addRow(index, table_strings[index], 0);
      ROW_SELECT(index, selected);
      index++;  
    }
    flags = 0xFF;
  }

  return index-shift;
}

uint8_t i2c_read()
{
  char inChar;
  bool start_flag = false;  // найден пакет данных
  uint8_t count = 0;
  uint8_t received = 0;     // запоминаем кол-во байт в приёмном буффере

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
      if(in_index == IN_BUF_SIZE-1) inChar = '\n';

      in_buffer[in_index++] = inChar;
      received++;

      if(inChar=='\n')
      {
        DEBUG_PRINTLN(in_buffer);
        if(!flags_num)
        {
          flags_num=read_names(I2C_FLAG, 0, 0);
        } 
        else
        if(!sens_num)
        {
          sens_num=read_names(I2C_NAME, flags_num, 1);
        }

        if(in_index==0) return received;

        parser();
          
        if(in_index==0) return received;

        char time[10];
        terminal.write('\n');
        terminal.write(printTime(time, 10));
        terminal.write("->");
        terminal.write(in_buffer);
        terminal.flush();
        
        memset(in_buffer, 0, IN_BUF_SIZE);
        in_index = 0;
      }      
    }
    else if(count==2)
    {
      //count++;
      count--;
      start_flag = true;
      // читаем flags
      if(flags != inChar)
      {
        flags = inChar;
      
        Blynk.virtualWrite(ALARM_PIN, GET_FLAG(ALARM));

        for(uint8_t k = 0; k < flags_num; k++)
        {
          table.updateRow(k, table_strings[k], GET_FLAG(k));
          ROW_SELECT(k, GET_FLAG(k));
        }
      }
    }         
  }

  DEBUG_PRINTLN();

  return received;
}

char* printTime(char* buf, uint8_t size)
{
  time_t rawtime = epoch;
  struct tm  ts;

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  ts = *localtime(&rawtime);
  strftime(buf, size, "%H:%M:%S", &ts);
  return buf;
}

// прибавляем секунду ко времени
void myTime()
{
  epoch++;
}

void myTimerEvent()
{
  uint8_t res = 0;
  // Запрашиваем данные в Ардуино
  do
  {
    if(Wire.requestFrom(ARDUINO_I2C_ADDR, 32)) res = i2c_read();    
  } while(res);
  

  GET_SENS_VALUE(sens_index)
  
  if(!flags_num)
  {
    I2C_SEND_COMMAND(0,I2C_FLAG_NAMES)
    return;
  }
  
  if(flags_num<2)
  {
    FLAGS_DELETE
    table.clear();
  }
}

// таймер синхронизации времени
void syncTimerEvent()
{
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP); 

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    DEBUG_PRINTLN(F("no packet yet"));
  }
  else {
    DEBUG_PRINT(F("packet received, length="));
    DEBUG_PRINTLN(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    DEBUG_PRINT(F("Seconds since Jan 1 1900 = " ));
    DEBUG_PRINTLN(secsSince1900);

    // now convert NTP time into everyday time:
    DEBUG_PRINT(F("Unix time = "));
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);
    
    // корректировка часового пояса и синхронизация 
    epoch = epoch + GMT * 3600;
      
   // настройка времени часов модема SIM800L
    // AT+CCLK="19/08/27,10:01:00+03"
    // print the hour, minute and second:
    DEBUG_PRINT(F("The UTC time is "));       // UTC is the time at Greenwich Meridian (GMT)

    time_t rawtime = epoch;
    struct tm  ts;
    char       buf[80];

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&rawtime);
    strftime(buf, sizeof(buf), "AT+CCLK=\"%y/%m/%d,%H:%M:%S+", &ts);
    if(GMT < 10)
    {
      strcat(buf,"0");
    }
    itoa(GMT,buf+strlen(buf),10);
    strcat(buf,"\"\n");
    SERIAL_PRINTLN(buf);
    
    I2C_WRITE(buf,strlen(buf));
  }
  // wait ten seconds before asking for the time again
}

// You can send commands from Terminal to your hardware. Just use
// the same Virtual Pin as your Terminal Widget
BLYNK_WRITE(TERMINAL_PIN)
{
  if(strstr((char*)param.getBuffer(), "#") != NULL)
  { // Это DTMF команда
    char* p = (char*)param.getBuffer();
    uint8_t i = 0;
    char buf[4];
    while(isdigit(*p) && i < 3) buf[i++] = *p++;
    buf[i] = 0;
    if(i)
    {
      i = atoi(buf);
      I2C_SEND_COMMAND(i,I2C_DTMF);
    }    
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
  timer.setInterval(SYNC_TIME_PERIOD, syncTimerEvent);
  timer.setInterval(1000, myTime);

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
    uint16_t tmp;

    if(index < flags_num)
   	{
   		tmp = flags;
   		selected ? bitSet(tmp,index) : bitClear(tmp,index);
      I2C_SEND_COMMAND(tmp,I2C_FLAGS);   		
 	  }
    else
    {
      index-=flags_num;
      tmp = (index << 8) | selected;
      I2C_SEND_COMMAND(tmp,I2C_SENS_ENABLE);      
    }
  });

	terminal.print(F("Ready "));
	terminal.print(F("IP address: "));
	terminal.println(WiFi.localIP());
	terminal.flush();

  //clean table at start
  table.clear();

  DEBUG_PRINTLN(F("Starting UDP"));
  udp.begin(localPort);
  DEBUG_PRINT(F("Local port: "));
  DEBUG_PRINTLN(udp.localPort());
  // синхронизируем время при загрузке
  syncTimerEvent();
}

void loop()
{
	Blynk.run();
	timer.run(); // Initiates BlynkTimer
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  DEBUG_PRINTLN(F("sending NTP packet..."));
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
} 

