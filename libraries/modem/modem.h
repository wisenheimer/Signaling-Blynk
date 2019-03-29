#ifndef MODEM_H
#define MODEM_H

#include "text.h"
#include "my_sensors.h"

#if MODEM_ENABLE
#   define RING_TO_ADMIN(index) if(GET_FLAG(RING_ENABLE)){SERIAL_PRINT(F("ATD>"));SERIAL_PRINT(index);SERIAL_PRINTLN(';');}
#else
#   define RING_TO_ADMIN(index)
#endif

#define NLENGTH 14 // Max. length of phone number 
#define TLENGTH 21 // Max. length of text for number
#define DTMF_BUF_SIZE 6

typedef struct cell
{
    char    phone[NLENGTH];
    char    name[TLENGTH];
    uint8_t index; // индекс ячейки в телефонной книге  
} ABONENT_CELL;

class MODEM
{
  public:
    MODEM::MODEM();
    ~MODEM();
    TEXT *email_buffer;
    void init();
    void wiring();
#if MODEM_ENABLE
    void email();
    ABONENT_CELL admin; // телефон админа
#endif

  private:
#if MODEM_ENABLE
    ABONENT_CELL last_abonent;
    uint8_t phone_num;
    uint8_t gsm_operator;
    uint8_t cell_num;
#endif
    TEXT *text;    
    uint8_t answer_flags;
    uint8_t flag_sensor_enable;
    uint8_t dtmf_index;    
    uint8_t reset_count;
    uint8_t gprs_init_count;
    uint32_t timeRegularOpros;
    uint16_t DTMF[2];  
    char dtmf[DTMF_BUF_SIZE]; // DTMF команда
    
    void sleep();
    void parser();    
    void flags_info();
    void reinit();
    void reinit_end();
    bool get_modem_answer(const char* answer, uint16_t wait_ms);
    uint8_t get_sm_cell(uint8_t index);
    void email_send_abonent(ABONENT_CELL abonent);
    char* get_number_and_type(char* p);
    char* get_name(char* p);
    bool read_com(const char* answer);
};

#endif
