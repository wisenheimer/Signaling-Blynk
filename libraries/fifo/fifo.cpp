
#include <stdlib.h>
#include <string.h>
//#include <assert.h>
#include "fifo.h"

/*
 * Create/Delete FIFO.
 */
fifo_t new_fifo_from_memory(void *buffer, unsigned int buffer_size)
{
  fifo_t fifo = 0;

  if (buffer && buffer_size > FIFO_HEADER_SIZE)
  {
    fifo                      = (fifo_t)buffer;
    fifo->head                = 0;
    fifo->tail                = 0;
    fifo->filling             = 0;
    fifo->buffer              = (char*)buffer + FIFO_HEADER_SIZE;
    fifo->size                = buffer_size - FIFO_HEADER_SIZE;
    fifo->allocated           = 0;
    ((char*)fifo->buffer)[0]  = 0;
  }

  return fifo;
}

fifo_t new_fifo(unsigned int fifo_size)
{
  fifo_t fifo = 0;
  void* buffer;

  if (fifo_size)
  {
    buffer = malloc(FIFO_HEADER_SIZE + fifo_size);
    if (buffer)
    {
      fifo = new_fifo_from_memory(buffer, FIFO_HEADER_SIZE + fifo_size);
      fifo->allocated = 1;
    }
  }

  return fifo;
}

void delete_fifo(fifo_t fifo)
{
  if (fifo)
  {
    if (fifo->allocated)
    {
      free(fifo);
    }
  }
}

/*
 * Write data to FIFO.
 */
unsigned int fifo_write(fifo_t fifo, void *buffer, unsigned int buffer_size)
{
  unsigned int cnt, cnt_rest;
  unsigned int empty_size, fifo_real_tail;

  empty_size = fifo->size - fifo->filling;
  fifo_real_tail = fifo->tail % fifo->size;
  if (buffer_size > empty_size)
  {
    // Restrict buffer size!
    buffer_size = empty_size;
  }

  cnt_rest = buffer_size;
  if (cnt_rest)
  {
    cnt = ((fifo->size - fifo_real_tail) < cnt_rest) ? (fifo->size - fifo_real_tail) : (cnt_rest);
    memcpy(((char*)fifo->buffer) + fifo_real_tail, buffer, cnt);
    cnt_rest -= cnt;
    if (cnt_rest)
    {
      memcpy(fifo->buffer, ((char*)buffer) + cnt, cnt_rest);
    }
    fifo->tail = (fifo->tail + buffer_size) % fifo->size;
    fifo->filling += buffer_size;
    ((char*)fifo->buffer)[fifo->tail] = 0;   
  }

  return buffer_size;
}

/*
 * Read data from FIFO.
 */
unsigned int fifo_read(fifo_t fifo, void *buffer, unsigned int buffer_size, bool flag_copy)
{
  unsigned int cnt, cnt_rest;
  unsigned int fifo_real_head;

  if(!fifo->filling) return 0;

  if (buffer_size > fifo->filling)
  {
    // Restrict buffer size!
    buffer_size = fifo->filling;
  }

  cnt_rest = buffer_size;
  if (cnt_rest)
  {
    fifo_real_head = fifo->head % fifo->size;
    cnt = ((fifo->size - fifo_real_head) < cnt_rest) ? (fifo->size - fifo_real_head) : (cnt_rest);
    memcpy(buffer, ((char*)fifo->buffer) + fifo_real_head, cnt);
    cnt_rest -= cnt;
    if (cnt_rest)
    {
      memcpy(((char*)buffer) + cnt, fifo->buffer, cnt_rest);
    }
    if(!flag_copy)
    {
      fifo->head = (fifo->head + buffer_size) % fifo->size;
      fifo->filling -= buffer_size;
    }
  } 

  return buffer_size;
}