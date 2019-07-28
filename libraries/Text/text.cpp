#include "text.h"


TEXT::TEXT(unsigned int size)
{
  fifo = new_fifo(size);
}

TEXT::~TEXT()
{
  delete_fifo(fifo);
}

uint16_t TEXT::AddText(char* str)
{
  return fifo_write(fifo, str, strlen(str));
}

uint16_t TEXT::AddText_P(const char* str)
{
  char ch;
  uint16_t i = 0;

  while((ch = (char)pgm_read_byte(str+i)))
  {
    if(!AddChar(ch)) return i;
    i++;
  }

  return i;
}

bool TEXT::AddChar(char ch)
{
  if(FIFO_FILLING(fifo) < FIFO_SIZE(fifo))
  {
    FIFO_PUT_BYTE(fifo, ch);
    return true;
  }
  return false;
}

char TEXT::GetChar()
{
  char ch = 0;

  FIFO_GET_BYTE(fifo, ch);  
  
  return ch;
}

uint16_t TEXT::GetBytes(char* buf, unsigned int len, bool flag_copy)
{
  return fifo_read(fifo, buf, len, flag_copy);
}

void TEXT::AddInt(unsigned int i)
{
  char *buf = (char*)calloc(1,7);
  AddText(itoa(i,buf,10));  
  free(buf);
}

void TEXT::AddFloat(float value)
{
  int i;
  AddInt(value);
  AddChar('.');
  i = value*100;
  AddInt(abs(i%100));  
}

uint16_t TEXT::filling()
{
  return FIFO_FILLING(fifo);
}

uint16_t TEXT::free_space()
{
  return FIFO_SIZE(fifo) - FIFO_FILLING(fifo);
}

void TEXT::Clear()
{
  FIFO_CLEAR(fifo);
}

char* TEXT::GetText()
{
  return (char*)fifo->buffer;
}