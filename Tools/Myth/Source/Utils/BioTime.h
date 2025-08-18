#ifndef MYTH_UTILS_BIO_TIME_H
#define MYTH_UTILS_BIO_TIME_H

#include <stdint.h>

typedef uint64_t biotime_t;
#define FS_BIOTIME_EPOCH ((biotime_t) 1241654400) // May 7th, 2009 @ 00:00:00
biotime_t FsGetBioTime(void);

#endif // !MYTH_UTILS_BIO_TIME_H
