#ifndef MYTH_UTILS_CHECKSUM_H
#define MYTH_UTILS_CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

uint32_t ChecksumCRC32(const void* data, size_t size);

#endif // !MYTH_UTILS_CHECKSUM_H
