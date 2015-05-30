#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#ifndef __MINGW32__
#include <endian.h>
#else
/* Todo: make these definitions work on BE systems! */
# define htole32(value) (value)
# define htole16(value) (value)

#define le32toh(value) htole32(value)
#define le16toh(value) htole16(value)
#endif

#endif
