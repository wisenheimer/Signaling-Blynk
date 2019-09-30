/*
  В скетче использованы функции из библиотеки iarduino_GSM
  https://iarduino.ru/file/345.html
*/
#include "modem.h"
#include <avr/sleep.h>
#include <avr/power.h>

FLAGS
bool flag_gprs_connect = false; // показывает, есть ли соединение
bool MODEM_NEED_INIT = false;

extern MY_SENS *sensors;

volatile uint8_t answer_flags;
enum answer {get_pdu, get_ip, ip_ok, gprs_connect, email_end, smtpsend, smtpsend_end, admin_phone};

#define CLEAR_FLAG_ANSWER           answer_flags=0
#define GET_FLAG_ANSWER(flag)       bitRead(answer_flags,flag)
#define SET_FLAG_ANSWER_ONE(flag)   bitSet(answer_flags,flag)
#define SET_FLAG_ANSWER_ZERO(flag)  bitClear(answer_flags,flag)
#define INVERT_FLAG_ANSWER(flag)    INV_FLAG(answer_flags,1<<flag)

# include "i2c.h"

#define OK   "OK"
#define ERR  "ERROR"

// определить количество занятых записей и их общее число
#define SIM_RECORDS_INFO  F("AT+CPBS?")

#define MODEM_GET_STATUS  F("AT+CPAS")
  #define NOT_READY       "CPAS: 1"

#define RING_BREAK        SERIAL_PRINTLN(F("ATH")) // разорвать все соединения

// Выставляем тайминги (мс)
// время между опросами модема на предмет зависания и не отправленных смс/email
#define REGULAR_OPROS_TIME  10000
uint32_t opros_time;
#if TM1637_ENABLE

uint32_t timeSync = 0;

#endif

#define GPRS_GET_IP       if(!answer_flags && flag_gprs_connect){SERIAL_PRINTLN(F("AT+SAPBR=2,1"));SET_FLAG_ANSWER_ONE(get_ip);gprs_init_count++;}
#define GPRS_CONNECT(op)  if(!answer_flags && !flag_gprs_connect){SERIAL_PRINT(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\";+SAPBR=3,1,\"APN\",\"internet\";+SAPBR=3,1,\"USER\",\"")); \
                          SERIAL_PRINT(PGM_TO_CHAR(op.user));SERIAL_PRINT(F("\";+SAPBR=3,1,\"PWD\",\""));SERIAL_PRINT(PGM_TO_CHAR(op.user));\
                          SERIAL_PRINTLN(F("\";+SAPBR=1,1"));SET_FLAG_ANSWER_ONE(gprs_connect);flag_gprs_connect=true;}
#define GPRS_DISCONNECT   if(flag_gprs_connect){SERIAL_PRINTLN(F("AT+SAPBR=0,1"));answer_flags=0;flag_gprs_connect=false;}

// ****************************************
#define ADMIN_PHONE_SET_ZERO   memset(&admin,0,sizeof(ABONENT_CELL))

#define PGM_TO_CHAR(str)       strcpy_P(text->GetText()+text->filling()+1,(char*)pgm_read_word(&(str))) 
// Опрашивает модем
#define READ_COM_FIND_RAM(str) strstr(text->GetText(),str)
#define READ_COM_FIND(str)     strstr_P(text->GetText(),PSTR(str))
#define READ_COM_FIND_P(str)   strstr_P(text->GetText(),(char*)pgm_read_word(&(str)))

#define POWER_ON digitalWrite(BOOT_PIN,LOW);DELAY(1000);digitalWrite(BOOT_PIN,HIGH)

#define GET_MODEM_ANSWER(answer,wait_ms) runAT(wait_ms,PSTR(answer))

enum {ADMIN};
const char cell_name_0[] PROGMEM = "\"ADMIN\"";
//const char cell_name_0[] PROGMEM = "\"Admin\"";
const char* const abonent_name[] PROGMEM = {cell_name_0};

//Эти сообщения пересылаются на e-mail
const char str_0[] PROGMEM = "+CBC:";
const char str_1[] PROGMEM = "ALARM";
const char* const string[] PROGMEM = {str_0, str_1};

uint8_t op_count; // число операторов 

typedef struct PROGMEM
{
  const char* op;
  const char* user;
} OPERATORS; 

#define ADD_OP(index,name,user) const char op_##index[]    PROGMEM=name; \
                                const char user_##index[]  PROGMEM=user

// Пароли для входа в интернет
ADD_OP(0, "MTS",     "mts");
ADD_OP(1, "MEGAFON", "gdata");
ADD_OP(2, "MegaFon", "gdata");
ADD_OP(3, "Beeline", "beeline");
ADD_OP(4, "Bee Line", "beeline");
// Все операторы с пустым паролем (Tele2, MOTIV, ALTEL и т.д.)
ADD_OP(5, "ANYOP", "");  

const OPERATORS op_base[] PROGMEM = {
  {op_0, user_0},
  {op_1, user_1},
  {op_2, user_2},
  {op_3, user_3},
  {op_4, user_4},
  {op_5, user_5}
};

// Условия для рестарта модема
#define ADD_RESET(index,name) const char reset_##index[] PROGMEM=name

ADD_RESET(0,NOT_READY);
ADD_RESET(1,"SIM800");
ADD_RESET(2,"NOT READY");
ADD_RESET(3,"SIM not");
ADD_RESET(4," 99");
ADD_RESET(5,"+CMS ERROR");
ADD_RESET(6,"failed");

const char* const reset[] PROGMEM = {reset_0, reset_1, reset_2, reset_3, reset_4, reset_5, reset_6};

// Условия для завершения звонка
#define ADD_BREAK(index,name) const char ath_##index[] PROGMEM=name

ADD_BREAK(0,"CONNECT");
ADD_BREAK(1,"ANSWER");
ADD_BREAK(2,"BUSY");

const char* const ath[] PROGMEM = {ath_0, ath_1, ath_2};

#if BEEP_ENABLE

  void beep()
  {
    tone(BEEP_PIN,5000);
    DELAY(1000);
    noTone(BEEP_PIN);
  }

  #define BEEP_INIT   pinMode(BEEP_PIN,OUTPUT);digitalWrite(BEEP_PIN,LOW)
  #define BEEP        beep();

#else

  #define BEEP_INIT
  #define BEEP

#endif

// Сколько раз модем может не ответить до перезагрузки
#define RESET_COUNT 3

//****************обработчик прерывания********************
//  О прерываниях подробно https://arduinonsk.ru/blog/87-all-pins-interrupts

void ring()
{

} 

#ifdef __cplusplus
extern "C"{
  ISR(PCINT2_vect)
  {

  }
}
#endif

//**********************************************************

MODEM::MODEM()
{
  op_count = sizeof(op_base)/sizeof(OPERATORS);
  gsm_operator = op_count - 1;
  // зуммер
  BEEP_INIT;
  // Устанавливаем скорость связи Ардуино и модема
  SERIAL_BEGIN;
  I2C_INIT;
  
  //  Установка кодировки для символов Кириллицы:
  TXTsendCodingDetect("п");
}

MODEM::~MODEM()
{
  delete text;
  delete email_buffer;
}

void MODEM::init()
{
  dtmf_index      = 0;
  dtmf_str_index  = 0;
  DTMF[0]         = 0;
  DTMF[1]         = 0;
  text            = new TEXT(350);
  email_buffer    = new TEXT(192);
  phone_num       = 0;
  cell_num        = 0;
  ADMIN_PHONE_SET_ZERO;
  SET_FLAG_ONE(RING_ENABLE);
  OTCHET_INIT
  reinit();
}

void MODEM::reinit()
{
  uint8_t i, count = 10;

  sim800_enable = false;
  reset_count = 0;
  gprs_init_count = 0;
  flag_gprs_connect=false;
  CLEAR_FLAG_ANSWER;
  
  DEBUG_PRINTLN(F("BOOT"));
  POWER_ON; // Включение GSM модуля
  for (i = 0; i < count; ++i)
  {
    SERIAL_PRINTLN(F("AT"));
    if(GET_MODEM_ANSWER(OK, 1000)) break;
  }

  opros_time = REGULAR_OPROS_TIME;
  timeRegularOpros = millis();
}

void MODEM::reinit_end()
{
  if(MODEM_NEED_INIT)
  {
    const __FlashStringHelper* cmd[] = {
      F("AT+DDET=1"),         // вкл. DTMF. 
      F("ATS0=0"),            // устанавливает количество гудков до автоответа
      F("AT+CLTS=1;&W"),         // синхронизация времени по сети
      F("ATE0"),              // выключаем эхо
      F("AT+CLIP=1"),         // Включаем АОН
#if PDU_ENABLE
      F("AT+CMGF=0"),         // PDU формат СМС
#else 
      F("AT+CMGF=1"),         // Формат СМС = ASCII текст
#endif
      F("AT+IFC=1,1"),        // устанавливает программный контроль потоком передачи данных
      F("AT+CSCS=\"GSM\""),   // Режим кодировки текста = GSM (только англ.)
      F("AT+CNMI=2,2,0,0,0"), // Текст смс выводится в com-порт
      F("AT+CSCB=1"),         // Отключаем рассылку широковещательных сообщений        
      //F("AT+CNMI=1,1")      // команда включает отображение номера пришедшей СМСки +CMTI: «MT», <номер смски>
      F("AT+CNMI=2,2"),       // выводит сразу в сериал порт, не сохраняя в симкарте
      F("AT+CPBS=\"SM\""),    // вкл. доступ к sim карте
      F("AT+CMGD=1,4"),       // удалить все смс
      F("AT+CEXTERNTONE=0"),  // открывает микрофон
      F("ATL9"),              // громкость динамика
    //  F("AT+CLVL=?"),
    //  F("AT+CLVL?"),
      SIM_RECORDS_INFO        // определить количество занятых записей и их общее число
    };

    uint8_t size = sizeof(cmd)/sizeof(cmd[0]);

    for(uint8_t i = 0; i < size; i++)
    {
      SERIAL_PRINTLN(cmd[i]);
      if(!GET_MODEM_ANSWER(OK, 10000)) return;
    }

    do { SERIAL_PRINTLN(F("AT+COPS?")); }
    while (!GET_MODEM_ANSWER("+COPS: 0,0", 10000));

    maxSMS=SMSmax();          //  Максимально допустимое количество входящих непрочитанных SMS сообщений.
    numSMS=maxSMS;

    DTMF[0] = SYNC_TIME;

    timeRegularOpros = millis();

    I2C_RESET

    BEEP

    MODEM_NEED_INIT = false;
  }     
}

#define DTMF_SHIFT(buf,index,len) if(index>len){for(uint8_t i=0;i<index;i++){buf[i]=buf[i+1];}index--;}

