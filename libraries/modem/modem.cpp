#include "modem.h"
#include <avr/sleep.h>
#include <avr/power.h>

// DTMF команды.
enum {
    GUARD_ON = 1, // 1# - постановка на охрану
    GUARD_OFF,    // 2# - снятие с охраны 
    EMAIL_ON_OFF, // 3# - включить/отключить GPRS/EMAIL
    SMS_ON_OFF,   // 4# - включить/отключить SMS
    TEL_ON_OFF,   // 5# - включить/отключить звонок при тревоге
    GET_INFO,     // 6# - сбор и отправка всех данных датчиков
    EMAIL_ADMIN_PHONE,// 7# - отправляем на почту номер админа
    EMAIL_PHONE_BOOK, // 8# - отправка на почту телефонной книги
    ADMIN_NUMBER_DEL, // 9# - админ больше не админ
    SM_CLEAR,     // 10# - удалить все номера с симкарты
    MODEM_RESET,  // 11# - перезагрузка модема
    ESP_RESET,    // 12# - перезагрузка модема
    BAT_CHARGE,   // 13# - показывает заряд батареи в виде строки
                  //  +CBC: 0,100,4200
                  // где 100 - процент заряда
                  // 4200 - напряжение на батарее в мВ.
    CONNECT_ON_OFF, // 14# - Инвертирует флаг CONNECT_ALWAYS
    SENS_GET_NAMES, // 15# - Возвращает имена датчиков
    SYNC_TIME       // 16# - синхронизация времени модема и часов
  };

FLAGS
bool flag_gprs_connect = false; // показывает, есть ли соединение

# include "i2c.h"

extern MY_SENS *sensors;

enum answer {get_ip, ip_ok, gprs_connect, email_end, smtpsend, smtpsend_end, admin_phone};

#define CLEAR_FLAG_ANSWER           answer_flags=0
#define GET_FLAG_ANSWER(flag)       bitRead(answer_flags,flag)
#define SET_FLAG_ANSWER_ONE(flag)   bitSet(answer_flags,flag)
#define SET_FLAG_ANSWER_ZERO(flag)  bitClear(answer_flags,flag)
#define INVERT_FLAG_ANSWER(flag)    INV_FLAG(answer_flags,1<<flag)

#define OK   "OK"
#define ERR  "ERROR"

#define GET_TIME F("AT+CCLK?")

// определить количество занятых записей и их общее число
#define SIM_RECORDS_INFO  F("AT+CPBS?")

#define MODEM_GET_STATUS  F("AT+CPAS")
  #define NOT_READY       "CPAS: 1"

#define RING_BREAK        SERIAL_PRINTLN(F("ATH")) // разорвать все соединения

// Выставляем тайминги (мс)
// время между опросами модема на предмет зависания и не отправленных смс/email
#define REGULAR_OPROS_TIME  10000
uint32_t opros_time;

#define GPRS_GET_IP       if(!answer_flags && flag_gprs_connect){SERIAL_PRINTLN(F("AT+SAPBR=2,1"));SET_FLAG_ANSWER_ONE(get_ip);gprs_init_count++;}
#define GPRS_CONNECT(op)  if(!answer_flags && !flag_gprs_connect){SERIAL_PRINT(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\";+SAPBR=3,1,\"APN\",\"internet\";+SAPBR=3,1,\"USER\",\"")); \
                          SERIAL_PRINT(PGM_TO_CHAR(op.user));SERIAL_PRINT(F("\";+SAPBR=3,1,\"PWD\",\""));SERIAL_PRINT(PGM_TO_CHAR(op.user));\
                          SERIAL_PRINTLN(F("\";+SAPBR=1,1"));SET_FLAG_ANSWER_ONE(gprs_connect);flag_gprs_connect=true;}
#define GPRS_DISCONNECT   if(flag_gprs_connect){SERIAL_PRINTLN(F("AT+SAPBR=0,1"));answer_flags=0;flag_gprs_connect=false;}

// отправка смс AT+CMGS=\"+79xxxxxxxxx\"
#define SEND_SMS  if(GET_FLAG(SMS_ENABLE) && admin.phone[0]){SERIAL_PRINT(F("AT+CMGS=\""));SERIAL_PRINT(admin.phone);SERIAL_PRINTLN('\"');}

// ****************************************
#define ADMIN_PHONE_SET_ZERO  memset(&admin,0,sizeof(ABONENT_CELL))

