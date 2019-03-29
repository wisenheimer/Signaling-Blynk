#ifndef __FIFO__MACROS__INDCLUDED__
#define __FIFO__MACROS__INDCLUDED__ 1

//#include <assert.h>

#define FIFO_SIZE(_fifo)        ((_fifo)->size)
#define FIFO_FILLING(_fifo)     ((_fifo)->filling)

#define FIFO_CLEAR(_fifo)                               \
{                                                       \
  (_fifo)->head         = 0;                            \
  (_fifo)->tail         = 0;                            \
  (_fifo)->filling      = 0;                            \
  ((char*)(_fifo)->buffer)[0] = 0;						\
}

#define FIFO_GET_BYTE(_fifo, _byte)                     \
if((_fifo)->filling)                                    \
{                                                       \
  (_byte) = ((char*)(_fifo)->buffer)[(_fifo)->head];    \
  ((char*)(_fifo)->buffer)[(_fifo)->head++] = 0;		\
  (_fifo)->head %= (_fifo)->size;                       \
  (_fifo)->filling--;                                   \
}

#define FIFO_PUT_BYTE(_fifo, _byte)                     \
{                                                       \
  ((char*)(_fifo)->buffer)[(_fifo)->tail++] = (_byte);  \
  ((char*)(_fifo)->buffer)[(_fifo)->tail] = 0;			\
  (_fifo)->tail %= (_fifo)->size;                       \
  (_fifo)->filling++;                                   \
}

#endif /* __FIFO__MACROS__INDCLUDED__ */
