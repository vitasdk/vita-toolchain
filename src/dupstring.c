#include "dupstring.h"

#include <string.h>

#if defined(HAVE_STRNDUP) && HAVE_STRNDUP

char *dupstring(const char *source, size_t maxlen) {
  return strndup(source, maxlen);
}

#else /* Fallback implementation */
#include <stdlib.h>

char *dupstring(const char *source, size_t maxlen) {
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