#define PGM_TO_CHAR(str) strcpy_P(text->GetText()+text->filling()+1,(char*)pgm_read_word(&(str))) 
// Опрашивает модем
#define READ_COM_FIND_RAM(str) strstr(text->GetText(),str)
#define READ_COM_FIND(str)     strstr_P(text->GetText(),PSTR(str))
#define READ_COM_FIND_P(str)   strstr_P(text->GetText(),(char*)pgm_read_word(&(str)))

#define POWER_ON  digitalWrite(BOOT_PIN,LOW);DELAY(1000);digitalWrite(BOOT_PIN,HIGH)

#define GET_MODEM_ANSWER(answer,wait_ms) get_modem_answer(PSTR(answer),wait_ms)

enum {ADMIN};
const char cell_name_0[] PROGMEM = "\"ADMIN\"";
//const char cell_name_0[] PROGMEM = "\"Admin\"";
const char* const abonent_name[] PROGMEM = {cell_name_0};

//Эти сообщения пересылаются на e-mail
const char str_0[] PROGMEM = "+CUSD:";   
const char str_1[] PROGMEM = "+CBC:";
const char str_2[] PROGMEM = "ALARM";
const char str_3[] PROGMEM = "+CCLK:";
const char* const string[] PROGMEM = {str_0, str_1, str_2, str_3};

uint8_t op_count; // число операторов 

typedef struct PROGMEM
{
  const char* op;
  const char* user;
} OPERATORS; 

#define ADD_OP(index,name,user) const char op_##index[]    PROGMEM=name; \
                                const char user_##index[]  PROGMEM=user

ADD_OP(0, "MTS",     "mts");
ADD_OP(1, "MEGAFON", "gdata");
ADD_OP(2, "MegaFon", "gdata");
ADD_OP(3, "Tele2",   "");
ADD_OP(4, "MOTIV",   "");
ADD_OP(5, "Beeline", "beeline");
ADD_OP(6, "Bee Line", "beeline");

const OPERATORS op_base[] PROGMEM = {
  {op_0, user_0},
  {op_1, user_1},
  {op_2, user_2},
  {op_3, user_3},
  {op_4, user_4},
  {op_5, user_5},
  {op_6, user_6}
};

// Условия для рестарта модема
#define ADD_RESET(index,name) const char reset_##index[] PROGMEM=name

ADD_RESET(0,NOT_READY);
ADD_RESET(1,"SIM800");
ADD_RESET(2,"NOT READY");
ADD_RESET(3,"SIM not");
ADD_RESET(4," 99");
ADD_RESET(5,"+CMS ERROR");

const char* const reset[] PROGMEM = {reset_0, reset_1, reset_2, reset_3, reset_4, reset_5};

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
  #define BEEP        beep()

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
  SET_FLAG_ONE(INTERRUPT);
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
  gsm_operator = op_count;
  // зуммер
  BEEP_INIT;
  // Устанавливаем скорость связи Ардуино и модема
  SERIAL_BEGIN;
  I2C_INIT;
}

MODEM::~MODEM()
{
  delete text;
  delete email_buffer;
}

void MODEM::init()
{
  dtmf_index   = 0;
  DTMF[0]      = MODEM_RESET;
  DTMF[1]      = 0;
  text         = new TEXT(160);
  email_buffer = new TEXT(255);

  phone_num    = 0;
  cell_num     = 0;
  ADMIN_PHONE_SET_ZERO;
  SET_FLAG_ONE(RING_ENABLE);
  OTCHET_INIT
}

