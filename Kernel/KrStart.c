/**
 * 
 * KrnlMgmt.c
 * Translation unit containing the kernel entry point, responsible for initialization and then routing management.
 * 
 */

#include <stdint.h>

__attribute__((noreturn))
void KrStart(void)
{
    uint8_t* pVideoMem = (uint8_t*) 0xB8000;
    *pVideoMem++ = 'B';
    *pVideoMem++ = 0x0F;

    while (1);
}
