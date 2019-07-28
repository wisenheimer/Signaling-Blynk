#ifndef MODEM_H
#define MODEM_H

#include "text.h"
#include "my_sensors.h"

#if TM1637_ENABLE
#   include "clock.h"
#endif

#define RING_TO_ADMIN(index,flag) if(GET_FLAG(RING_ENABLE) && flag){SERIAL_PRINT(F("ATD>"));SERIAL_PRINT(index);SERIAL_PRINTLN(';');}

#define NLENGTH 14 // Max. length of phone number 
#define TLENGTH 21 // Max. length of text for number
#define DTMF_BUF_SIZE 6

#define GSM_TXT_CP866    0   //  Название кодировки в которой написан текст. (паремр функций TXTsendCoding() и TXTreadCoding() указывающий кодировку CP866)
#define GSM_TXT_UTF8     1   //  Название кодировки в которой написан текст. (паремр функций TXTsendCoding() и TXTreadCoding() указывающий кодировку UTF8)
#define GSM_TXT_WIN1251  2   //  Название кодировки в которой написан текст. (паремр функций TXTsendCoding() и TXTreadCoding() указывающий кодировку WIN1251)
#define GSM_SMS_CLASS_0  0   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 0)
#define GSM_SMS_CLASS_1  1   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 1)
#define GSM_SMS_CLASS_2  2   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 2)
#define GSM_SMS_CLASS_3  3   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 3)
#define GSM_SMS_CLASS_NO 4   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS без класса)


typedef struct cell
{
    char    phone[NLENGTH];
    char    name[TLENGTH];
    uint8_t index; // индекс ячейки в телефонной книге  
} ABONENT_CELL;

typedef struct sms_info{
    uint16_t lngID; // идентификатор склеенных SMS
    uint8_t lngSum; // количество склеенных SMS
    uint8_t lngNum; // номер данной склеенной SMS
    char tim[18];   // дата, время
    char num[17];   // номер получателя
    char txt[141];   // передаваемый текст  
} SMS_INFO;

class MODEM
{
  public:
    MODEM::MODEM();
    ~MODEM();
    TEXT        *email_buffer;
    void        init    ();
    void        wiring  ();
    void        email   ();
    ABONENT_CELL admin; // телефон админа

  private:
    ABONENT_CELL last_abonent;
    uint8_t     phone_num;
    uint8_t     gsm_operator;
    uint8_t     cell_num;
    TEXT        *text;    
    uint8_t     answer_flags;
    uint8_t     flag_sensor_enable;
    uint8_t     reset_count;
    uint8_t     gprs_init_count;
    uint32_t    timeRegularOpros;
    uint16_t    DTMF[2];  
    uint8_t     dtmf_index; 
    uint8_t     dtmf_str_index; 
    char        dtmf[DTMF_BUF_SIZE]; // DTMF команда
    char        dtmf_str[9]; // DTMF строка

    uint8_t     SMSsum = 0;
    uint8_t     numSMS;                                                                                  //  Объявляем  переменную для хранения № прочитанной SMS (число)
    uint8_t     maxSMS;                                                                                  //  Объявляем  переменную для хранения объема памяти SMS (число)
    uint8_t     codTXTread =        GSM_TXT_UTF8;                                                        //  Тип кодировки строки StrIn.
    uint8_t     codTXTsend =        GSM_TXT_UTF8;                                                        //  Тип кодировки строки StrIn.
    uint8_t     clsSMSsend =        GSM_SMS_CLASS_NO;
    
    void        sleep               ();
    uint8_t     parser              ();    
    void        flags_info          ();
    void        reinit              ();
    void        reinit_end          ();
    uint8_t     get_sm_cell         (uint8_t index);
    void        email_send_abonent  (ABONENT_CELL abonent);
    char*       get_number_and_type (char* p);
    char*       get_name            (char* p);
    bool        read_com            (const char* answer);

    uint8_t     _num                (char);                                                             //  Преобразование символа в число(аргумент функции: символ 0-9,a-f,A-F)
    char        _char               (uint8_t);                                                          //  Преобразование числа в символ (аргумент функции: число 0-15)
    uint8_t     _SMSsum             (void);                                                             //  Получение кол SMS в памяти(без аргументов)
    uint8_t     SMSmax              (void);  
    uint16_t    _SMStxtLen          (const char*);                                                      //  Получение количества символов в строке (строка с текстом)
    uint16_t    _SMScoderGSM        (const char*, uint16_t, uint16_t=255);                              //  Кодирование текста в GSM (строка с текстом, позиция взятия из строки, количество кодируемых символов) Функция возвращает позицию после последнего закодированного символа из строки txt.
    void        _SMSdecodGSM        (      char*, uint16_t, uint16_t, uint16_t=0);                      //  Разкодирование текста GSM (строка для текста, количество символов в тексте, позиция начала текста в строке, количество байт занимаемое заголовком)
    void        _SMSdecod8BIT       (      char*, uint16_t, uint16_t);                                  //  Разкодирование текста 8BIT (строка для текста, количество байт в тексте, позиция начала текста в строке)
    uint16_t    _SMScoderUCS2       (const char*, uint16_t, uint16_t=255);                              //  Кодирование текста в UCS2 (строка с текстом, позиция взятия из строки, количество кодируемых символов) Функция возвращает позицию после последнего закодированного символа из строки txt.
    void        _SMSdecodUCS2       (      char*, uint16_t, uint16_t);                                  //  Разкодирование текста UCS2 (строка для текста, количество байт в тексте, позиция начала текста в строке)
    void        _SMScoderAddr       (const char*);                                                      //  Кодирования адреса SMS (строка с адресом)
    void        _SMSdecodAddr       (      char*, uint16_t, uint16_t);                                  //  Разкодирование адреса SMS (строка для адреса, количество полубайт в адресе, позиция адреса  в строке)
    void        _SMSdecodDate       (      char*,           uint16_t);                                  //  Разкодирование даты SMS (строка для даты, позиция даты в строке)
    bool        SMSsend             (const char*, const char*, uint16_t=0x1234, uint8_t=1, uint8_t=1);  //  Отправка SMS (аргумент функции: текст, номер, идентификатор составного SMS сообщения, количество SMS в составном сообщении, номер SMS в составном сообщении) 
    void        PDUread             (SMS_INFO*);
    void        TXTsendCodingDetect (const char* str);
    bool        runAT               (uint16_t wait_ms, const char* answer);
    uint8_t     SMSavailable        (void);   
};

#endif
