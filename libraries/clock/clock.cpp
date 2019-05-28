#include "TM1637.h"  // http://www.seeedstudio.com/wiki/File:DigitalTube.zip
#include "settings.h"    

TM1637 tm1637(CLK,DIO);  
  
boolean flag;

byte second = 0, minute = 0, hour = 0;

void clock_set(byte time[])
{
  hour = time[0];
  minute = time[1];
  second = time[2];
}

void clock_init() {
  tm1637.init();
  tm1637.set(brightness);  
}

void clock()
{
  int8_t TimeDisp[4];

  second++; 
  if (second > 59)
  {
    second = 0;
    minute++;
  }
  if (minute > 59)
  {
    minute = 0;
    hour++;
  }
  if (hour > 23)
  {
    hour = 0;
  }

  flag = !flag;
  tm1637.point(flag);

  // забиваем массив значениями для отпарвки на экран
    
  TimeDisp[0] = hour / 10;
  TimeDisp[1] = hour % 10;
  TimeDisp[2] = minute / 10;
  TimeDisp[3] = minute % 10;

  // отправляем массив на экран
  tm1637.display(TimeDisp);
}