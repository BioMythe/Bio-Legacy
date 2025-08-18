#ifndef BIO_KERNEL_DRIVER_INTERFACE_BFSI_H
#define BIO_KERNEL_DRIVER_INTERFACE_BFSI_H

#include <stdint.h>

typedef uint64_t FileDescriptor;

typedef struct __attribute__((packed))
{
    FileDescriptor (*BfsOpenFile)(const char* pPath, uint32_t flags);
} BFSI;

#endif // BIO_KERNEL_DRIVER_INTERFACE_BFSI_H