uint8_t MODEM::parser()
{
  char *p, *pp;
  char ch;

  if(GET_FLAG_ANSWER(get_pdu))
  {
    SMS_INFO sms_data;

    SERIAL_PRINTLN(text->GetText());
    
    PDUread(&sms_data);
    
    SERIAL_PRINT(F("num=")); SERIAL_PRINTLN(sms_data.num);
    SERIAL_PRINT(F("tim=")); SERIAL_PRINTLN(sms_data.tim);
    SERIAL_PRINT(F("ID="));  SERIAL_PRINTLN(sms_data.lngID);
    SERIAL_PRINT(F("SUM=")); SERIAL_PRINTLN(sms_data.lngSum);
    SERIAL_PRINT(F("NUM=")); SERIAL_PRINTLN(sms_data.lngNum);

    SERIAL_PRINTLN(sms_data.txt);

    text->Clear();  
    text->AddText(sms_data.txt);
    
    if(esp8266_enable || SMS_FORWARD)
    { // пересылвем входящие смс админу
      email_buffer->AddText(text->GetText());
      email_buffer->AddChar('\n');
    }

    if (admin.phone[0])
    {
      if (strstr(admin.phone, sms_data.num))
      {
        // смс от Админа
        SET_FLAG_ANSWER_ONE(admin_phone);
      }
    }

    SET_FLAG_ANSWER_ZERO(get_pdu);     
  }

  if(READ_COM_FIND(OK)!=NULL)
  {
    if(GET_FLAG_ANSWER(email_end))
    {
      SET_FLAG_ANSWER_ZERO(email_end);
      SERIAL_PRINT(F("AT+SMTPBODY="));
      SERIAL_PRINTLN(email_buffer->filling());      
    }

    if(GET_FLAG_ANSWER(smtpsend))
    {
      SET_FLAG_ANSWER_ZERO(smtpsend);
      SET_FLAG_ANSWER_ONE(smtpsend_end);
      SERIAL_PRINTLN(F("AT+SMTPSEND"));
    }

    if(GET_FLAG_ANSWER(get_ip))
    {
      SET_FLAG_ANSWER_ZERO(get_ip);
    }

    if(GET_FLAG_ANSWER(ip_ok))
    {
      SET_FLAG_ANSWER_ZERO(ip_ok);
      email();
    }

    if(GET_FLAG_ANSWER(gprs_connect))
    {
      SET_FLAG_ANSWER_ZERO(gprs_connect);
      GPRS_GET_IP
    }
  }

  // требуется загрузка текста смс для отправки
  if(READ_COM_FIND(">")!=NULL)
  {
#if PDU_ENABLE

#else
    SERIAL_PRINT(email_buffer->GetText());    
    SERIAL_FLUSH;
    SERIAL_WRITE(26);
#endif

    return 1;        
  }


  // +CPBS: "SM",18,100
  if((p = READ_COM_FIND("CPBS:"))!=NULL)
  {
    p+=11;
    pp=p;
    while(*p!=',' && *p) p++;
    *p=0;
    phone_num = atoi(pp);
    cell_num = atoi(p+1);

    return 1;    
  }

  // +CPBF: 20,"+79xxxxxxxxx",145,"ADMIN"
  // +CPBR: 1,"+380xxxxxxxxx",129,"Ivanov"
  // +CPBR: 20,"+79xxxxxxxxx",145,"USER"
  // Запоминаем индекс ячейки телефонной книги
  if((p=READ_COM_FIND("CPBR: "))!=NULL || (p=READ_COM_FIND("CPBF: "))!=NULL)
  {
    p+=6; pp = p;
    if(GET_FLAG(EMAIL_ENABLE) || esp8266_enable)
    {
      email_buffer->AddText(p);
      if(millis() - timeRegularOpros > opros_time - 1000)
      { // оставляем время для получения всех записей с симкарты до отправки e-mail
        timeRegularOpros = millis() - opros_time + 1000;
      }       
    }
    while(*p!=',' && *p) p++;
    *p = 0;
    last_abonent.index = atoi(pp);
    p+=2; pp = p;
    get_name(get_number_and_type(p));

    if(strstr_P(pp,(char*)pgm_read_word(&abonent_name[ADMIN]))!=NULL)
    {
      admin = last_abonent;
    }
    return 1;
  }

  // RING
  //+CLIP: "+79xxxxxxxxx",145,"",0,"USER",0
  //+CLIP: "+79xxxxxxxxx",145,"",0,"",0
  if((p=READ_COM_FIND("CLIP:"))!=NULL)
  {
    if(GET_FLAG(EMAIL_ENABLE) || esp8266_enable) email_buffer->AddText(p);
    memset(&last_abonent, 0, sizeof(ABONENT_CELL));

    get_name(get_number_and_type(p+7)+5);
    if(last_abonent.name[2]) // номер зарегистрирован
    {
      if(READ_COM_FIND_RAM(admin.phone)==NULL && admin.phone[0]) // не админ
      {
        RING_BREAK;
        if(GET_FLAG(GUARD_ENABLE)) DTMF[0] = GUARD_OFF;
        else DTMF[0] = GUARD_ON;
      }
      else SERIAL_PRINTLN(F("ATA"));          
    }
    else
    {
      RING_BREAK;
      if(admin.index == 0)
      {
        /* Добавление текущего номера в телефонную книгу */
        strcpy_P(last_abonent.name,(char*)pgm_read_word(&(abonent_name[ADMIN])));    
        SERIAL_PRINT(F("AT+CPBW=,\""));
        SERIAL_PRINT(last_abonent.phone);
        SERIAL_PRINT(F("\",145,"));
        SERIAL_PRINTLN(last_abonent.name);
        DTMF[0] = EMAIL_PHONE_BOOK;       
      }
    } 
    
    DEBUG_PRINT(F("admin.index=")); DEBUG_PRINTLN(admin.index);
    DEBUG_PRINT(F("admin.phone=")); DEBUG_PRINTLN(admin.phone);
    DEBUG_PRINT(F("last_abonent.phone=")); DEBUG_PRINTLN(last_abonent.phone);

    return 1;
  }

  if(READ_COM_FIND("COPS: 0,0")!=NULL)
  {
    gsm_operator = 0;
    while (gsm_operator < op_count-1 && READ_COM_FIND_P(op_base[gsm_operator].op)==NULL) gsm_operator++;
    return 1;
  }

  if ((p=READ_COM_FIND("+CPMS:"))!=NULL) 
  { //  Получаем позицию начала текста "+CPMS:" в ответе +CPMS: "ПАМЯТЬ1",ИСПОЛЬЗОВАНО,ОБЪЁМ, "ПАМЯТЬ2",ИСПОЛЬЗОВАНО,ОБЪЁМ, "ПАМЯТЬ3",ИСПОЛЬЗОВАНО,ОБЪЁМ\r\n
    uint8_t i = 0;
    while(*p && i<7)
    {
      if(*p==',') i++; //  Получаем позицию параметра ИСПОЛЬЗОВАНО, он находится через 7 запятых после текста "+CPMS:".
      p++;
    }

    SMSsum = _num(*p); p++; //  Получаем первую цифру в найденной позиции, это первая цифра количества использованной памяти.
    if( *p!=','  ){SMSsum*=10; SMSsum+= _num(*p); p++;}  //  Если за первой цифрой не стоит знак запятой, значит там вторая цифра числа, учитываем и её.
    SERIAL_PRINT(F("SMSsum="));
    SERIAL_PRINTLN(SMSsum);

    //  Получаем позицию параметра ОБЪЁМ, он находится через 8 запятых после текста "+CPMS:".
    if( *p!=',' )
    {
      p++;
      maxSMS = _num(*p); p++;                         //  Получаем первую цифру в найденной позиции, это первая цифра доступной памяти.
      if( *p!='\r'  ){maxSMS*=10; maxSMS+= _num(*p);} //  Если за первой цифрой не стоит знак \r, значит там вторая цифра числа, учитываем и её.
      SERIAL_PRINT(F("maxSMS="));
      SERIAL_PRINTLN(maxSMS);
    }
    
    return 1;
  }

  if (READ_COM_FIND("CMGS:")!=NULL) // +CMGS: <mr> - индекс отправленного сообщения 
  { // смс успешно отправлено
    // удаляем все отправленные сообщения
    SERIAL_PRINTLN(F("AT+CMGD=1,3"));

    uint8_t i;
#if PDU_ENABLE
    i=70;    
#else
    i=160;    
#endif
    while(i && email_buffer->filling())
    {
      ch=email_buffer->GetChar();
      i--;
    }

    return 1;
  }
  
  if ((p=READ_COM_FIND("CMTI:"))!=NULL) // получили номер смс в строке вида +CMTI: "SM",0
  {       
    p+=11;
    // получили номер смс в строке вида +CMTI: "SM",0
    // чтение смс из памяти модуля
    SERIAL_PRINT(F("AT+CMGR=")); SERIAL_PRINTLN(p);
    return 1;
  }

  if ((p=READ_COM_FIND("+CMGR:"))!=NULL) // +CMGR: СТАТУС,["НАЗВАНИЕ"],РАЗМЕР\r\nPDU\r\n\r\nOK\r\n".
  {       
    if((p = READ_COM_FIND_RAM(","))!=NULL) //  Получаем позицию первой запятой следующей за текстом "+CMGR:", перед этой запятой стоит цифра статуса.
    {             
      if (*(p-1) != '0') { SMSavailable(); return;}  //  Если параметр СТАТУС не равен 0, значит это не входящее непрочитанное SMS сообщение. Обращаемся к функции SMSavailable(), чтоб удалить все сообщения, кроме входящих непрочитанных.
      SET_FLAG_ANSWER_ONE(get_pdu);
    }
    return 1;
  }

  if ((p=READ_COM_FIND("+CMT:"))!=NULL) // получили номер смс в строке вида +CMT: "",140
  {
    SET_FLAG_ANSWER_ONE(get_pdu);      
    return 1;
  }

  // +CMGL: 1,"REC UNREAD","679","","18/10/22,19:16:57+12"
  if ((p=READ_COM_FIND("CMGL:"))!=NULL) // получили номер смс в строке
  {       
    // находим индекс смс в памяти модема
    p+=6;
   
    SERIAL_PRINT(F("AT+CMGR="));
    while(*p!=',')
    {  // чтение смс из памяти модуля
       SERIAL_PRINT(*p++);
    }
    SERIAL_PRINTLN();
    return 1;    
  }

  if (admin.phone[0])
  {
    if (READ_COM_FIND_RAM(admin.phone))
    {
      // звонок или смс от Админа
      SET_FLAG_ANSWER_ONE(admin_phone);
      return 1;
    }      
  }

  if(READ_COM_FIND("DIALTON")!=NULL)
  {
    RING_TO_ADMIN(admin.index, admin.phone[0]);
    return 1;    
  }

#if TM1637_ENABLE
  // получаем время  +CCLK: "04/01/01,03:36:04+12"
  if ((p=READ_COM_FIND("+CCLK"))!=NULL)
  {
    char ch;
    uint8_t time[3];
    p+=5;
    if(*p==':') p+=12;
    else if(*p=='=') p+=11;
    else return 0;

    for(uint8_t i = 0; i < 3; i++)
    {
      ch = *(p+2);
      *(p+2) = 0;
      time[i] = atoi(p);
      *(p+2) = ch;
      p+=3;
    }
    clock_set(time);
  }
#endif

  for(uint8_t i = 0; i < 7; i++)
  {
    if(READ_COM_FIND_P(reset[i])!=NULL)
    {
      DTMF[0]=MODEM_RESET;
      return 1;
    }
  }

  for(uint8_t i = 0; i < 3; i++)
  {
    if(READ_COM_FIND_P(ath[i])!=NULL)
    {
      RING_BREAK;
      dtmf_index = 0;
      dtmf_str_index = 0;
      return 1;
    }
  }

  // ALARM RING
  // +CALV: 2

  for(uint8_t i = 0; i < 2; i++)
  {
    if((p=READ_COM_FIND_P(string[i]))!=NULL)
    {
      email_buffer->AddText(p);
      return 1;
    }
  }

  // +CUSD: 0, "Balance:40,60r", 15
  // +CUSD: 0, "041D0435043F0440043", 72
  if ((p=READ_COM_FIND("+CUSD:"))!=NULL)
  { // Пришло уведомление о USSD-ответе
    char* pos;
    uint16_t len;    //  Переменные для хранения позиции начала и длины информационного блока в ответе (текст ответа).
    uint8_t coder;   //  Переменные для хранения статуса USSD запроса и кодировки USSD ответа.
    char _txt[161]; 
//  Разбираем ответ:  
//  Получаем  позицию начала текста "+CUSD:" в ответе "+CUSD: СТАТУС, "ТЕКСТ" ,КОДИРОВКА\r\n".
    while(*p!='\"' && *p) p++;
    if(*p!='\"') return 0;
    p++;  pos = p;              //  Сохраняем позицию первого символа текста ответа на USSD запрос.
    while(*p!='\"' && *p) p++;
    if(*p!='\"') return 0;
    len = p-pos; len/=2;        //  Сохраняем количество байтов в тексте ответа на USSD запрос.
    while(*p!=',' && *p) p++;
    if(*p!=',') return 0;
    p++;
    while(*p==' ' && *p) p++;
    coder = _num(*p); p++;      //  Сохраняем значение первой цифры кодировки текста.
    if( *p!='\r'  ){coder*=10; coder+= _num(*p); p++;}  //  Сохраняем значение второй цифры кодировки текста (если таковая существует).
    if( *p!='\r'  ){coder*=10; coder+= _num(*p); p++;}  //  Сохраняем значение третей цифры кодировки текста (если таковая существует).
//  Разкодируем ответ в строку _txt:     //
    if(coder==72) {_SMSdecodUCS2(_txt, len, pos-text->GetText());}else  //  Разкодируем ответ в строку _txt указав len - количество байт     и pos - позицию начала закодированного текста.
    if(coder==15) {memcpy(_txt, pos, len<<1);_txt[len<<1]=0;}else
                  // {_SMSdecodGSM (buf, (len/7*8),pos-text->GetText());}else    //  Разкодируем ответ в строку _txt указав len - количество символов и pos - позицию начала закодированного текста.
                  {_SMSdecod8BIT(_txt, len, pos-text->GetText());}      //  Разкодируем ответ в строку _txt указав len - количество байт     и pos - позицию начала закодированного текста.

    email_buffer->AddText(_txt);
    email_buffer->AddChar('\n');

    return 1;    
  }

  ////////////////////////////////////////////////////////////
  /// Для GPRS
  ////////////////////////////////////////////////////////////
  // требуется загрузка текста письма для отправки
  if(READ_COM_FIND("DOWNLOAD")!=NULL)
  {
    SERIAL_PRINT(email_buffer->GetText());
    SERIAL_FLUSH;
    SERIAL_WRITE(26);
    // Ждём OK от модема,
    // после чего даём команду на отправку письма
    SET_FLAG_ANSWER_ONE(smtpsend);
    
    return 1;        
  }

  if((p = READ_COM_FIND("+SMTPSEND:"))!=NULL)
  {
    SET_FLAG_ANSWER_ZERO(smtpsend_end); 
    // письмо успешно отправлено
    if((p = READ_COM_FIND(": 1"))!=NULL)
    {
#if CONNECT_ALWAYS==0
      // разрываем соединение с интернетом
      GPRS_DISCONNECT;

#endif
      gprs_init_count = 0;
      email_buffer->Clear();
    }

    return 1;    
  }

  if((p = READ_COM_FIND("+SAPBR"))!=NULL)
  {
    if((p = READ_COM_FIND(": 1,1"))!=NULL)
    {
      SET_FLAG_ANSWER_ZERO(get_ip);
      SET_FLAG_ANSWER_ONE(ip_ok);
    }
    else flag_gprs_connect=false;

    return 1;
  }

  // получаем смс
  // Выполнение любой AT+ команды
  if ((p=READ_COM_FIND("AT+"))!=NULL || (p=READ_COM_FIND("at"))!=NULL)
  {
    if (GET_FLAG_ANSWER(admin_phone))
    {
      SERIAL_PRINTLN(p);
      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  // Выполнение любой AT+ команды
  if ((p=READ_COM_FIND("Охрана"))!=NULL || (p=READ_COM_FIND("охрана"))!=NULL)
  {
    if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=13;
      if(*p=='0') DTMF[0] = GUARD_OFF;
      if(*p=='1') DTMF[0] = GUARD_ON;
      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  // Выполнение любой AT+ команды
  if ((p=READ_COM_FIND("Guard"))!=NULL || (p=READ_COM_FIND("guard"))!=NULL)
  {
    if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=6;
      if(*p=='0') DTMF[0] = GUARD_OFF;
      if(*p=='1') DTMF[0] = GUARD_ON;
      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  if((p = READ_COM_FIND("DTMF:"))!=NULL) //+DTMF: 2
  { 
    p+=6;

    DEBUG_PRINT("p=");
    DEBUG_PRINTLN(p);

    DEBUG_PRINT("dtmf=");
    DEBUG_PRINTLN(dtmf);
   
    if(*p == '#')
    {
      DTMF[0] = atoi(dtmf);
      dtmf_index=0;
      RING_BREAK;
    }
    else if(*p == '*')
    {
      DTMF[1] = atoi(dtmf);
      dtmf_index=0; 
    }
    else
    {
      DTMF_SHIFT(dtmf,dtmf_index,DTMF_BUF_SIZE-2);
      dtmf[dtmf_index++] = *p;
      dtmf[dtmf_index] = 0;   
    }

    DTMF_SHIFT(dtmf_str,dtmf_str_index,7); 
    dtmf_str[dtmf_str_index++] = *p;
    dtmf_str[dtmf_str_index] = 0;
   
    return 1;    
  }
  
  // Получаем смс вида "pin НОМЕР_ПИНА on/off".
  // Например, "pin 14 on" и "pin 14 off"
  // задают пину 14 (А0) значения HIGH или LOW соответственно.
  // Поддерживаются пины от 5 до 19, где 14 соответствует А0, 15 соответствует А1 и т.д
  // Внимание. Пины A6 и A7 не поддерживаются, т.к. они не работают как цифровые выходы.
  if ((p=READ_COM_FIND("pin "))!=NULL)
  {
    //if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=4;
      uint8_t buf[3];
      uint8_t i = 0;
      uint8_t pin;
      uint8_t level;
      while(*p>='0' && *p<='9' && i < 2)
      {
        buf[i++] = *p++;
      }
      buf[i] = 0;
      pin = atoi(buf);

      if(pin < 5 || pin > A5) return;
      
      p++;
      
      if(*p == 'o' && *(p+1) == 'n') level = HIGH;
      else if(*p == 'o' && *(p+1) == 'f' && *(p+2) == 'f') level = LOW;
      else return;      
  
      pinMode(pin, OUTPUT); // устанавливаем пин как выход
      digitalWrite(pin, level);
      // ответное смс
      email_buffer->AddText_P(PSTR("PIN "));
      email_buffer->AddInt((int)pin);
      email_buffer->AddText_P(PSTR(" is "));
      email_buffer->AddChar(digitalRead(pin)+48);
      email_buffer->AddChar('\n');
      DEBUG_PRINTLN(email_buffer->GetText()); // отладочное собщение

      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }   
}

void MODEM::sleep()
{  
#if SLEEP_MODE_ENABLE

  if(!digitalRead(POWER_PIN) && !GET_FLAG(ALARM))
  {
    // Маскируем прерывания
    PCMSK2 = 0b11110000;
    // разрешаем прерывания
    PCICR = bit(2);
    // Прерывание для RING
    attachInterrupt(0, ring, LOW); //Если на 0-вом прерываниии - ноль, то просыпаемся.
      
    I2C_OFF

#if TM1637_ENABLE
    digitalWrite(CLK, LOW);
    digitalWrite(DIO, LOW);         
#endif

    SERIAL_PRINTLN(F("AT+CSCLK=2")); // Режим энергосбережения
       
    SERIAL_FLUSH;
        
#if WTD_ENABLE
    wdt_disable();
#endif 

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode(); // Переводим МК в спящий режим

    // запрещаем прерывания
    PCICR = 0;
    detachInterrupt(0);

    do
    {
      text->Clear();
      SERIAL_PRINTLN(F("AT+CSCLK=0")); // Выкл. режим энергосбережения
      DELAY(500);
      SERIAL_PRINTLN(F("AT+CSCLK?"));
    }
    while(!GET_MODEM_ANSWER("CSCLK: 0", 1000));

    I2C_ON
                
    if(GET_FLAG(EMAIL_ENABLE) || esp8266_enable) email_buffer->AddText_P(PSTR("WakeUp:"));

    DTMF[0] = SYNC_TIME;

#if WTD_ENABLE
    wdt_enable(WDTO_8S);
#endif   
  }
#endif 
}

void MODEM::email()
{
  if(email_buffer->filling())
  {
    uint8_t i, size;

    const __FlashStringHelper* cmd[] = 
    {
      F("AT+EMAILCID=1;+EMAILTO=30;+SMTPSRV="), SMTP_SERVER,
      F(";+SMTPAUTH=1,"), SMTP_USER_NAME_AND_PASSWORD,
      F(";+SMTPFROM="), SENDER_ADDRESS_AND_NAME,
      F(";+SMTPSUB=\"Info\""),
      F(";+SMTPRCPT=0,0,"), RCPT_ADDRESS_AND_NAME
#ifdef RCPT_CC_ADDRESS_AND_NAME
      ,F(";+SMTPRCPT=1,0,"), RCPT_CC_ADDRESS_AND_NAME // копия
#ifdef RCPT_BB_ADDRESS_AND_NAME
      ,F(";+SMTPRCPT=2,0,"), RCPT_BB_ADDRESS_AND_NAME // вторая копия
#endif
#endif
    };

    size = sizeof(cmd)/sizeof(cmd[0]);
  
    for(i = 0; i < size; i++)
    {
      SERIAL_PRINT(cmd[i]);
      SERIAL_FLUSH;
    } 

    SERIAL_PRINTLN();

    SET_FLAG_ANSWER_ONE(email_end);
  }
}

char* MODEM::get_number_and_type(char* p)
{
  uint8_t i = 0;

  while((*p!='\"' || *p=='+') && i < NLENGTH-1) last_abonent.phone[i++] = *p++;
        
  last_abonent.phone[i] = 0;

  return p+6;
}

char* MODEM::get_name(char* p)
{
  uint8_t i = 0;
  uint8_t count = 2;

  memset(last_abonent.name, 0, TLENGTH);

  while(count && *p)
  {
    if(*p == '\"') count--;
    last_abonent.name[i++] = *p++;
  }

  return p;
}

bool MODEM::read_com(const char* answer)
{
  char inChar;
  
  while (Serial.available()) {
    // получаем новый байт:
    inChar = (char)Serial.read();
    // добавляем его:
    if(!text->free_space())
    {
      text->GetChar();
    }
    text->AddChar(inChar);    
    // если получили символ новой строки, оповещаем программу об этом,
    // чтобы она могла принять дальнейшие действия.
    if (inChar == '\n')
    {
      if(text->filling() == 1) text->Clear();
      else
      {
        DEBUG_PRINT(text->GetText());      
        if(!sim800_enable)
        {
          sim800_enable = true;
          MODEM_NEED_INIT = true;
        }
        reset_count = 0;
        timeRegularOpros = millis();
        parser();
        if(answer!=NULL)
        {
          if(strstr_P(text->GetText(),answer)!=NULL)
          {
            text->Clear();
            return true;
          }
        }        
        text->Clear();    
      }
    }
  }

  return false;
}

#define DELETE_PHONE(index)    {SERIAL_PRINT(F("AT+CPBW="));SERIAL_PRINTLN(index);}
#define FLAG_STATUS(flag,name) flag_status(flag,PSTR(name))

void MODEM::flag_status(int flag, const char* name)
{
  email_buffer->AddText_P(name);
  email_buffer->AddText_P(GET_FLAG(flag)?PSTR(VKL):PSTR(VIKL));
}

void MODEM::flags_info()
{
  FLAG_STATUS(GUARD_ENABLE,  GUARD_NAME);
#if SIRENA_ENABLE
  FLAG_STATUS(SIREN_ENABLE,  SIREN_NAME);
  FLAG_STATUS(SIREN2_ENABLE, SIREN2_NAME);
#endif
    
  if (sim800_enable)
  {
    FLAG_STATUS(EMAIL_ENABLE,EMAIL_NAME);
    FLAG_STATUS(RING_ENABLE, RING_NAME);
    FLAG_STATUS(SMS_ENABLE,  SMS_NAME);
  }
  
  email_buffer->AddChar('\n');
}

void MODEM::wiring() // прослушиваем телефон
{
  uint8_t tmp;

  read_com(NULL);

  if (sim800_enable)
  {
#if TM1637_ENABLE
    // синхронизация времени часов
    if(millis() - timeSync > SYNC_TIME_PERIOD)
    {
      DTMF[0] = SYNC_TIME;
      timeSync = millis();
    }
#endif

    // Опрос модема
    if(millis() - timeRegularOpros > opros_time)
    {
      SET_FLAG_ANSWER_ZERO(admin_phone);

      if(reset_count > RESET_COUNT) DTMF[0]=MODEM_RESET;

      if(gprs_init_count > RESET_COUNT)
      {
        // если gprs не подключается, переходим на смс
        if(GET_FLAG(SMS_ENABLE))
        {
          gprs_init_count = 0;
          SET_FLAG_ZERO(EMAIL_ENABLE);
          GPRS_DISCONNECT
        } 
        else DTMF[0]=MODEM_RESET;
      }

      if(MODEM_NEED_INIT==false && !admin.phone[0]) DTMF[0]=EMAIL_ADMIN_PHONE;

      reinit_end();

      reset_count++;

      if(MODEM_NEED_INIT==false)
      {
        // Есть данные для отправки
        if(email_buffer->filling() && esp8266_enable==false)
        {      
          if(GET_FLAG(EMAIL_ENABLE))
          {
            GPRS_CONNECT(op_base[gsm_operator])
            else GPRS_GET_IP                             
          }
          else // Отправка SMS
          if(GET_FLAG(SMS_ENABLE))
          {
            if(admin.phone[0])
            {
              uint8_t len = email_buffer->filling();
              SERIAL_PRINT(F("SMS LEN = ")); SERIAL_PRINTLN(len);
#if PDU_ENABLE
              char txt[71];
              tmp = email_buffer->GetBytes(txt,70,1);
              txt[tmp] = 0;

              SMSsend(txt,admin.phone);
#else
              // AT+CMGS=\"+79xxxxxxxxx\"
              SERIAL_PRINT(F("AT+CMGS=\""));SERIAL_PRINT(admin.phone);SERIAL_PRINTLN('\"');
#endif
            }            
          }
          else email_buffer->Clear();            
        }
        else if(MODEM_NEED_INIT==false)
        {
          sleep();
          // проверяем непрочитанные смс и уровень сигнала
          //SERIAL_PRINTLN(F("AT+CMGL=\"REC UNREAD\",1;+CSQ"));
          SERIAL_PRINTLN(F("AT+CSQ"));
        }
      }

      timeRegularOpros = millis();       
    }
  }
  else
  {
    if(text->filling())
    {
      parser();
      text->Clear();
    }
  }

  if(GET_FLAG(INTERRUPT))
  {
    SET_FLAG_ZERO(INTERRUPT);
    DTMF[0] = GET_INFO;
    sensors->TimeReset();
  }

  if(DTMF[0])
  {
    DEBUG_PRINT(F("DTMF=")); DEBUG_PRINTLN(DTMF[0]);
    if(DTMF[1])
    {
      SERIAL_PRINT(F("AT+CUSD=1,\"")); SERIAL_PRINT(dtmf_str); SERIAL_PRINTLN('\"');
    }
    else    
    switch (DTMF[0])
    {
      case GUARD_ON:
        // Закрываем дверь в автомобиле
        CLOSE_DOOR        

        SET_FLAG_ONE(GUARD_ENABLE);
        FLAG_STATUS(GUARD_ENABLE, GUARD_NAME);
        email_buffer->AddChar('\n');
        break;                        
      case GUARD_OFF:
        // Открываем дверь в автомобиле
        OPEN_DOOR
        SET_FLAG_ZERO(GUARD_ENABLE);
        FLAG_STATUS(GUARD_ENABLE, GUARD_NAME);
        email_buffer->AddChar('\n');
        break;
      case GET_INFO:
        email_buffer->AddText_P(PSTR(SVET));
        email_buffer->AddText_P(digitalRead(POWER_PIN)?PSTR(VKL):PSTR(VIKL));
        flags_info(); 
        if(GET_FLAG(GUARD_ENABLE)) sensors->GetInfoAll(email_buffer);                       
        break;
#if SIRENA_ENABLE
      case SIREN_ON_OFF:
        INVERT_FLAG(SIREN_ENABLE);
        FLAG_STATUS(SIREN_ENABLE, SIREN_NAME);
        INVERT_FLAG(SIREN2_ENABLE);
        FLAG_STATUS(SIREN2_ENABLE, SIREN2_NAME);
        email_buffer->AddChar('\n');             
        break;
#endif
      case EMAIL_ON_OFF:
        INVERT_FLAG(EMAIL_ENABLE);
        FLAG_STATUS(EMAIL_ENABLE, EMAIL_NAME);
        email_buffer->AddChar('\n');
        break;
      case MODEM_RESET:
          reinit();
        break;
        case ESP_RESET:
          I2C_RESET;
        break;
      case TEL_ON_OFF:
        INVERT_FLAG(RING_ENABLE);
        FLAG_STATUS(RING_ENABLE, RING_NAME);
        email_buffer->AddChar('\n');
        DEBUG_PRINTLN("RING");     
        break;      
      case SMS_ON_OFF:
        INVERT_FLAG(SMS_ENABLE);
        FLAG_STATUS(SMS_ENABLE, SMS_NAME);
        email_buffer->AddChar('\n');             
        break;
      case BAT_CHARGE:
        SERIAL_PRINTLN(F("AT+CBC"));
        break;
      case EMAIL_ADMIN_PHONE: // получение номера админа на email
        SERIAL_PRINT(F("AT+CPBF="));
        SERIAL_PRINTLN(PGM_TO_CHAR(abonent_name[ADMIN]));   
        break;
      case EMAIL_PHONE_BOOK: // получение на email всех записей на симкарте
        SERIAL_PRINTLN(F("AT+CPBF"));
        break;
      case ADMIN_NUMBER_DEL:
        DELETE_PHONE(admin.index);
        ADMIN_PHONE_SET_ZERO;
        break;
      case SM_CLEAR:
        SERIAL_PRINTLN(SIM_RECORDS_INFO);
        if(GET_MODEM_ANSWER(OK, 5000))
        {
          ADMIN_PHONE_SET_ZERO;
          tmp = 0;
          uint8_t index = 1;
          while(tmp < phone_num && index <= cell_num)
          {
            SERIAL_PRINT(F("AT+CPBR=")); SERIAL_PRINTLN(index);
            if(GET_MODEM_ANSWER("CPBR:", 5000))
            {
              tmp++;
              DELETE_PHONE(index);
            }
            index++;
          }  
          //SERIAL_PRINTLN(SIM_RECORDS_INFO);
        }
        break;
      case SYNC_TIME:
        SERIAL_PRINTLN(F("AT+CCLK?"));
        break;
      default:
        SERIAL_PRINT(F("AT+CUSD=1,\""));
#if PDU_ENABLE
        SERIAL_PRINT('*');
#else
        SERIAL_PRINT('#');
#endif
        SERIAL_PRINT(DTMF[0]); SERIAL_PRINTLN(F("#\""));
    }
    DTMF[0] = 0;
    DTMF[1] = 0;
    
    dtmf_index      = 0;
    dtmf_str_index  = 0;
  }
}

//    ФУНКЦИЯ ВЫПОЛНЕНИЯ AT-КОМАНД:                                                         //  Функция возвращает: строку с ответом модуля.
bool  MODEM::runAT(uint16_t wait_ms, const char* answer)
{
  bool res;
  uint32_t time = millis() + wait_ms;

  do
  {    
    res = read_com(answer);
#if WTD_ENABLE
    wdt_reset();
#endif        
  }
  while(millis() < time && !res);

  return res;
}

//    ФУНКЦИЯ ПРЕОБРАЗОВАНИЯ СИМВОЛА В ЧИСЛО:                                           //  Функция возвращает: число uint8_t.
uint8_t MODEM::_num(char symbol){                                                       //  Аргументы функции:  символ 0-9,a-f,A-F.
  uint8_t i = uint8_t(symbol);                                                          //  Получаем код символа
  if ( (i>=0x30) && (i<=0x39) ) { return i-0x30; }else                                  //  0-9
  if ( (i>=0x41) && (i<=0x46) ) { return i-0x37; }else                                  //  A-F
  if ( (i>=0x61) && (i<=0x66) ) { return i-0x57; }else                                  //  a-f
  { return      0; }                                                                    //  остальные символы вернут число 0.
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ ПРЕОБРАЗОВАНИЯ ЧИСЛА В СИМВОЛ:                                            //  Функция возвращает: символ char.
char  MODEM::_char(uint8_t num){                                                        //  Аргументы функции:  число 0-15.
  if(num<10){return char(num+0x30);}else                                                //  0-9
  if(num<16){return char(num+0x37);}else                                                //  A-F
  {return '0';}                                                                         //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ ПОЛУЧЕНИЯ КОЛИЧЕСТВА СИМВОЛОВ В СТРОКЕ С УЧЁТОМ КОДИРОВКИ:                //  Функция возвращает: число uint16_t соответствующее количеству символов (а не байт) в строке.
uint16_t MODEM::_SMStxtLen(const char* txt){                                            //  Аргументы функции:  txt - строка с текстом
  uint16_t numIn=0, sumSymb=0;                                                          //  Объявляем временные переменные.
  uint16_t lenIn=strlen(txt);                                                           //
  uint8_t  valIn=0;                                                                     //
  switch(codTXTsend){                                                                   //  Тип кодировки строки txt.
  //  Получаем количество символов в строке txt при кодировке UTF-8:                    //
  case GSM_TXT_UTF8:                                                                    //
    while(numIn<lenIn){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
      valIn=uint8_t(txt[numIn]); sumSymb++;                                             //
      if(valIn<0x80){numIn+=1;}else                                                     //  Символ состоит из 1 байта
      if(valIn<0xE0){numIn+=2;}else                                                     //  Символ состоит из 2 байт
      if(valIn<0xF0){numIn+=3;}else                                                     //  Символ состоит из 3 байт
      if(valIn<0xF8){numIn+=4;}else                                                     //  Символ состоит из 4 байт
      {numIn+=5;}                                                                       //  Символ состоит из 5 и более байт
    }                                                                                   //
    break;                                                                              //
    //  Получаем количество символов в строке txt при кодировке CP866:                  //
    case GSM_TXT_CP866:   sumSymb=lenIn; break;                                         //
    //  Получаем количество символов в строке txt при кодировке Windows1251:            //
    case GSM_TXT_WIN1251: sumSymb=lenIn; break;                                         //
  } return sumSymb;                                                                     //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ КОДИРОВАНИЯ ТЕКСТА SMS СООБЩЕНИЯ В ФОРМАТЕ GSM:                           //  Функция возвращает: число uint16_t соответствующее позиции после последнего закодированного символа из строки txt.
uint16_t MODEM::_SMScoderGSM(const char* txt, uint16_t pos, uint16_t len){              //  Аргументы функции:  txt - строка с текстом, pos - позиция взятия первого символа из строки txt, len - количество кодируемых символов из строки txt.
  uint8_t  valByteIn = 0;                                                               //  Определяем временную переменную для хранения значения очередного читаемого символа из строки txt.
  uint16_t numByteIn = 0;                                                               //  Определяем временную переменную для хранения номера   очередного читаемого символа из строки txt.
  uint8_t  numBitIn = 0;                                                                //  Определяем временную переменную для хранения номера   очередного бита в байте valByteIn.
  uint8_t  valByteOut = 0;                                                              //  Определяем временную переменную для хранения значения очередного сохраняемого байта.
  uint8_t  numBitOut = 0;                                                               //  Определяем временную переменную для хранения номера   очередного бита в байте valByteOut.
                                                                                        //
  if(len==255){len=strlen(txt)-pos;}                                                    //
  while( numByteIn < len ){                                                             //  Пока номер очередного читаемого символа меньше заданного количества читаемых символов.
    valByteIn = txt[pos+numByteIn]; numByteIn+=1;                                       //  Читаем значение символа с номером numByteIn в переменную valByteIn.
    for(numBitIn=0; numBitIn<7; numBitIn++){                                            //  Проходим по битам байта numByteIn (номер бита хранится в numBitIn).
      bitWrite( valByteOut, numBitOut, bitRead(valByteIn,numBitIn) ); numBitOut++;      //  Копируем бит из позиции numBitIn байта numByteIn (значение valByteIn) в позицию numBitOut символа numByteOut, увеличивая значение numBitOut после каждого копирования.
      if(numBitOut>7){                                                                  //  
        SERIAL_PRINT(_char(valByteOut/16));                                             //  
        SERIAL_PRINT(_char(valByteOut%16));                                             //  
        valByteOut=0; numBitOut=0;                                                      //  
      }                                                                                 //  При достижении numBitOut позиции старшего бита в символе (numBitOut>=7), обнуляем старший бит символа (txt[numByteOut]&=0x7F), обнуляем позицию бита в символе (numBitOut=0), переходим к следующему символу (numByteOut++).
    }                                                                                   //  
  }                                                                                     //  
  if(numBitOut){                                                                        //  
    SERIAL_PRINT(_char(valByteOut/16));                                                 //  
    SERIAL_PRINT(_char(valByteOut%16));                                                 //  
  }                                                                                     //  
  return pos+numByteIn;                                                                 //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ GSM ТЕКСТА SMS СООБЩЕНИЯ ИЗ СТРОКИ                         //
void  MODEM::_SMSdecodGSM(char* txt, uint16_t len, uint16_t pos, uint16_t udh_len)      //
{ //  Аргументы функции:  txt - строка для текста, len - количество символов в тексте, pos - позиция начала текста в строке, udh_len количество байт занимаемое заголовком.
  if(udh_len>0){ len -= udh_len*8/7; if(udh_len*8%7){len--;} }                          //  Если текст начинается с заголовка, то уменьшаем количество символов в тексте на размер заголовка.
  uint8_t  valByteIn  = 0;                                                              //  Определяем временную переменную для хранения значения очередного читаемого байта из строки.
  uint16_t numByteIn  = udh_len*2;                                                      //  Определяем временную переменную для хранения номера   очередного читаемого байта из строки.
  uint8_t  numBitIn   = udh_len==0?0:(7-(udh_len*8%7));                                 //  Определяем временную переменную для хранения номера   очередного бита в байте numByteIn.
  uint16_t numByteOut = 0;                                                              //  Определяем временную переменную для хранения номера   очередного раскодируемого символа для строки txt.
  uint8_t  numBitOut  = 0;                                                              //  Определяем временную переменную для хранения номера   очередного бита в байте numByteOut.
                                                                                        //
  while(numByteOut<len){                                                                //  Пока номер очередного раскодируемого символа не станет больше объявленного количества символов.
    valByteIn = _num(*(text->GetText()+pos+numByteIn))*16 +                             //  Читаем значение байта с номером numByteIn в переменную valByteIn.
                _num(*(text->GetText()+pos+numByteIn+1)); numByteIn+=2;                 //
    while(numBitIn<8){                                                                  //  Проходим по битам байта numByteIn (номер бита хранится в numBitIn).
      bitWrite( txt[numByteOut], numBitOut, bitRead(valByteIn,numBitIn) ); numBitOut++; //  Копируем бит из позиции numBitIn байта numByteIn (значение valByteIn) в позицию numBitOut символа numByteOut, увеличивая значение numBitOut после каждого копирования.
      if(numBitOut>=7){ txt[numByteOut]&=0x7F; numBitOut=0; numByteOut++;}              //  При достижении numBitOut позиции старшего бита в символе (numBitOut>=7), обнуляем старший бит символа (txt[numByteOut]&=0x7F), обнуляем позицию бита в символе (numBitOut=0), переходим к следующему символу (numByteOut++).
      numBitIn++;                                                                       //  
    } numBitIn=0;                                                                       //  
  }                                                                                     //
  txt[numByteOut]=0;                                                                    //  Присваиваем символу len+1 значение 0 (конец строки).
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ 8BIT ТЕКСТА SMS СООБЩЕНИЯ:                                 //
void  MODEM::_SMSdecod8BIT(char* txt, uint16_t len, uint16_t pos){                      //  Аргументы функции:  txt - строка для текста, len - количество байт в тексте, pos - позиция начала текста в строке.
  uint16_t numByteIn  = 0;                                                              //  Определяем временную переменную для хранения номера   очередного читаемого байта из строки.
  uint16_t numByteOut = 0;                                                              //  Определяем временную переменную для хранения номера очередного раскодируемого символа для строки txt.
                                                                                        //  
  while(numByteOut<len){                                                                //  Пока номер очередного раскодируемого символа не станет больше объявленного количества символов.
    txt[numByteOut]= _num(*(text->GetText()+pos+numByteIn))*16 + _num(*(text->GetText()+pos+numByteIn+1)); numByteIn+=2; numByteOut++;      //  Читаем значение байта с номером numByteIn в символ строки txt под номером numByteOut.
  } txt[numByteOut]=0;                                                                  //  Присваиваем символу len+1 значение 0 (конец строки).
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ КОДИРОВАНИЯ ТЕКСТА SMS СООБЩЕНИЯ В ФОРМАТЕ UCS2:                          //  Функция возвращает: число uint16_t соответствующее позиции после последнего закодированного символа из строки txt.
uint16_t MODEM::_SMScoderUCS2(const char* txt, uint16_t pos, uint16_t len)              //  Аргументы функции:  txt - строка с текстом, pos - позиция взятия первого символа из строки txt, len - количество кодируемых символов из строки txt.
{                                                                                       //
  uint8_t  valByteInThis  = 0;                                                          //  Определяем временную переменную для хранения значения очередного читаемого байта.
  uint8_t  valByteInNext  = 0;                                                          //  Определяем временную переменную для хранения значения следующего читаемого байта.
  uint16_t numByteIn      = pos;                                                        //  Определяем временную переменную для хранения номера очередного   читаемого байта.
  uint16_t numSymbIn      = 0;                                                          //  Определяем временную переменную для хранения номера очередного   читаемого символа.
  uint8_t  lenTXT         = strlen(txt);                                                //  Определяем временную переменную для хранения длины строки в байтах.
  switch(codTXTsend){                                                                   //  Тип кодировки строки StrIn.
    //        Преобразуем текст из кодировки UTF-8 кодировку UCS2: (и записываем его в строку в формате текстового HEX)             //
    case GSM_TXT_UTF8:                                                                  //
      while(numSymbIn<len && numByteIn<lenTXT)                                          //
      { //  Пока номер очередного читаемого символа не станет больше объявленного количества кодируемых символов или не превысит длину строки.
        valByteInThis = uint8_t(txt[numByteIn  ]);                                      //  Читаем значение очередного байта.
        valByteInNext = uint8_t(txt[numByteIn+1]);                                      //  Читаем значение следующего байта.
        numSymbIn++;                                                                    //  Увеличиваем количество прочитанных символов.
        if (valByteInThis==0x00) { return numByteIn; }                                  //  Очередной прочитанный символ является символом конца строки, возвращаем номер прочитанного байта.
        else if (valByteInThis < 0x80)                                                  //
        {                                                                               //
          numByteIn+=1;  SERIAL_PRINT(F("00"));                                         //  Очередной прочитанный символ состоит из 1 байта и является символом латинского алфавита, записываем 00 и его значение.
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем его старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем его младший полубайт.
        }                                                                               //
        else if (valByteInThis==0xD0)                                                   //
        {                                                                               //
          numByteIn+=2; SERIAL_PRINT(F("04"));                                          //  Очередной прочитанный символ состоит из 2 байт и является символом Русского алфавита, записываем 04 и проверяем значение байта valByteInNext ...
          if (valByteInNext==0x81) { SERIAL_PRINT(F("01")); }                           //  Символ  'Ё' - 208 129 => 04 01
          if((valByteInNext>=0x90)&&(valByteInNext<=0xBF))                              //
          {                                                                             //
            SERIAL_PRINT(_char((valByteInNext-0x80)/16));                               //  Символы 'А-Я,а-п' - 208 144 - 208 191  =>  04 16 - 04 63
            SERIAL_PRINT(_char((valByteInNext-0x80)%16));                               //
          }                                                                             //  
        }                                                                               //
        else if (valByteInThis==0xD1)                                                   //
        {                                                                               //
          numByteIn+=2; SERIAL_PRINT(F("04"));                                          //  Очередной прочитанный символ состоит из 2 байт и является символом Русского алфавита, записываем 04 и проверяем значение байта valByteInNext ...
          if (valByteInNext==0x91) { SERIAL_PRINT(F("51")); }                           //  Символ 'ё' - 209 145 => 04 81
          if((valByteInNext>=0x80)&&(valByteInNext<=0x8F))                              //
          {                                                                             //
            SERIAL_PRINT(_char((valByteInNext-0x40)/16));                               //  Символы 'р-я' - 209 128 - 209 143 => 04 64 - 04 79
            SERIAL_PRINT(_char((valByteInNext-0x40)%16));                               //
          }                                                                             //  
        }                                                                               //
        else if (valByteInThis <0xE0) { numByteIn+=2; SERIAL_PRINT(F("043F")); }        //  Очередной прочитанный символ состоит из 2 байт, записываем его как символ '?'
        else if (valByteInThis <0xF0) { numByteIn+=3; SERIAL_PRINT(F("003F")); }        //  Очередной прочитанный символ состоит из 3 байт, записываем его как символ '?'
        else if (valByteInThis <0xF8) { numByteIn+=4; SERIAL_PRINT(F("003F")); }        //  Очередной прочитанный символ состоит из 4 байт, записываем его как символ '?'
        else { numByteIn+=5; SERIAL_PRINT(F("003F")); }                                 //  Очередной прочитанный символ состоит из 5 и более байт, записываем его как символ '?'
      }                                                                                 //
      break;                                                                            //
                                                                                        //
//        Преобразуем текст из кодировки CP866 в кодировку UCS2: (и записываем его в строку в формате текстового HEX)           //  
    case GSM_TXT_CP866:                                                                 //
      while(numSymbIn<len && numByteIn<lenTXT)                                          //
      {                                                                                 //  Пока номер очередного читаемого символа не станет больше объявленного количества кодируемых символов или не превысит длину строки.
        valByteInThis = uint8_t(txt[numByteIn]);                                        //  Читаем значение очередного символа.
        numSymbIn++; numByteIn++;                                                       //  Увеличиваем количество прочитанных символов и номер прочитанного байта.
        if (valByteInThis==0x00) { return numByteIn; }                                  //  Очередной прочитанный символ является символом конца строки, возвращаем номер прочитанного байта.
        else if (valByteInThis <0x80)                                                   //
        {                                                                               //
          SERIAL_PRINT(F("00"));                                                        //  Очередной прочитанный символ состоит из 1 байта и является символом латинского алфавита, записываем 00 и его значение.
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем его старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем его младший полубайт.
        }                                                                               //
        else if (valByteInThis==0xF0) { SERIAL_PRINT(F("0401")); }                      //  Символ 'Ё' - 240 => 04 01 записываем его байты.
        else if (valByteInThis==0xF1) { SERIAL_PRINT(F("0451")); }                      //  Символ 'e' - 241 => 04 81 записываем его байты.
        else if((valByteInThis>=0x80)&&(valByteInThis<=0xAF))                           //
        {                                                                               //
          SERIAL_PRINT(F("04")); valByteInThis-=0x70;                                   //  Символы 'А-Я,а-п' - 128 - 175 => 04 16 - 04 63 записываем 04 и вычисляем значение для кодировки UCS2:
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
        else if((valByteInThis>=0xE0)&&(valByteInThis<=0xEF))                           //
        {                                                                               //
          SERIAL_PRINT(F("04")); valByteInThis-=0xA0;                                   //  Символы 'р-я'     - 224 - 239      =>  04 64 - 04 79                 записываем 04 и вычисляем значение для кодировки UCS2:
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
      }                                                                                 //
      break;                                                                            //
//        Преобразуем текст из кодировки Windows1251 в кодировку UCS2: (и записываем его в формате текстового HEX)         //  
    case GSM_TXT_WIN1251:                                                               //
      while(numSymbIn<len && numByteIn<lenTXT)                                          //
      {                                                                                 //  Пока номер очередного читаемого символа не станет больше объявленного количества кодируемых символов или не превысит длину строки.
        valByteInThis = uint8_t(txt[numByteIn]);                                        //  Читаем значение очередного символа.
        numSymbIn++; numByteIn++;                                                       //  Увеличиваем количество прочитанных символов и номер прочитанного байта.
        if (valByteInThis==0x00) { return numByteIn; }                                  //  Очередной прочитанный символ является символом конца строки, возвращаем номер прочитанного байта.
        else if (valByteInThis <0x80)                                                   //
        {                                                                               //
          SERIAL_PRINT(F("00"));                                                        //  Очередной прочитанный символ состоит из 1 байта и является символом латинского алфавита, записываем 00 и его значение.
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем его старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем его младший полубайт.
        }                                                                               //
        else if (valByteInThis==0xA8) { SERIAL_PRINT(F("0401")); }                      //  Символ 'Ё' - 168 => 04 01 записываем его байты.
        else if (valByteInThis==0xB8) { SERIAL_PRINT(F("0451")); }                      //  Символ 'e' - 184 => 04 81 записываем его байты.
        else if((valByteInThis>=0xC0)&&(valByteInThis<=0xEF))                           //
        {                                                                               //
          SERIAL_PRINT(F("04")); valByteInThis-=0xB0;                                   //  Символы 'А-Я,а-п' - 192 - 239 => 04 16 - 04 63 записываем 04 и вычисляем значение для кодировки UCS2:
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
        else if((valByteInThis>=0xF0)&&(valByteInThis<=0xFF))                           //
        {                                                                               //
          SERIAL_PRINT(F("04")); valByteInThis-=0xB0;                                   //  Символы 'р-я' - 240 - 255 => 04 64 - 04 79 записываем 04 и вычисляем значение для кодировки UCS2:
          SERIAL_PRINT(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          SERIAL_PRINT(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
      }                                                                                 //
      break;                                                                            //
  }                                                                                     //  
  return numByteIn;                                                                     //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ UCS2 ТЕКСТА SMS СООБЩЕНИЯ ИЗ строки                        //
void  MODEM::_SMSdecodUCS2(char* txt, uint16_t len, uint16_t pos){                      //  Аргументы функции:  txt - строка для текста, len - количество байт в тексте, pos - позиция начала текста в строке.
  uint8_t  byteThis = 0;                                                                //  Определяем временную переменную для хранения значения очередного читаемого байта.
  uint8_t  byteNext = 0;                                                                //  Определяем временную переменную для хранения значения следующего читаемого байта.
  uint16_t numIn    = 0;                                                                //  Определяем временную переменную для хранения номера   очередного читаемого байта.
  uint16_t numOut   = 0;                                                                //  Определяем временную переменную для хранения номера   очередного раскодируемого символа.
                                                                                        // 
  len*=2;                                                                               //  Один байт данных занимает 2 символа в строке.
  switch(codTXTread){                                                                   //  Тип кодировки строки StrIn.
//        Преобразуем текст из кодировки UCS2 в кодировку UTF-8:                        //
    case GSM_TXT_UTF8:                                                                  //
      while(numIn<len){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
        byteThis = _num(*(text->GetText()+pos+numIn))*16 +                              //
                   _num(*(text->GetText()+pos+numIn+1)); numIn+=2;                      //  Читаем значение очередного байта в переменную byteThis.
        byteNext = _num(*(text->GetText()+pos+numIn))*16 +                              //
                   _num(*(text->GetText()+pos+numIn+1)); numIn+=2;                      //  Читаем значение следующего байта в переменную byteNext.
        if(byteThis==0x00){                            txt[numOut]=byteNext;      numOut++;}else //  Символы латинницы
        if(byteNext==0x01){txt[numOut]=0xD0; numOut++; txt[numOut]=byteNext+0x80; numOut++;}else //  Символ  'Ё'       - 04 01         => 208 129
        if(byteNext==0x51){txt[numOut]=0xD1; numOut++; txt[numOut]=byteNext+0x40; numOut++;}else //  Символ  'ё'       - 04 81         => 209 145
        if(byteNext< 0x40){txt[numOut]=0xD0; numOut++; txt[numOut]=byteNext+0x80; numOut++;}else //  Символы 'А-Я,а-п' - 04 16 - 04 63 => 208 144 - 208 191
                          {txt[numOut]=0xD1; numOut++; txt[numOut]=byteNext+0x40; numOut++;}     //  Символы 'р-я'     - 04 64 - 04 79 => 209 128 - 209 143
      }                                                                                 //
      txt[numOut]=0;                                                                    //
      break;                                                                            //
//        Преобразуем текст из кодировки UCS2 в кодировку CP866:                        //
    case GSM_TXT_CP866:                                                                 //
      while(numIn<len){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
        byteThis = _num(*(text->GetText()+pos+numIn))*16 + _num(*(text->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение очередного байта в переменную byteThis.
        byteNext = _num(*(text->GetText()+pos+numIn))*16 + _num(*(text->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение следующего байта в переменную byteNext.
        if(byteThis==0x00){txt[numOut]=byteNext;      numOut++;}else                    //  Символы латинницы
        if(byteNext==0x01){txt[numOut]=byteNext+0xEF; numOut++;}else                    //  Символ  'Ё'       - 04 01          =>  240
        if(byteNext==0x51){txt[numOut]=byteNext+0xA0; numOut++;}else                    //  Символ  'ё'       - 04 81          =>  141
        if(byteNext< 0x40){txt[numOut]=byteNext+0x70; numOut++;}else                    //  Символы 'А-Я,а-п' - 04 16 - 04 63  =>  128 - 175
                          {txt[numOut]=byteNext+0xA0; numOut++;}                        //  Символы 'р-я'     - 04 64 - 04 79  =>  224 - 239
      }                                                                                 //
      txt[numOut]=0;                                                                    //
      break;                                                                            //
//        Преобразуем текст из кодировки UCS2 в кодировку Windows1251:                  //
    case GSM_TXT_WIN1251:                                                               //
      while(numIn<len){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
        byteThis = _num(*(text->GetText()+pos+numIn))*16 + _num(*(text->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение очередного байта в переменную byteThis.
        byteNext = _num(*(text->GetText()+pos+numIn))*16 + _num(*(text->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение следующего байта в переменную byteNext.
        if(byteThis==0x00){txt[numOut]=byteNext;      numOut++;}else                    //  Символы латинницы
        if(byteNext==0x01){txt[numOut]=byteNext+0xA7; numOut++;}else                    //  Символ  'Ё'       - 04 01          =>  168
        if(byteNext==0x51){txt[numOut]=byteNext+0x67; numOut++;}else                    //  Символ  'ё'       - 04 81          =>  184
        if(byteNext< 0x40){txt[numOut]=byteNext+0xB0; numOut++;}else                    //  Символы 'А-Я,а-п' - 04 16 - 04 63  =>  192 - 239
                          {txt[numOut]=byteNext+0xB0; numOut++;}                        //  Символы 'р-я'     - 04 64 - 04 79  =>  240 - 255
      }                                                                                 //
      txt[numOut]=0;                                                                    //
      break;                                                                            //
  }                                                                                     //
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ КОДИРОВАНИЯ АДРЕСА SMS СООБЩЕНИЯ В СТРОКУ:                                //
void  MODEM::_SMScoderAddr(const char* num){                                            //  Аргументы функции:  num - строка с адресом для кодирования.
  uint16_t j=num[0]=='+'?1:0, len=strlen(num);                                          //  Определяем временные переменные.
                                                                                        //
  for(uint16_t i=j; i<len; i+=2){                                                       //  
    if( (len<=(i+1)) || (num[i+1]==0) ){SERIAL_PRINT('F');}else{SERIAL_PRINT(num[i+1]);}//  Отправляем следующий символ из строки num, если символа в строке num нет, то отправляем символ 'F'.
    if( (len<= i   ) || (num[i  ]==0) ){SERIAL_PRINT('F');}else{SERIAL_PRINT(num[i  ]);}//  Отправляем текущий   символ из строки num, если символа в строке num нет, то отправляем символ 'F'.
  }                                                                                     //
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ АДРЕСА SMS СООБЩЕНИЯ ИЗ СТРОКИ:                            //
void  MODEM::_SMSdecodAddr(char* num, uint16_t len, uint16_t pos){                      //  Аргументы функции:  num - строка для получения адреса.
  uint8_t j=0;                                                                          //            len - количество полубайт в адресе (количество символов в номере).
                                                                                        //
  for(uint16_t i=0; i<len; i+=2){                                                       //            pos - позиция адреса в строке.
    if( *(text->GetText()+pos+i+1)!='F' && *(text->GetText()+pos+i+1)!='f'){num[i]=*(text->GetText()+pos+i+1); j++;} //  Сохраняем следующий символ из строки на место текущего в строке num, если этот символ не 'F' или 'f'.
    if( *(text->GetText()+pos+i  )!='F' && *(text->GetText()+pos+i  )!='f'){num[i+1]=*(text->GetText()+pos+i); j++;} //  Сохраняем текущий   символ из строки на место следующего в строке num, если этот символ не 'F' или 'f'.
  } num[j]=0;                                                                           //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ ДАТЫ ОТПРАВКИ SMS СООБЩЕНИЯ ИЗ СТРОКИ:                     //
void  MODEM::_SMSdecodDate(char* tim, uint16_t pos){                                    //  Аргументы функции:  tim - строка для даты
  tim[ 0]=*(text->GetText()+pos+5);                                                     //  ст. день.     pos - позиция даты в строке
  tim[ 1]=*(text->GetText()+pos+4);                                                     //  мл. день.
  tim[ 2]='.';                                                                          //  
  tim[ 3]=*(text->GetText()+pos+3);                                                     //  ст. мес.
  tim[ 4]=*(text->GetText()+pos+2);                                                     //  мл. мес.
  tim[ 5]='.';                                                                          //  
  tim[ 6]=*(text->GetText()+pos+1);                                                     //  ст. год.
  tim[ 7]=*(text->GetText()+pos+0);                                                     //  мл. год.
  tim[ 8]=' ';                                                                          //  
  tim[ 9]=*(text->GetText()+pos+7);                                                     //  ст. час.
  tim[10]=*(text->GetText()+pos+6);                                                     //  мл. час.
  tim[11]=':';                                                                          //  
  tim[12]=*(text->GetText()+pos+9);                                                     //  ст. мин.
  tim[13]=*(text->GetText()+pos+8);                                                     //  мл. мин.
  tim[14]=':';                                                                          //  
  tim[15]=*(text->GetText()+pos+11);                                                    //  ст. сек.
  tim[16]=*(text->GetText()+pos+10);                                                    //  мл. сек.
  tim[17]=0;                                                                            //  конец строки.
}                                                                                       //  
                                                                                        //  
/*                                                                                      //
  ФУНКЦИЯ ЧТЕНИЯ ВХОДЯЩЕГО SMS СООБЩЕНИЯ:                                               //
  Функция возвращает: ничего                                                            //
*/                                                                                      //
void  MODEM::PDUread(SMS_INFO* sms){ //  Аргументы функции:  SMS_INFO).                 //
//      Готовим переменные:                                                             //  
  uint8_t  i              = 0;                                                          //
  bool   f                = 0;                                                          //  Флаг указывающий на успешное чтение сообщения из памяти в строку.
  uint8_t  PDU_SCA_LEN    = 0;                                                          //  Первый байт блока SCA, указывает количество оставшихся байт в блоке SCA.
  uint8_t  PDU_SCA_TYPE   = 0;                                                          //  Второй байт блока SCA, указывает формат адреса сервисного центра службы коротких сообщений.
  uint8_t  PDU_SCA_DATA   = 0;                                                          //  Позиция первого байта блока SCA, содержащего адрес сервисного центра службы коротких сообщений.
  uint8_t  PDU_TYPE       = 0;                                                          //  Первый байт блока TPDU, указывает тип PDU, состоит из флагов RP UDHI SRR VPF RD MTI (их назначение совпадает с первым байтом команды AT+CSMP).
  uint8_t  PDU_OA_LEN     = 0;                                                          //  Первый байт блока OA, указывает количество полезных (информационных) полубайтов в блоке OA.
  uint8_t  PDU_OA_TYPE    = 0;                                                          //  Второй байт блока OA, указывает формат адреса отправителя сообщения.
  uint8_t  PDU_OA_DATA    = 0;                                                          //  Позиция третьего байта блока OA, это первый байт данных содержащих адрес отправителя сообщения.
  uint8_t  PDU_PID        = 0;                                                          //  Первый байт после блока OA, указывает на идентификатор (тип) используемого протокола.
  uint8_t  PDU_DCS        = 0;                                                          //  Второй байт после блока OA, указывает на схему кодирования данных (кодировка текста сообщения).
  uint8_t  PDU_SCTS_DATA  = 0;                                                          //  Позиция первого байта блока SCTS, содержащего дату и время отправки сообщения.
  uint8_t  PDU_UD_LEN     = 0;                                                          //  Первый байт блока UD (следует за блоком SCTS), указывает на количество символов (7-битной кодировки) или количество байт (остальные типы кодировок) в блоке UD.
  uint8_t  PDU_UD_DATA    = 0;                                                          //  Позиция второго байта блока UD, с данного байта начинается текст SMS или блок заголовка (UDH), зависит от флага UDHI в байте PDUT (PDU_TYPE).
  uint8_t  PDU_UDH_LEN    = 0;                                                          //  Первый байт блока UDH, указывает количество оставшихся байт в блоке UDH. (блок UDH отсутствует если сброшен флаг UDHI в байте PDU_TYPE).
  uint8_t  PDU_UDH_IEI    = 0;                                                          //  Второй байт блока UDH является первым байтом блока IEI, указывает на назначение заголовка. Для составных SMS значение IEI равно 0x00 или 0x08. Если IEI равно 0x00, то блок IED1 занимает 1 байт, иначе IED1 занимает 2 байта.
  uint8_t  PDU_UDH_IED_LEN= 0;                                                          //  Первый байт после блока IEI, указывает на количество байт в блоке IED состояшем из IED1,IED2,IED3. Значение данного байта не учитывается в данной библиотеке.
  uint16_t PDU_UDH_IED1   = 0;                                                          //  Данные следуют за байтом IEDL (размер зависит от значения PDU_IEI). Для составных SMS значение IED1 определяет идентификатор длинного сообщения (все SMS в составе длинного сообщения должны иметь одинаковый идентификатор).
  uint8_t  PDU_UDH_IED2   = 1;                                                          //  Предпоследний байт блока UDH. Для составных SMS его значение определяет количество SMS в составе длинного сообщения.
  uint8_t  PDU_UDH_IED3   = 1;                                                          //  Последний байт блока UDH. Для составных SMS его значение определяет номер данной SMS в составе длинного сообщения.

  sms->txt[0]=0; sms->num[0]=0; sms->tim[0]=0; sms->lngID=0; sms->lngSum=1; sms->lngNum=1;  //  Чистим данные по ссылкам и указателям для ответа.

//      Определяем значения из PDU блоков SMS сообщения находящегося в строке text:                                      //  
//      |                                                  PDU                                                   |       //  
//      +------------------+-------------------------------------------------------------------------------------+       //  
//      |                  |                                        TPDU                                         |       //  
//      |                  +-----------------------------------------+-------------------------------------------+       //  
//      |        SCA       |                                         |                    UD                     |       //  
//      |                  |      +---------------+                  |     +--------------------------------+    |       //  
//      |                  |      |      OA       |                  |     |             [UDH]              |    |       //  
//      +------------------+------+---------------+-----+-----+------+-----+--------------------------------+----+       //  
//      | SCAL [SCAT SCAD] | PDUT | OAL [OAT OAD] | PID | DCS | SCTS | UDL | [UDHL IEI IEDL IED1 IED2 IED3] | UD |       //  
//                                                                              //  
//  SCAL (Service Center Address Length) - байт указывающий размер адреса сервисного центра коротких сообщений.          //  
//  PDU_SCA_LEN   = _num(text->GetChar())*16 + _num(text->GetChar()); //  Получаем значение  первого байта блока SCA (оно указывает на количество оставшихся байт в блоке SCA).
//  SCAT (Service Center Address Type) - байт хранящий тип адреса сервисного центра коротких сообщений.                  //  
//  PDU_SCA_TYPE  = _num(text->GetChar())*16 + _num(text->GetChar()); //  Получаем значение  второго байта блока SCA (тип адреса: номер, текст, ... ).
//  SCAD (Service Center Address Date) - блок адреса сервисного центра коротких сообщений. С третьего байта начинается сам адрес.       //  
//  SCAL (Service Center Address Length) - байт указывающий размер адреса сервисного центра коротких сообщений.          //  
  PDU_SCA_LEN   = _num(*(text->GetText()))*16 + _num(*(text->GetText()+1));             //  Получаем значение  первого байта блока SCA (оно указывает на количество оставшихся байт в блоке SCA).
  i = 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
  //  SCAT (Service Center Address Type) - байт хранящий тип адреса сервисного центра коротких сообщений.                //  
  PDU_SCA_TYPE  = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));         //  Получаем значение  второго байта блока SCA (тип адреса: номер, текст, ... ).
  i+= 2;                                                                                //  
  PDU_SCA_DATA  = i;                                                                    //  Сохраняем позицию  третьего байта блока SCA (для получения адреса в дальнейшем).
  i+= PDU_SCA_LEN*2 - 2;                                                                //  Смещаем курсор на PDU_SCA_LEN байт после байта PDU_SCA_LEN.
//  PDUT (Packet Data Unit Type) - байт состоящий из флагов определяющих тип блока PDU. //  
  PDU_TYPE    = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));           //  Получаем значение  байта PDU_TYPE (оно состоит из флагов RP UDHI SRR VPF RD MTI).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  OAL (Originator Address Length) - байт указывающий размер адреса отправителя.       //  
  PDU_OA_LEN    = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));         //  Получаем значение  первого байта блока OA (оно указывает на количество полезных полубайтов в блоке OA).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  OAT (Originator Address Type) - байт хранящий тип адреса отправителя.               //  
  PDU_OA_TYPE   = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));         //  Получаем значение  второго байта блока OA (тип адреса: номер, текст, ... ).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  OAD (Originator Address Date) - адрес отправителя. С третьего байта начинается сам адрес, его размер равен PDU_OA_LEN полубайтов.     //  
  PDU_OA_DATA   = i;                                                                    //  Сохраняем позицию  третьего байта блока OA (для получения адреса в дальнейшем).
  i+= (PDU_OA_LEN + (PDU_OA_LEN%2));                                                    //  Смещаем курсор на значение PDU_OA_LEN увеличенное до ближайшего чётного.
//  PID (Protocol Identifier) - идентификатор протокола передачи данных, по умолчанию байт равен 00.                     //  
  PDU_PID     = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));           //  Получаем значение  байта PID (идентификатор протокола передачи данных).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  DCS (Data Coding Scheme) - схема кодирования данных (кодировка текста сообщения).   //  
  PDU_DCS     = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));           //  Получаем значение  байта DCS (схема кодирования данных).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  SCTS (Service Center Time Stam) - отметка о времени получения сообщения в сервис центр коротких сообщений.           //  
  PDU_SCTS_DATA = i;                                                                    //  Сохраняем позицию  первого байта блока SCTS (для получения даты и времени в дальнейшем).
  i+= 14;                                                                               //  Смещаем курсор на 14 полубайт (7 байт).
//  UDL (User Data Length) - размер данных пользователя.                                //  
  PDU_UD_LEN    = _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1));         //  Получаем значение  байта UDL (размер данных пользователя). Для 7-битной кодировки - количество символов, для остальных - количество байт.
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  UD (User Data) - данные пользователя (заголовок и текст SMS сообщения).             //  
  PDU_UD_DATA   = i;                                                                    //  Позиция первого байта блока UD (данные). Блок UD может начинаться с блока UDH (заголовок), если установлен флаг UDHI в байте PDU_TYPE, а уже за ним будет следовать данные текста SMS.
//  UDHL (User Data Header Length) - длина заголовока в данных пользователя.            //  
  PDU_UDH_LEN   = (PDU_TYPE & 0b01000000)? _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1)) : 0; //  Получаем значение  первого байта блока UDH (оно указывает на количество оставшихся байт в блоке UDH). Блок UDH отсутствует если сброшен флаг UDHI в байте PDU_TYPE, иначе блок UD начинается с блока UDH.
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  IEI (Information Element Identifier) - идентификатор информационного элемента.      //  
  PDU_UDH_IEI   = (PDU_UDH_LEN)? _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1)) : 0; //  Получаем значение  первого байта блока IEI (блок указывает на назначение заголовка). Для составных SMS блок IEI состоит из 1 байта, а его значение равно 0x00 или 0x08. Если IEI равно 0x00, то блок IED1 занимает 1 байт, иначе IED1 занимает 2 байта.
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  IEDL (Information Element Data Length) - длина данных информационных элементов.     //  
  PDU_UDH_IED_LEN = (PDU_UDH_LEN)? _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1)) : 0; //  Получаем значение  первого байта после блока IEI (оно указывает на количество байт в блоке IED состояшем из IED1,IED2,IED3). Значение данного байта не учитывается в данной библиотеке.
  i+= PDU_UDH_LEN*2 - 4;                                                                //  Смещаем курсор к последнему байту блока UDH.
//  IED3 (Information Element Data 3) - номер SMS в составе составного длинного сообщения.  //  
  PDU_UDH_IED3  = (PDU_UDH_LEN)? _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1)) : 0; //  Получаем значение  последнего байта блока UDH. (оно указывает на номер SMS в составе составного длинного сообщения).
  i-= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт) к началу.
//  IED2 (Information Element Data 2) - количество SMS в составе составного длинного сообщения. //  
  PDU_UDH_IED2  = (PDU_UDH_LEN)? _num(*(text->GetText()+i))*16 + _num(*(text->GetText()+i+1)) : 0; //  Получаем значение  предпоследнего байта блока UDH. (оно указывает на количество SMS в составе составного длинного сообщения).
  i-= 2; if(PDU_UDH_IEI){i-= 2;}                                                        //  Смещаем курсор на 2 или 4 полубайта (1 или 2 байта) к началу.
//  IED1 (Information Element Data 1) - идентификатор длинного сообщения.               //  
  PDU_UDH_IED1  = (PDU_UDH_IEI)? _num(*(text->GetText()+i))*4096 + _num(*(text->GetText()+i+1))*256 + _num(*(text->GetText()+i+2))*16 + _num(*(text->GetText()+i+3)) :
  _num(*(text->GetText()+i))*16   + _num(*(text->GetText()+i+1));                       //  Получаем значение  идентификатора длинного сообщения (все SMS в составе длинного сообщения должны иметь одинаковый идентификатор).
//      Выполняем дополнительные преобразования значений блоков для удобства дальнейшей работы: //  
//  Вычисляем схему кодирования данных (текста SMS сообщения):                          //  
  if((PDU_DCS&0xF0)==0xC0){PDU_DCS=0;}else if((PDU_DCS&0xF0)==0xD0){PDU_DCS=0;}else if((PDU_DCS&0xF0)==0xE0){PDU_DCS=2;}else{PDU_DCS=(PDU_DCS&0x0C)>>2;}  //  PDU_DCS = 0-GSM, 1-8бит, 2-UCS2.
//  Вычисляем тип адреса отправителя:                                                   //  
  if((PDU_OA_TYPE-(PDU_OA_TYPE%16))==0xD0){PDU_OA_TYPE=1;}else{PDU_OA_TYPE=0;}          //  PDU_OA_TYPE = 0 - адресом отправителя является номер телефона, 1 - адрес отправителя указан в алфавитноцифровом формате.
//  Вычисляем длину адреса отправителя:                                                 //  
  if(PDU_OA_TYPE){PDU_OA_LEN=(PDU_OA_LEN/2)+(PDU_OA_LEN/14);}                           //  PDU_OA_LEN = количество символов для адреса отправителя в алфавитноцифровом формате, количество цифр для адреса указанного в виде номера телефона отправителя.
//  Вычисляем длину блока UDH вместе с его первым байтом:                               //  
  if(PDU_UDH_LEN>0){PDU_UDH_LEN++;}                                                     //  PDU_UDH_LEN теперь указывает не количество оставшихся байт в блоке UDH, а размер всего блока UDH (добавили 1 байт занимаемый самим байтом PDU_UDH_LEN).
//      Расшифровка SMS сообщения:                                                      //  
//  Получаем адрес отправителя.                                                         //
  if(PDU_OA_TYPE) {_SMSdecodGSM (sms->num, PDU_OA_LEN, PDU_OA_DATA);}                   //  Если адрес отправителя указан в алфавитноцифровом формате, то декодируем адрес отправителя как текст в 7-битной кодировке в строку num.
  else      {_SMSdecodAddr(sms->num, PDU_OA_LEN, PDU_OA_DATA);}                         //  Иначе декодируем адрес отправителя как номер в строку num.
//  Получаем дату отправки сообщения (дату получения SMS сервисным центром).            //  
  _SMSdecodDate(sms->tim, PDU_SCTS_DATA);                                               //  В строку tim вернётся текст содержания "ДД.ММ.ГГ ЧЧ.ММ.СС".
                                                                                        //
//  Получаем текст сообщения:                                                           //  
  if(PDU_DCS==0){_SMSdecodGSM ( sms->txt, PDU_UD_LEN            , PDU_UD_DATA, PDU_UDH_LEN    );} //  Получаем текст сообщения принятого в кодировке GSM.
  if(PDU_DCS==1){_SMSdecod8BIT( sms->txt, PDU_UD_LEN-PDU_UDH_LEN, PDU_UD_DATA+(PDU_UDH_LEN*2) );} //  Получаем текст сообщения принятого в 8-битной кодировке.
  if(PDU_DCS==2){_SMSdecodUCS2( sms->txt, PDU_UD_LEN-PDU_UDH_LEN, PDU_UD_DATA+(PDU_UDH_LEN*2) );} //  Получаем текст сообщения принятого в кодировке UCS2.
//  Возвращаем параметры составного сообщения:                                          //
  if(PDU_UDH_LEN>1){                                                                    //
    sms->lngID  = PDU_UDH_IED1;                                                         //  Идентификатор составного сообщения.
    sms->lngSum = PDU_UDH_IED2;                                                         //  Количество SMS в составном сообщении.
    sms->lngNum = PDU_UDH_IED3;                                                         //  Номер данной SMS в составном сообщении.
  }                                                                                     //
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ОТПРАВКИ SMS СООБЩЕНИЯ:                                                   //  Функция возвращает: флаг успешной отправки SMS сообщения true/false
bool  MODEM::SMSsend(const char* txt, const char* num, uint16_t lngID, uint8_t lngSum, uint8_t lngNum){ //  Аргументы функции:  txt - передаваемый текст, num - номер получателя, lngID - идентификатор склеенных SMS, lngSum - количество склеенных SMS, lngNum - номер данной склеенной SMS.
//      Готовим переменные:                                                             //  
  uint16_t txtLen      = _SMStxtLen(txt);                                               //  Количество символов (а не байтов) в строке.
  uint8_t  PDU_TYPE     = lngSum>1?0x41:0x01;;                                          //  Первый байт блока TPDU, указывает тип PDU, состоит из флагов RP UDHI SRR VPF RD MTI (их назначение совпадает с первым байтом команды AT+CSMP). Если сообщение составное (склеенное), то устанавливаем флаг UDHI для передачи заголовка.
  uint8_t  PDU_DA_LEN   = strlen(num); if(num[0]=='+'){PDU_DA_LEN--;}                   //  Первый байт блока DA, указывает количество полезных (информационных) полубайтов в блоке DA. Так как адрес получателя указывается ввиде номера, то значение данного блока равно количеству цифр в номере.
  uint8_t  PDU_DCS      = 0x00;                                                         //  Второй байт после блока DA, указывает на схему кодирования данных (кодировка текста сообщения).
  uint8_t  PDU_UD_LEN   = 0x00;                                                         //  Первый байт блока UD, указывает на количество символов (в 7-битной кодировке GSM) или количество байт (остальные типы кодировок) в блоке UD.
  uint16_t PDU_UDH_IED1 = lngID;                                                        //  Данные следуют за байтом IEDL (размер зависит от значения PDU_IEI). Для составных SMS значение IED1 определяет идентификатор длинного сообщения (все SMS в составе длинного сообщения должны иметь одинаковый идентификатор).
  uint8_t  PDU_UDH_IED2 = lngSum;                                                       //  Предпоследний байт блока UDH. Для составных SMS его значение определяет количество SMS в составе длинного сообщения.
  uint8_t  PDU_UDH_IED3 = lngNum;                                                       //  Последний байт блока UDH. Для составных SMS его значение определяет номер данной SMS в составе длинного сообщения.
  uint16_t PDU_TPDU_LEN = 0x00;                                                         //  
                                                                                        //
  SERIAL_PRINT(F("SMS_send: "));                                                        //
  SERIAL_PRINTLN(txt);                                                                  //  
//  Блок TPDU включает все блоки PDU кроме блока SCA.                                   //
//      Определяем формат кодировки SMS сообщения:                                      //  
  for(uint8_t i=0; i<strlen(txt); i++){ if(uint8_t(txt[i])>=0x80){PDU_DCS=0x08;} }      //  Если в одном из байтов отправляемого текста установлен 7 бит, значит сообщение требуется закодировать в формате UCS2
//      Определяем класс SMS сообщения;                                                 //  
  if(clsSMSsend==GSM_SMS_CLASS_0){PDU_DCS|=0x10;}else                                   //  SMS сообщение 0 класса
  if(clsSMSsend==GSM_SMS_CLASS_1){PDU_DCS|=0x11;}else                                   //  SMS сообщение 1 класса
  if(clsSMSsend==GSM_SMS_CLASS_2){PDU_DCS|=0x12;}else                                   //  SMS сообщение 2 класса
  if(clsSMSsend==GSM_SMS_CLASS_3){PDU_DCS|=0x13;}                                       //  SMS сообщение 3 класса
//      Проверяем формат номера (адреса получателя):                                    //  
//      if(num[0]=='+'){if(num[1]!='7'){return false;}}                                 //  Если первые символы не '+7' значит номер указан не в международном формате.
//      else           {if(num[0]!='7'){return false;}}                                 //  Если первый символ  не  '7' значит номер указан не в международном формате.
//      Проверяем значения составного сообщения:                                        //  
  if(lngSum==0)    {return false;}                                                      //  Количество SMS в составном сообщении должно находиться в диапазоне от 1 до 255.
  if(lngNum==0)    {return false;}                                                      //  Номер      SMS в составном сообщении должен находиться в диапазоне от 1 до 255.
  if(lngNum>lngSum){return false;}                                                      //  Номер SMS не должен превышать количество SMS в составном сообщении.
//      Проверяем длину текста сообщения:                                               //  
  if(PDU_DCS%16==0){ if(lngSum==1){ if(txtLen>160){txtLen=160;} }else{ if(txtLen>152){txtLen=152;} } }    //  В формате GSM  текст сообщения не должен превышать 160 символов для короткого сообщения или 152 символа  для составного сообщения.
  if(PDU_DCS%16==8){ if(lngSum==1){ if(txtLen>70 ){txtLen= 70;} }else{ if(txtLen> 66){txtLen= 66;} } }    //  В формате UCS2 текст сообщения не должен превышать  70 символов для короткого сообщения или 66  символов для составного сообщения.
//      Определяем размер блока UD: (блок UD может включать блок UDH - заголовок, который так же учитывается) //  Количество байт отводимое для кодированного текста и заголовка (если он присутствует).
  if(PDU_DCS%16==0){PDU_UD_LEN=txtLen;  if(lngSum>1){PDU_UD_LEN+=8;}}                   //  Получаем размер блока UD в символах, при кодировке текста сообщения в формате GSM  и добавляем размер заголовка (7байт = 8символов) если он есть.
  if(PDU_DCS%16==8){PDU_UD_LEN=txtLen*2;  if(lngSum>1){PDU_UD_LEN+=7;}}                 //  Получаем размер блока UD в байтах,   при кодировке текста сообщения в формате UCS2 и добавляем размер заголовка (7байт) если он есть.
//      Определяем размер блока TPDU:                                                       //
  if(PDU_DCS%16==0){PDU_TPDU_LEN = 0x0D + PDU_UD_LEN*7/8; if(PDU_UD_LEN*7%8){PDU_TPDU_LEN++;} } //  Размер блока TPDU = 13 байт занимаемые блоками (PDUT, MR, DAL, DAT, DAD, PID, DCS, UDL) + размер блока UD (рассчитывается из количества символов указанных в блоке UDL).
  if(PDU_DCS%16==8){PDU_TPDU_LEN = 0x0D + PDU_UD_LEN;}                                  //  Размер блока TPDU = 13 байт занимаемые блоками (PDUT, MR, DAL, DAT, DAD, PID, DCS, UDL) + размер блока UD (указан в блоке UDL).
//      Отправляем SMS сообщение:                                                       //
  SERIAL_PRINT(F("AT+CMGS=")); SERIAL_PRINTLN(PDU_TPDU_LEN);                            //    
  DELAY(1000);                                                                          //
//  Выполняем команду отправки SMS без сохранения в область памяти, отводя на ответ до 1 сек.
//      Создаём блок PDU:                                                               //  
//      |                                                     PDU                                                     |           //  
//      +------------------+------------------------------------------------------------------------------------------+           //  
//      |                  |                                           TPDU                                           |           //  
//      |                  +----------------------------------------------+-------------------------------------------+           //  
//      |        SCA       |                                              |                    UD                     |           //  
//      |                  |           +---------------+                  |     +--------------------------------+    |           //  
//      |                  |           |      DA       |                  |     |              UDH               |    |           //  
//      +------------------+------+----+---------------+-----+-----+------+-----+--------------------------------+----+           //  
//      | SCAL [SCAT SCAD] | PDUT | MR | DAL [DAT DAD] | PID | DCS | [VP] | UDL | [UDHL IEI IEDL IED1 IED2 IED3] | UD |           //  
//                                                                                          //  
  SERIAL_PRINT(F("00"));                                                                // SCAL (Service Center Address Length)   //  Байт указывающий размер адреса сервисного центра.   Указываем значение 0x00. Значит блоков SCAT и SCAD не будет. В этом случае модем возьмет адрес сервисного центра не из блока SCA, а с SIM-карты.
  SERIAL_PRINT(_char(PDU_TYPE/16));                                                     //
  SERIAL_PRINT(_char(PDU_TYPE%16));                                                     // PDUT (Packet Data Unit Type)           //  Байт состоящий из флагов определяющих тип блока PDU.  Указываем значение 0x01 или 0x41. RP=0 UDHI=0/1 SRR=0 VPF=00 RD=0 MTI=01. RP=0 - обратный адрес не указан, UDHI=0/1 - наличие блока заголовка, SRR=0 - не запрашивать отчёт о доставке, VPF=00 - нет блока срока жизни сообщения, RD=0 - не игнорировать копии данного сообщения, MTI=01 - данное сообщение является исходящим.
  SERIAL_PRINT(F("00"));                                                                // MR (Message Reference)                 //  Байт                          Указываем значение 0x00. 
  SERIAL_PRINT(_char(PDU_DA_LEN/16));                                                   //
  SERIAL_PRINT(_char(PDU_DA_LEN%16));                                                   // DAL  (Destination Address Length)      //  Байт указывающий размер адреса получателя.  Указываем значение количество цифр в номере получател (без знака + и т.д.). +7(123)456-78-90 => 11 цифр = 0x0B.
  SERIAL_PRINT(F("91"));                                                                // DAT  (Destination Address Type)        //  Байт хранящий тип адреса получателя.        Указываем значение 0x91. Значит адрес получателя указан в международном формате: +7******... .
  _SMScoderAddr(num);                                                                   // DAD  (Destination Address Date)        //  Блок с данными адреса получателя.           Указываем номер num, кодируя его в конец строки.
  SERIAL_PRINT(F("00"));                                                                // PID  (Protocol Identifier)             //  Байт с идентификатором протокола передачи данных.   Указываем значение 0x00. Это значение по умолчанию.
  SERIAL_PRINT(_char(PDU_DCS/16));                                                      //
  SERIAL_PRINT(_char(PDU_DCS%16));                                                      // DCS  (Data Coding Scheme)              //  Байт указывающий схему кодирования данных.        Будем использовать значения 00-GSM, 08-UCS2 и 10-GSM, 18-UCS2. Во втором случае сообщение отобразится на дисплее, но не сохраняется на телефоне получателя.
  SERIAL_PRINT(_char(PDU_UD_LEN/16));                                                   //
  SERIAL_PRINT(_char(PDU_UD_LEN%16));                                                   // UDL  (User Data Length)                //  Байт указывающий размер данных (размер блока UD).   Для 7-битной кодировки - количество символов, для остальных - количество байт.
  if(PDU_UDH_IED2>1){                                                                   // UDH  (User Data Header)                //  Если отправляемое сообщение является стоставным,    то добавляем заголовок (блок UDH)...
    SERIAL_PRINT(F("06"));                                                              // UDHL (User Data Header Length)         //  Байт указывающий количество оставшихся байт блока UDH.  Указываем значение 0x06. Это значит что далее следуют 6 байт: 1 байт IEI, 1 байт IEDL, 2 байта IED1, 1 байт IED2 и 1 байт IED3.
    SERIAL_PRINT(F("08"));                                                              // IEI  (Information Element Identifier)  //  Байт указывающий на назначение заголовка.       Указываем значение 0x08. Это значит что данное сообщение является составным с 2 байтным блоком IED1 (если указать значение 0x0, то блок IED1 должен занимать 1 байт).
    SERIAL_PRINT(F("04"));                                                              // IEDL (Information Element Data Length) //  Байт указывающий количество оставшихся байт блока IED.  Указываем значение 0x04. Это значит что далее следуют 4 байта: 2 байта IED1, 1 байт IED2 и 1 байт IED3.
    SERIAL_PRINT(_char(PDU_UDH_IED1/4096));                                             //
    SERIAL_PRINT(_char(PDU_UDH_IED1%4096/256));                                         // IED1 (Information Element Data 1)      //  Блок указывающий идентификатор составного сообщения.  Все сообщения в составе составного должны иметь одинаковый идентификатор.
    SERIAL_PRINT(_char(PDU_UDH_IED1%256/16));                                           //
    SERIAL_PRINT(_char(PDU_UDH_IED1%16));                                               //    (2 байта)                           //
    SERIAL_PRINT(_char(PDU_UDH_IED2/16));                                               //
    SERIAL_PRINT(_char(PDU_UDH_IED2%16));                                               // IED2 (Information Element Data 1)      //  Байт указывающий количество SMS в составе составного сообщения.
    SERIAL_PRINT(_char(PDU_UDH_IED3/16));                                               //
    SERIAL_PRINT(_char(PDU_UDH_IED3%16));                                               // IED3 (Information Element Data 1)      //  Байт указывающий номер данной SMS в составе составного сообщения.
  }                                                                                     //
                                                                                        // UD (User Data)                         //  Блок данных пользователя (текст сообщения).       Указывается в 16-ричном представлении.
                                                                                        //  if(!GET_MODEM_ANSWER(">",1000))       //  Выполняем команду отправки SMS без сохранения в область памяти, отводя на ответ до 1 сек.
//      Проверяем готовность модуля к приёму блока PDU для отправки SMS сообщения:      //
  DELAY(500);                                                                           //  Выполняем отправку текста, выделяя на неё до 500 мс.
//      Кодируем и передаём текст сообщения по 24 символа:                              //  Передача текста частями снижает вероятность выхода за пределы "кучи" при передаче длинных сообщений.
  uint16_t txtPartSMS=0;                                                                //  Количество отправляемых символов за очередной проход цикла.
  uint16_t txtPosNext=0;                                                                //  Позиция после последнего отправленного символа в строке txt.
                                                                                        //
  while (txtLen) {                                                                      //
    txtPartSMS=txtLen>24?24:txtLen; txtLen-=txtPartSMS;                                 //
    if(PDU_DCS%16==0){txtPosNext=_SMScoderGSM ( txt, txtPosNext, txtPartSMS);}          //  Записать в конец строки, закодированный в формат GSM  текст сообщения, текст брать из строки txt начиная с позиции txtPosNext, всего взять txtPartSMS символов (именно символов, а не байтов).
    if(PDU_DCS%16==8){txtPosNext=_SMScoderUCS2( txt, txtPosNext, txtPartSMS);}          //  Записать в конец строки, закодированный в формат UCS2 текст сообщения, текст брать из строки txt начиная с позиции txtPosNext, всего взять txtPartSMS символов (именно символов, а не байтов).        
    DELAY(500);                                                                         //
  }                                                                                     //
  SERIAL_PRINT("\32");                                                                  //
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ АВТООПРЕДЕЛЕНИЯ КОДИРОВКИ СКЕТЧА:                                         //  
void  MODEM::TXTsendCodingDetect(const char* str){                                      //  Аргументы функции:  строка состоящая из символа 'п' и символа конца строки.
  if(strlen(str)==2)      {codTXTsend=GSM_TXT_UTF8;   }                                 //  Если символ 'п' занимает 2 байта, значит текст скетча в кодировке UTF8.
  else  if(uint8_t(str[0])==0xAF) {codTXTsend=GSM_TXT_CP866;    }                       //  Если символ 'п' имеет код 175, значит текст скетча в кодировке CP866.
  else  if(uint8_t(str[0])==0xEF) {codTXTsend=GSM_TXT_WIN1251;  }                       //  Если символ 'п' имеет код 239, значит текст скетча в кодировке WIN1251.
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ПРОВЕРКИ НАЛИЧИЯ ПРИНЯТЫХ SMS СООБЩЕНИЙ:                                  //  Функция возвращает: целочисленное значение uint8_t соответствующее количеству принятых SMS.
uint8_t MODEM::SMSavailable(void){                                                      //  Аргументы функции:  отсутствуют.
  if(_SMSsum()==0){return 0;}                                                           //  Если в выбранной памяти нет SMS (входящих и исходящих) то возвращаем 0.
  SERIAL_PRINTLN(F("AT+CMGD=1,3"));                                                     //
  GET_MODEM_ANSWER(OK,10000);                                                           //  Выполняем команду удаления SMS сообщений из памяти, выделяя на это до 10 секунд, второй параметр команды = 3, значит удаляются все прочитанные, отправленные и неотправленные SMS сообщения.
  return _SMSsum();                                                                     //  Так как все прочитанные, отправленные и неотправленные SMS сообщения удалены, то функция _SMSsum вернёт только количество входящих непрочитанных SMS сообщений.
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ПОЛУЧЕНИЯ КОЛИЧЕСТВА ВСЕХ SMS В ПАМЯТИ:                                   //  Функция возвращает: число uint8_t соответствующее общему количеству SMS хранимых в выбранной области памяти.
uint8_t MODEM::_SMSsum(void){                                                           //  Аргументы функции:  отсутствуют.
  SERIAL_PRINTLN(F("AT+CPMS?"));                                                        //
  //  Выполняем команду получения выбранных областей памяти для хранения SMS сообщений. //
  GET_MODEM_ANSWER("+CPMS:",10000);                                                     //
  return SMSsum;                                                                        //
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ПОЛУЧЕНИЯ ОБЪЕМА ПАМЯТИ SMS СООБЩЕНИЙ:                                    //  Функция возвращает: целочисленное значение uint8_t соответствующее максимальному количеству принятых SMS.
uint8_t MODEM::SMSmax(void){                                                            //  Аргументы функции:  отсутствуют.                                                                 //  Объявляем временные переменные.
  SERIAL_PRINTLN(F("AT+CPMS?"));                                                        //  Выполняем команду получения выбранных областей памяти для хранения SMS сообщений.
  GET_MODEM_ANSWER("+CPMS:",10000);                                                     //
  return maxSMS;                                                                        //
}                                                                                       //    
