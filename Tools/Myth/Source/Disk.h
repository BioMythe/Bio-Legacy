#ifndef MYTH_DISK_H
#define MYTH_DISK_H

#include "FileSystem.h"

#include <stdio.h>
#include <stdbool.h>

typedef enum
{
    FS_MAKE_FILE_SYSTEM_SUCCESSFUL,
    FS_MAKE_FILE_SYSTEM_MISC_FAILURE,
    FS_MAKE_FILE_SYSTEM_INVALID_PARAMETER,
    FS_MAKE_FILE_SYSTEM_DISK_ERROR,
    FS_MAKE_FILE_SYSTEM_INSANE_BLOCK_SIZE,
    FS_MAKE_FILE_SYSTEM_INSUFFICIENT_DISK_SIZE,
    FS_MAKE_FILE_SYSTEM_INVALID_HEADER,
    FS_MAKE_FILE_SYSTEM_INVALID_TAIL,
    FS_MAKE_FILE_SYSTEM_INVALID_CHECKSUM,
    FS_MAKE_FILE_SYSTEM_INVALID_CONFIGURATION_HEADER
} makefs_status_t;
const char* FsMakeFsStatusToString(makefs_status_t status);

// Rewrites the pMeta. Useful when a field is changed and changes have to be replicated to disk.
bool FsWriteMeta(FILE* pDisk, FsMeta* pMeta);

makefs_status_t FsMakeFileSystem(FILE* pDisk, FsMeta* pMeta, uint64_t bytesPerNodeRatio);
makefs_status_t FsReadFileSystem(FILE* pDisk, FsMeta* pDest);

typedef struct
{
    FILE*   pDisk;
    FsMeta  Meta;
    bool    bLoaded;
} FileSystemOnDisk;

FileSystemOnDisk FsLoadFileSystemOnDisk(const char* pDiskPath);
void FsCloseDisk(FileSystemOnDisk fsOnDisk);

#endif // MYTH_DISK_H
