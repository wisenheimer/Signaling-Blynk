#ifndef __FIFO__INDCLUDED__
#define __FIFO__INDCLUDED__ 1

#include "fifo_macros.h"

/*
 * NOTE: it is possible to use USE_FIFO_WITHOUT_ADDITIONAL_CHECK preprocessor
 *       directive to remove additional check (valid pointers, parameters etc)
 */

#define FIFO_INVALID_HANDLE     0
#define FIFO_HEADER_SIZE        sizeof(struct fifo_s)

struct fifo_s
{
  unsigned int head;
  unsigned int tail;
  unsigned int filling;
  void        *buffer;
  unsigned int size;
  unsigned int allocated;
};

typedef struct fifo_s* fifo_t;

/*
 * Create new FIFO handle with given FIFO size.
 * Return: !0 -> Ok, 0 -> Errror.
 */
fifo_t new_fifo(unsigned int fifo_size);

/*
 * Create new FIFO handle in given buffer. Use this function if U have problem
 * with alignment. Actually FIFO size less than buffer size on FIFO_HEADER_SIZE.
 * Return: !0 -> Ok, 0 -> Errror.
 */
fifo_t new_fifo_from_memory(void *buffer, unsigned int buffer_size);

/*
 * Delete FIFO handle and free allocated data.
 */
void delete_fifo(fifo_t fifo);

/*
 * Put data to FIFO, but not change amount of byte in FIFO (only copy
 * data). Call fifo_write_commit() to commit fifo_write() operations.
 * Return: amount of byte which are placed in FIFO (0 if FIFO full).
 */
unsigned int fifo_write(fifo_t fifo, void *buffer, unsigned int buffer_size);

/*
 * Get data from FIFO, but not change amount of byte in FIFO (only copy
 * data). Call fifo_read_commit() to commit fifo_read() operations.
 * Return: amount of byte which are taken from FIFO (0 if FIFO empty).
 */
unsigned int fifo_read(fifo_t fifo, void *buffer, unsigned int buffer_size);

#endif /* __FIFO__INDCLUDED__ */

