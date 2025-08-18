#include "Disk.h"

#include "Utils/Checksum.h"

#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

const char* FsMakeFsStatusToString(makefs_status_t status)
{
    switch (status)
    {
    #define DOCASE(x) case x: return #x;
        DOCASE(FS_MAKE_FILE_SYSTEM_SUCCESSFUL);
        DOCASE(FS_MAKE_FILE_SYSTEM_MISC_FAILURE);
        DOCASE(FS_MAKE_FILE_SYSTEM_INVALID_PARAMETER);
        DOCASE(FS_MAKE_FILE_SYSTEM_DISK_ERROR);
        DOCASE(FS_MAKE_FILE_SYSTEM_INSANE_BLOCK_SIZE);
        DOCASE(FS_MAKE_FILE_SYSTEM_INSUFFICIENT_DISK_SIZE);
        DOCASE(FS_MAKE_FILE_SYSTEM_INVALID_HEADER);
        DOCASE(FS_MAKE_FILE_SYSTEM_INVALID_TAIL);
        DOCASE(FS_MAKE_FILE_SYSTEM_INVALID_CHECKSUM);
        DOCASE(FS_MAKE_FILE_SYSTEM_INVALID_CONFIGURATION_HEADER);
    #undef DOCASE
    default: break;
    }

    return "((null))";
}

bool FsWriteMeta(FILE* pDisk, FsMeta* pMeta)
{
    pMeta->Checksum = ChecksumCRC32(pMeta, sizeof(FsMeta) - sizeof(uint32_t));
    
    uint64_t addrMetadata = pMeta->Origin * pMeta->BlockSize;
    if (fseek(pDisk, addrMetadata, SEEK_SET) != 0)
    {
        printf("FsMakeFileSystem failed, seek to metadata address (block %lu, raw address 0x%x) failed.", pMeta->Origin, (uint32_t) addrMetadata);
        return false;
    }

    if (fwrite(pMeta, 1, sizeof(FsMeta), pDisk) != sizeof(FsMeta))
    {
        puts("FsMakeFileSystem failed, couldn't write metadata to the disk.");
        return false;
    }
    return true;
}

