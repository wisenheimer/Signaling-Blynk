#ifndef myText_h
#define myText_h

#include "Arduino.h"
#include "fifo.h"

class TEXT
{
  public:
    TEXT(unsigned int size);
    ~TEXT();

    uint16_t filling();
    uint16_t free_space();
    uint16_t AddText(char* str);
    uint16_t AddText_P(const char* str);
    bool AddChar(char ch);
    void AddInt(unsigned int i);
    void AddFloat(float value);
    void Clear();
    char* GetText();
    char GetChar();
    uint16_t GetBytes(char* buf, unsigned int len);
    
  private:
    fifo_t fifo;
};

#endif