void MODEM::parser()
{
  char *p, *pp;
  char ch;

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

  // +CPBS: "SM",18,100
  if((p = READ_COM_FIND("CPBS:"))!=NULL)
  {
    p+=11;
    pp=p;
    while(*p!=',' && *p) p++;
    *p=0;
    phone_num = atoi(pp);
    cell_num = atoi(p+1);

    return;    
  }

  // +CPBF: 20,"+79xxxxxxxxx",145,"ADMIN"
  // +CPBR: 1,"+380xxxxxxxxx",129,"Ivanov"
  // +CPBR: 20,"+79xxxxxxxxx",145,"USER"
  // Запоминаем индекс ячейки телефонной книги
  if((p=READ_COM_FIND("CPBR: "))!=NULL || (p=READ_COM_FIND("CPBF: "))!=NULL)
  {
    p+=6; pp = p;
    if(GET_FLAG(EMAIL_ENABLE))
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
    return;
  }

  // RING
  //+CLIP: "+79xxxxxxxxx",145,"",0,"USER",0
  //+CLIP: "+79xxxxxxxxx",145,"",0,"",0
  if((p=READ_COM_FIND("CLIP:"))!=NULL)
  {
    if(GET_FLAG(EMAIL_ENABLE)) email_buffer->AddText(p);
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

    return;
  }

  if(gsm_operator == op_count)
  {
    gsm_operator = 0;
    while (gsm_operator < op_count && READ_COM_FIND_P(op_base[gsm_operator].op)==NULL) gsm_operator++;
  }

  if (READ_COM_FIND("CMGS:")!=NULL) // +CMGS: <mr> - индекс отправленного сообщения 
  { // смс успешно отправлено
    // удаляем все отправленные сообщения
    SERIAL_PRINTLN(F("AT+CMGD=1,3"));
    email_buffer->Clear();
    return;
  }
  
  if ((p=READ_COM_FIND("CMTI:"))!=NULL) // получили номер смс в строке вида +CMTI: "SM",0
  {       
    p+=11;
    // получили номер смс в строке вида +CMTI: "SM",0
    // чтение смс из памяти модуля
    SERIAL_PRINT(F("AT+CMGR=")); SERIAL_PRINTLN(p);
    return;
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
    return;    
  }

  if (admin.phone[0])
  {
    if (READ_COM_FIND_RAM(admin.phone))
    {
      // звонок или смс от Админа
      SET_FLAG_ANSWER_ONE(admin_phone);
      return;
    }      
  }

  if(READ_COM_FIND("DIALTON")!=NULL)
  {
    RING_TO_ADMIN(admin.index, admin.phone[0]);
    return;    
  }

#if TM1637_ENABLE
  // получаем время  +CCLK: "04/01/01,03:36:04+12"
  if ((p=READ_COM_FIND("+CCLK:"))!=NULL)
  {
    char ch;
    uint8_t time[3];
    p+=17;

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

  for(uint8_t i = 0; i < 6; i++)
  {
    if(READ_COM_FIND_P(reset[i])!=NULL)
    {
      DTMF[0]=MODEM_RESET;
      return;
    }
  }

  for(uint8_t i = 0; i < 3; i++)
  {
    if(READ_COM_FIND_P(ath[i])!=NULL)
    {
      RING_BREAK;
      dtmf_index = 0;
      return;
    }
  }

  for(uint8_t i = 0; i < 4; i++)
  {
    if((p=READ_COM_FIND_P(string[i]))!=NULL)
    {
      email_buffer->AddText(p);
      return;
    }
  }

  ////////////////////////////////////////////////////////////
  /// Для GPRS
  ////////////////////////////////////////////////////////////
  
  // требуется загрузка текста письма для отправки
  if(READ_COM_FIND("DOWNLOAD")!=NULL || READ_COM_FIND(">")!=NULL)
  {
    SERIAL_PRINT(email_buffer->GetText());
    SERIAL_FLUSH;
    SERIAL_WRITE(26);
    if(GET_FLAG(EMAIL_ENABLE))
    { // Ждём OK от модема,
      // после чего даём команду на отправку письма
      SET_FLAG_ANSWER_ONE(smtpsend);
    }
    return;        
  }

  if((p = READ_COM_FIND("+SMTPSEND:"))!=NULL)
  {
    SET_FLAG_ANSWER_ZERO(smtpsend_end); 
    // письмо успешно отправлено
    if((p = READ_COM_FIND(": 1"))!=NULL)
    {
      if(!GET_FLAG(CONNECT_ALWAYS))
      { // разрываем соединение с интернетом
        GPRS_DISCONNECT;
      }
      gprs_init_count = 0;
      email_buffer->Clear();
    }

    return;    
  }

  if((p = READ_COM_FIND("+SAPBR"))!=NULL)
  {
    if((p = READ_COM_FIND(": 1,1"))!=NULL)
    {
      SET_FLAG_ANSWER_ZERO(get_ip);
      SET_FLAG_ANSWER_ONE(ip_ok);
    }
    else flag_gprs_connect=false;

    return;
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
    
    return;      
  }

//////////////////////////////////////////////////////////// 

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
      dtmf[dtmf_index++] = *p;
      dtmf[dtmf_index] = 0;
      if(dtmf_index > DTMF_BUF_SIZE-2)
      {
        for(uint8_t i = 0; i < dtmf_index; i++)
        {
          dtmf[i] = dtmf[i+1];  
        }      
        dtmf_index--;
      }      
    }
   
    return;    
  }
  
  // Получаем смс вида "set pin НОМЕР_ПИНА on/off".
  // Например, "set pin 14 on" и "set pin 14 off"
  // задают пину 14 (А0) значения HIGH или LOW соответственно.
  // Поддерживаются пины от 5 до 19, где 14 соответствует А0, 15 соответствует А1 и т.д
  // Внимание. Пины A6 и A7 не поддерживаются, т.к. они не работают как цифровые выходы.
  if ((p=READ_COM_FIND("set pin"))!=NULL)
  {
    if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=8;
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
      digitalWrite(pin, level); // устанавливаем пин в 0, для выключения реле
      // ответное смс
      email_buffer->AddText_P(PSTR("PIN "));
      email_buffer->AddInt(pin);
      email_buffer->AddText_P(PSTR(" is "));
      email_buffer->AddInt(digitalRead(pin));
      DEBUG_PRINTLN(email_buffer->GetText()); // отладочное собщение

      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return;      
  }   
}

