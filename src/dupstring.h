#ifndef DUPSTRING_H
#define DUPSTRING_H

#include <stddef.h>

/* Wrapper around strndup */
char *dupstring(const char *source, size_t maxlen);

#endif /* DUPSTRING_H */
