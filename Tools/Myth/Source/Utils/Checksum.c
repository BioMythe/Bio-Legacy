#include "Checksum.h"

#define CRC32_POLYNOMINAL UINT32_C(0xEDB88320)

uint32_t ChecksumCRC32(const void* data, size_t size)
{
    const uint8_t* bytes = (const uint8_t*) data;
    uint32_t checksum = 0xffffffff;

    while (size--)
    {
        checksum ^= *bytes++;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (checksum & 1)
            {
                checksum = (checksum >> 1) ^ CRC32_POLYNOMINAL;
            }
            else
            {
                checksum >>= 1;
            }
        }
    }

    return checksum ^ 0xffffffff;
}