makefs_status_t FsMakeFileSystem(FILE* pDisk, FsMeta* pMeta, uint64_t bytesPerNodeRatio)
{
    assert(pDisk && pMeta);
    // We need a *somewhat* reasonable ratio.
    if (bytesPerNodeRatio < FS_MINIMUM_BLOCK_SIZE)
    {
        printf("FsMakeFileSystem failed, bytes per node ratio cannot be smaller than %u, but got %lu.\n", FS_MINIMUM_BLOCK_SIZE, bytesPerNodeRatio);
        return FS_MAKE_FILE_SYSTEM_INVALID_PARAMETER;
    }

    if (pMeta->BlockSize % FS_MINIMUM_BLOCK_SIZE)
    {
        printf("FsMakeFileSystem failed, block size must be a multiple of %u.\n", FS_MINIMUM_BLOCK_SIZE);
        return FS_MAKE_FILE_SYSTEM_INSANE_BLOCK_SIZE;
    }

    uint64_t diskSize = pMeta->Size * pMeta->BlockSize;
    ftruncate(fileno(pDisk), diskSize);
    
    pMeta->AddrBitmap = pMeta->Origin + 1;
    uint32_t trackedBlocksPerBitmapBlock = pMeta->BlockSize * 8; // Each byte can track 8 blocks.
    
    uint64_t bitmapSize = (pMeta->Size - pMeta->AddrBitmap + trackedBlocksPerBitmapBlock - 1) / trackedBlocksPerBitmapBlock;
    // Cut off excess size from bitmap that is due to bitmap's existence.
    // Bitmap size is a 2 step calculation, we first need to know how big the bitmap is for the entire disk first
    // in order to cut off the bitmao itself from this size.
    bitmapSize -= bitmapSize / trackedBlocksPerBitmapBlock;

    // zero the bitmap
    {
        uint64_t rawBitmapSize = bitmapSize * pMeta->BlockSize;
        uint8_t* bitmap = malloc(rawBitmapSize);
        memset(bitmap, 0, rawBitmapSize);

        uint64_t bitmapAddress = pMeta->AddrBitmap * pMeta->BlockSize;
        if (fseek(pDisk, bitmapAddress, SEEK_SET) != 0)
        {
            printf("FsMakeFileSystem failed, seek to bitmap address (block %lu, raw address 0x%x) failed.", pMeta->AddrBitmap, (uint32_t) bitmapAddress);
            free(bitmap);
            return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
        }
        
        if (fwrite(bitmap, 1, rawBitmapSize, pDisk) != rawBitmapSize)
        {
            puts("FsMakeFileSystem failed, failed to write clear bytes to the bitmap.");
            free(bitmap);
            return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
        }

        free(bitmap);
    }

    pMeta->AddrNodeTable = pMeta->AddrBitmap + bitmapSize;
    pMeta->NodeCapacity = pMeta->Size * pMeta->BlockSize / bytesPerNodeRatio;
    uint32_t nodeTableBlocks = pMeta->NodeCapacity / (pMeta->BlockSize / FS_NODE_SIZE);
    pMeta->AddrData = pMeta->AddrNodeTable + nodeTableBlocks;
    
    // After doing all calculations related to this, subtract one because node 0 is always unavailable.
    // But during critical size calculation like above, the original size needs to be used.
    pMeta->NodeCapacity--;

    pMeta->LastAllocatedDataBlock = pMeta->AddrData;
    pMeta->LastAllocatedNodeID = FS_NODE_ID_INVALID;

    if (pMeta->Size <= pMeta->AddrData)
    {
        puts("FsMakeFileSystem failed, disk is too small to contain the file system with the current configuration.");
        return FS_MAKE_FILE_SYSTEM_INSUFFICIENT_DISK_SIZE;
    }

    pMeta->ErrorState  = FS_ERROR_STATE_NORMAL;
    pMeta->ErrorAction = FS_ERROR_ACTION_NONE;

    pMeta->TsCreated = FsGetBioTime();
    pMeta->TsMounted = 0; // Not mounted yet.

    const char* uidCharset = FS_UNIQUE_ID_CHARSET;
    for (uint16_t i = 0; i < FS_UNIQUE_ID_SIZE; i++)
    {
        pMeta->UniqueID[i] = uidCharset[rand() % sizeof(FS_UNIQUE_ID_CHARSET)];
    }
    
    pMeta->NumAllocatedBlocks = pMeta->AddrNodeTable; // Up to AddrData we count everything as allocated (data before metadata block is considered allocated/reserved so we count that too).
    pMeta->NumAllocatedNodes = 0;
    pMeta->AddrExtension = 0;
    pMeta->CreatorID = FS_CREATOR_MYTH_TOOL;
    
    memcpy(pMeta->Header, FS_HEADER_STRING, FS_HEADER_SIZE);
    pMeta->Tail = FS_TAIL;

    if (!FsWriteMeta(pDisk, pMeta))
    {
        puts("FsMakeFileSystem failed, couldn't write metadata to the disk.");
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }
    
    {
        uint16_t rem = pMeta->BlockSize - sizeof(FsMeta);
        if (rem)
        {
            uint8_t* pad = malloc(rem);
            memset(pad, 0, rem);
            
            if (fwrite(pad, 1, rem, pDisk) != rem)
            {
                puts("FsMakeFileSystem failed, couldn't write metadata block padding to the disk.");
                free(pad);
                return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
            }
            
            free(pad);
        }
    }

    FsConfigChunk configChunk;
    memcpy(configChunk.Header, FS_CONFIG_HEADER_STRING, FS_CONFIG_HEADER_SIZE);
    configChunk.BytesPerBlock = pMeta->BlockSize;
    configChunk.FileSystemOffset = pMeta->Origin;

    if (fseek(pDisk, 0 + 2, SEEK_SET) != 0)
    {
        puts("FsMakeFileSystem failed, couldn't seek to Configuration Chunk on disk.");
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }

    if (fwrite(&configChunk, 1, sizeof(FsConfigChunk), pDisk) != sizeof(FsConfigChunk))
    {
        puts("FsMakeFileSystem failed, couldn't write Configuration Chunk to disk.");
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }

    return FS_MAKE_FILE_SYSTEM_SUCCESSFUL;
}