void MODEM::reinit()
{
  uint8_t cnt = 0;

  DEBUG_PRINTLN(F("BOOT"));

  POWER_ON; // Включение GSM модуля

  if(GET_MODEM_ANSWER("RDY", 5000))
  {
    sim800_enable = true;
    SET_FLAG_ONE(MODEM_NEED_INIT);
  }

  reset_count = 0;
  gprs_init_count = 0;
  flag_gprs_connect=false;
  CLEAR_FLAG_ANSWER;
  opros_time = REGULAR_OPROS_TIME;
  timeRegularOpros = millis();
}

void MODEM::reinit_end()
{
  if(GET_FLAG(MODEM_NEED_INIT))
  {
    const __FlashStringHelper* cmd[] = {
      F("AT+DDET=1"),         // вкл. DTMF. 
      F("ATS0=0"),            // устанавливает количество гудков до автоответа
      F("AT+CLTS=1;&W"),         // синхронизация времени по сети
      F("ATE0"),              // выключаем эхо
      F("AT+CLIP=1"),         // Включаем АОН
      F("AT+CMGF=1"),         // Формат СМС = ASCII текст
      F("AT+IFC=1,1"),        // устанавливает программный контроль потоком передачи данных
      F("AT+CSCS=\"GSM\""),   // Режим кодировки текста = GSM (только англ.)
      F("AT+CNMI=2,2,0,0,0"), // Текст смс выводится в com-порт
      F("AT+CSCB=1"),         // Отключаем рассылку широковещательных сообщений        
      //F("AT+CNMI=1,1")      // команда включает отображение номера пришедшей СМСки +CMTI: «MT», <номер смски>
      F("AT+CNMI=2,2"),       // выводит сразу в сериал порт, не сохраняя в симкарте
      F("AT+CPBS=\"SM\""),    // вкл. доступ к sim карте
      F("AT+CMGD=1,4"),       // удалить все смс
      SIM_RECORDS_INFO        // определить количество занятых записей и их общее число
    };

    uint8_t size = sizeof(cmd)/sizeof(cmd[0]);

    for(uint8_t i = 0; i < size; i++)
    {
      SERIAL_PRINTLN(cmd[i]);
      if(!GET_MODEM_ANSWER(OK, 10000)) return;
    }   

    DTMF[0] = SYNC_TIME;

    timeRegularOpros = millis();        

    DEBUG_PRINTLN(F("init end"));

    SET_FLAG_ZERO(MODEM_NEED_INIT);
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
                
    if(GET_FLAG(EMAIL_ENABLE)) email_buffer->AddText_P(PSTR("WakeUp:"));

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


bool MODEM::get_modem_answer(const char* answer, uint16_t wait_ms)
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
        reset_count = 0;
        timeRegularOpros = millis();
        if(answer!=NULL)
        {
          if(strstr_P(text->GetText(),answer)!=NULL)
          {
            text->Clear();
            return true;
          }
        }
        parser();
        text->Clear();    
      }
    }
  }

  return false;
}

#define DELETE_PHONE(index)    {SERIAL_PRINT(F("AT+CPBW="));SERIAL_PRINTLN(index);}
#define FLAG_STATUS(flag,name) email_buffer->AddText_P(PSTR(name));email_buffer->AddInt(GET_FLAG(flag));email_buffer->AddChar(' ')

