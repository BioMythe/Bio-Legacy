#include "BioTime.h"

#include <time.h>

biotime_t FsGetBioTime(void)
{
    return ((biotime_t) time(NULL)) - FS_BIOTIME_EPOCH;
}
