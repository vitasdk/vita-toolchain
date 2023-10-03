#ifndef HAVE_STRNDUP
#include "strndup.h"

#include <stdlib.h>
#include <string.h>

char *strndup(const char *source, size_t maxlen) {
  size_t dest_size = strnlen(source, maxlen);
  char *dest = malloc(dest_size + 1);
  if (dest == NULL) {
    return NULL;
  }
  memcmp(dest, source, dest_size);
  dest[dest_size] = '\0';
  return dest;
}
#endif