void MODEM::flags_info()
{
  FLAG_STATUS(GUARD_ENABLE,   "GUARD=");
  FLAG_STATUS(EMAIL_ENABLE,   "EMAIL=");
  
  if (sim800_enable)
  {
    FLAG_STATUS(RING_ENABLE,    "TEL=");
    FLAG_STATUS(CONNECT_ALWAYS, "CONNECT=");
    FLAG_STATUS(SMS_ENABLE,     "SMS=");
  }
  
  email_buffer->AddChar('\n');
}

void MODEM::wiring() // прослушиваем телефон
{
  uint8_t tmp;

  read_com(NULL);

  if (sim800_enable)
  {
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

      if(!GET_FLAG(MODEM_NEED_INIT) && !admin.phone[0]) DTMF[0]=EMAIL_ADMIN_PHONE;

      reinit_end();

      reset_count++;

      if(!GET_FLAG(MODEM_NEED_INIT))
      {
        // Есть данные для отправки
        if(email_buffer->filling() && esp8266_enable==false)
        {      
          if(GET_FLAG(EMAIL_ENABLE))
          {
            if(gsm_operator == op_count)
            {
              SERIAL_PRINTLN(F("AT+COPS?"));   
            }
            else
            {
              GPRS_CONNECT(op_base[gsm_operator])
              else GPRS_GET_IP
            }                 
          }
          else SEND_SMS
          else email_buffer->Clear();                  
        }
        else
        {
          sleep();
          // проверяем непрочитанные смс и уровень сигнала
          SERIAL_PRINTLN(F("AT+CMGL=\"REC UNREAD\",1;+CSQ"));
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
  }

  if(DTMF[0])
  {
    DEBUG_PRINT(F("DTMF=")); DEBUG_PRINTLN(DTMF[0]);
    BEEP;
    if(DTMF[1])
    {
      SERIAL_PRINT(F("AT+CUSD=1,\"#"));
      SERIAL_PRINT(DTMF[1]); SERIAL_PRINT('*');
      SERIAL_PRINT(DTMF[0]); SERIAL_PRINTLN(F("#\""));
    }
    else    
    switch (DTMF[0])
    {
      case GUARD_ON:
        SET_FLAG_ONE(GUARD_ENABLE);
        flags_info();
        break;                        
      case GUARD_OFF:
        SET_FLAG_ZERO(GUARD_ENABLE);
        flags_info();
        break;
      case GET_INFO:
        if(digitalRead(POWER_PIN))
        {
          if(GET_FLAG(GUARD_ENABLE)) sensors->TimeReset();
          email_buffer->AddText_P(PSTR(" Svet ON."));
        }           
        else
          email_buffer->AddText_P(PSTR(" Svet OFF."));
        flags_info(); 
        if(GET_FLAG(GUARD_ENABLE)) sensors->GetInfoAll(email_buffer);                       
        break;
      case EMAIL_ON_OFF:
        INVERT_FLAG(EMAIL_ENABLE);
        flags_info();
        break;
      case SENS_GET_NAMES:
        if(GET_FLAG(GUARD_ENABLE))
        {
          email_buffer->AddText_P(PSTR(I2C_NAME));            
          for(uint8_t k = 0; k < sensors->size; k++)
          {
            email_buffer->AddChar(' ');
            email_buffer->AddText(sensors->GetNameByIndex(k));              
          }
          email_buffer->AddChar('\n');
        }
        break;
        case MODEM_RESET:
          reinit();
        break;
        case ESP_RESET:
          I2C_RESET;
        break;
      case TEL_ON_OFF:
        INVERT_FLAG(RING_ENABLE);
        flags_info();     
        break;      
      case SMS_ON_OFF:
        INVERT_FLAG(SMS_ENABLE);
        flags_info();             
        break;
      case CONNECT_ON_OFF:
        INVERT_FLAG(CONNECT_ALWAYS);
        flags_info();             
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
        SERIAL_PRINTLN(GET_TIME);
        break;
      default:
        SERIAL_PRINT(F("AT+CUSD=1,\"#"));
        SERIAL_PRINT(DTMF[0]); SERIAL_PRINTLN(F("#\""));
    }
    DTMF[0] = 0;
    DTMF[1] = 0;
    
    dtmf_index = 0;
  }
}
