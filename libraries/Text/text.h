#ifndef myText_h
#define myText_h

#include "Arduino.h"
#include "fifo.h"

class TEXT
{
  public:
    TEXT(unsigned int);
    ~TEXT();

    uint16_t filling();
    uint16_t free_space();
    uint16_t AddText(char*);
    uint16_t AddText_P(const char*);
    bool AddChar(char);
    void AddInt(unsigned int);
    void AddFloat(float);
    void Clear();
    char* GetText();
    char GetChar();
    uint16_t GetBytes(char*, unsigned int, bool);
    
  private:
    fifo_t fifo;
};

#endif