makefs_status_t FsReadFileSystem(FILE* pDisk, FsMeta* pDest)
{
    // From the disk start, jump over the JMP SHORT reserved space.
    if (fseek(pDisk, 0 + 2, SEEK_SET) != 0)
    {
        puts("FsReadFileSystem failed, couldn't seek to the magic header offset.");
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }

    FsConfigChunk configChunk;
    if (fread(&configChunk, 1, sizeof(FsConfigChunk), pDisk) != sizeof(FsConfigChunk))
    {
        puts("FsReadFileSystem failed, failed to read Configuration Chunk from disk.");
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }

    if (memcmp(configChunk.Header, FS_CONFIG_HEADER_STRING, FS_CONFIG_HEADER_SIZE) != 0)
    {
        printf("FsReadFileSystem failed, Configuration Sector lacks a proper Myth File System Configuration Header. "
               "Expected header '" FS_CONFIG_HEADER_STRING "', but read '%." FS_STRINGIZE(FS_CONFIG_HEADER_SIZE) "s'. "
               "The lack of this header means that this disk does not contain a valid Myth File System.\n",
                configChunk.Header);
        return FS_MAKE_FILE_SYSTEM_INVALID_CONFIGURATION_HEADER;
    }

    uint64_t fsOffset = configChunk.FileSystemOffset * configChunk.BytesPerBlock;
    if (fseek(pDisk, fsOffset, SEEK_SET) != 0)
    {
        printf("FsReadFileSystem failed, couldn't seek to file system at offset (block %lu, raw address %lu) on disk.\n", configChunk.FileSystemOffset, fsOffset);
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }

    if (fread(pDest, 1, sizeof(FsMeta), pDisk) != sizeof(FsMeta))
    {
        printf("FsReadFileSystem failed, couldn't read file system metadata at offset (block %lu, raw address %lu) from disk.\n", configChunk.FileSystemOffset, fsOffset);
        return FS_MAKE_FILE_SYSTEM_DISK_ERROR;
    }

    if (memcmp(pDest->Header, FS_HEADER_STRING, FS_HEADER_SIZE) != 0)
    {
        printf("FsReadFileSystem failed, metadata block lacks a proper Myth File System header. "
               "Expected header '" FS_HEADER_STRING "', but read '%." FS_STRINGIZE(FS_HEADER_SIZE) "s'.\n",
               pDest->Header);
        return FS_MAKE_FILE_SYSTEM_INVALID_HEADER;
    }

    if (pDest->Tail != FS_TAIL)
    {
        printf("FsReadFileSystem failed, metadata block contains an invalid Myth File System tail value. "
               "Expected tail value '" FS_STRINGIZE(FS_TAIL) "', but read '%x'.\n", pDest->Tail);
        return FS_MAKE_FILE_SYSTEM_INVALID_TAIL;
    }

    uint32_t checksum = ChecksumCRC32(pDest, sizeof(FsMeta) - sizeof(uint32_t));
    if (pDest->Checksum != checksum)
    {
        printf("FsReadFileSystem failed, metadata checksum doesn't match with the freshly calculated checksum value for the block. "
               "Metadata block has checksum %u, but calculated checksum was %u.\n", pDest->Checksum, checksum);
        return FS_MAKE_FILE_SYSTEM_INVALID_CHECKSUM;
    }
    
    return FS_MAKE_FILE_SYSTEM_SUCCESSFUL;
}

FileSystemOnDisk FsLoadFileSystemOnDisk(const char* pDiskPath)
{
    FileSystemOnDisk result;
    memset(&result, 0, sizeof(FileSystemOnDisk));
    
    if (!(result.pDisk = fopen(pDiskPath, "rb")))
    {
        printf("FsLoadFileSystemOnDisk fail, couldn't open disk from path '%s'.\n", pDiskPath);
        return result;
    }

    makefs_status_t loadStatus = FsReadFileSystem(result.pDisk, &result.Meta);
    if (loadStatus != FS_MAKE_FILE_SYSTEM_SUCCESSFUL)
    {
        printf("FsLoadFileSystemOnDisk failed, FsReadFileSystem returned code %u (%s).\n", loadStatus, FsMakeFsStatusToString(loadStatus));
        
        fclose(result.pDisk);
        result.pDisk = NULL;
        memset(&result.Meta, 0, sizeof(FsMeta));
        
        return result;
    }

    result.bLoaded = true;
    return result;
}

void FsCloseDisk(FileSystemOnDisk fsOnDisk)
{
    if (fsOnDisk.bLoaded && fsOnDisk.pDisk)
    {
        fclose(fsOnDisk.pDisk);
        fsOnDisk.pDisk = NULL;
    }
}
