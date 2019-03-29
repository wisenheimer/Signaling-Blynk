#include <SPI.h>
#include "settings.h"
#include "nRF24.h"

#define RF_ADRESS (byte*)"1Node" // номер трубы
#define nRF24_CHANEL 0x60 // свободный Wi-Fi канал (в котором нет шумов!)

RF24 radio(CE_PIN,CSN_PIN); // "создать" модуль на пинах 9 и 10 Для Уно

byte gotByte = 0;

void nRF24init(byte mode)
{
  radio.begin();            //активировать модуль
  radio.setAutoAck(1);      //режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(0,15);   //(время между попыткой достучаться, число попыток)
  radio.enableAckPayload(); //разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32); //размер пакета, в байтах

  if(mode)
  {
    radio.openReadingPipe(1,RF_ADRESS);  //хотим слушать трубу RF_ADRESS  
  }
  else
  {
    radio.openWritingPipe(RF_ADRESS);  //мы - труба RF_ADRESS, открываем канал для передачи данных
  }
  radio.setChannel(nRF24_CHANEL); //выбираем канал (в котором нет шумов!)

  radio.setPALevel (RF24_PA_MAX); //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate (RF24_1MBPS); //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!
  // ВНИМАНИЕ!!! enableAckPayload НЕ РАБОТАЕТ НА СКОРОСТИ 250 kbps!
  
  radio.powerUp(); //начать работу
  
  if(mode)
  {
    radio.startListening();  //начинаем слушать эфир, мы приёмный модуль
  }
  else
  {
    radio.stopListening();  //не слушаем радиоэфир, мы передатчик
  }
}

byte nRFread(void)
{
  byte pipeNo;                          
  if( radio.available(&pipeNo))
  {    // слушаем эфир со всех труб
    radio.read( &gotByte, 1 );         // читаем входящий сигнал
    DEBUG_PRINT(F("nRF receive: ")); DEBUG_PRINTLN(gotByte, HEX);
    radio.writeAckPayload(pipeNo,&gotByte, 1 );  // отправляем обратно то что приняли
  }
}

bool nRFwrite(byte sendByte)
{
  DEBUG_PRINT(F("Sending... ")); DEBUG_PRINTLN(sendByte, HEX);    
  if ( radio.write(&sendByte,1) )
  { //отправляем значение sendByte
    if(!radio.available())
    { //если получаем пустой ответ
      DEBUG_PRINTLN(F("Empty"));
    }
    else
    {      
      while(radio.available() )
      { // если в ответе что-то есть
        radio.read( &gotByte, 1 );  // читаем
        DEBUG_PRINT(F("Anser: ")); DEBUG_PRINTLN(gotByte);
        if(sendByte == gotByte) return 1;                                  
      }
    }   
  }
  else{ DEBUG_PRINTLN(F("Fail")); }

  return false; 
}

byte nRFgetValue(void)
{
  return gotByte;
}

void nRFclear(void)
{
  gotByte = 0;
}