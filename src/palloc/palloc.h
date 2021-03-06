#ifndef PALLOC_H
#define PALLOC_H

#include <stdlib.h>

/*#define PALLOC_DEBUG*/
/*#define PALLOC_ACTIVE*/
#define PALLOC_SENTINEL 1

void pfree(void *ptr);
void *_palloc(size_t size, const char *type);

#define palloc(T) \
  (T*)_palloc(sizeof(T), #T)

#endif
