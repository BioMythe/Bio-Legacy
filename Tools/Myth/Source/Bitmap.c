#include "Bitmap.h"

#include <stdio.h>
#include <stdlib.h>

bitmappos_t FsBitmapResolveFromBlock(const FsMeta* pMeta, block_t block)
{
    uint32_t blocksPerBitmapBlock = pMeta->BlockSize * 8;
    // The bitmap starts tracking blocks from AddrNodeTable, all blocks before are untracked (including metadata and bitmap itself).
    block_t offsetedBlock = block - pMeta->AddrNodeTable;
    
    bitmappos_t pos;
    pos.Block = pMeta->AddrBitmap + (offsetedBlock / blocksPerBitmapBlock);
    pos.ByteOffset = (offsetedBlock % blocksPerBitmapBlock) / 8;
    pos.BitOffset = offsetedBlock % 8;
    
    return pos;
}

block_t FsBitmapResolveToBlock(const FsMeta* pMeta, bitmappos_t pos)
{
    uint32_t blocksPerBitmapBlock = pMeta->BlockSize * 8;
    uint64_t blockIndex = pos.Block - pMeta->AddrBitmap;
    return pMeta->AddrNodeTable + ((blockIndex * blocksPerBitmapBlock) + (pos.ByteOffset * 8) + pos.BitOffset);
}

uint8_t FsBitmapCheckBlock(FILE* pDisk, const FsMeta* pMeta, block_t block)
{
    if (block < pMeta->AddrNodeTable)
    {
        printf("FsBitmapCheckBlock failed, the bitmap doesn't track any blocks before AddrNodeTable (on this FS, block %lu), however "
               "the function was to check the block %lu, which is before the node table.\n", pMeta->AddrNodeTable, block);
        return FS_BITMAP_BLOCK_IVLD;
    }

    bitmappos_t pos = FsBitmapResolveFromBlock(pMeta, block);
    if (fseek(pDisk, pos.Block * pMeta->BlockSize + pos.ByteOffset, SEEK_SET) != 0)
    {
        printf("FsBitmapCheckBlock failed, couldn't seek to block %lu's position on disk.\n", block);
        return FS_BITMAP_BLOCK_IVLD;
    }

    uint8_t byte;
    if (fread(&byte, 1, sizeof(uint8_t), pDisk) != sizeof(uint8_t))
    {
        printf("FsBitmapCheckBlock failed, couldn't read bitmap byte %u in block %lu.", pos.ByteOffset, pos.Block);
        return FS_BITMAP_BLOCK_IVLD;
    }

    return byte & (1 << pos.BitOffset);
}

uint8_t FsBitmapSetBlock(FILE* pDisk, const FsMeta* pMeta, block_t block, uint8_t status)
{
    if (!block)
    {
        return FS_BITMAP_BLOCK_IVLD;
    }
    if (block < pMeta->AddrNodeTable)
    {
        printf("FsBitmapSetBlock failed, the bitmap doesn't track any blocks before AddrNodeTable (on this FS, block %lu), however "
               "the function was to check the block %lu, which is before the node table.\n", pMeta->AddrNodeTable, block);
        return FS_BITMAP_BLOCK_IVLD;
    }

    bitmappos_t pos = FsBitmapResolveFromBlock(pMeta, block);
    if (fseek(pDisk, pos.Block * pMeta->BlockSize + pos.ByteOffset, SEEK_SET) != 0)
    {
        printf("FsBitmapCheckBlock failed, couldn't seek to block %lu's position on disk.\n", block);
        return 0;
    }

    uint8_t byte;
    if (fread(&byte, 1, sizeof(uint8_t), pDisk) != sizeof(uint8_t))
    {
        printf("FsBitmapCheckBlock failed, couldn't read bitmap byte %u in block %lu.", pos.ByteOffset, pos.Block);
        return 0;
    }

    if (status)
    {
        byte |= (1 << pos.BitOffset);
    }
    else
    {
        byte &= ~(1 << pos.BitOffset);
    }

    if (fseek(pDisk, pos.Block * pMeta->BlockSize + pos.ByteOffset, SEEK_SET) != 0)
    {
        printf("FsBitmapCheckBlock failed, couldn't seek to block %lu's position on disk.\n", block);
        return 0;
    }

    if (fwrite(&byte, 1, sizeof(uint8_t), pDisk) != sizeof(uint8_t))
    {
        printf("FsBitmapCheckBlock failed, couldn't write to block %lu's position on disk.\n", block);
        return 0;
    }

    return 1;
}

uint8_t* FsLoadBitmap(FILE* pDisk, const FsMeta* pMeta)
{
    uint64_t rawBitmapSize = (pMeta->AddrNodeTable - pMeta->AddrBitmap) * pMeta->BlockSize;
    uint8_t* bitmap = malloc(rawBitmapSize);

    if (!bitmap)
    {
        puts("FsLoadBitmap failed, couldn't allocate space for intermediate bitmap representation.");
        return NULL;
    }
    
    if (fseek(pDisk, pMeta->AddrBitmap * pMeta->BlockSize, SEEK_SET) != 0)
    {
        printf("FsLoadBitmap failed, couldn't seek to bitmap at block %lu\n", pMeta->AddrBitmap);
        return NULL;
    }

    if (fread(bitmap, 1, rawBitmapSize, pDisk) != rawBitmapSize)
    {
        printf("FsLoadBitmap failed, couldn't read bitmap at block %lu\n", pMeta->AddrBitmap);
        free(bitmap);
        return NULL;
    }

    return bitmap;
}
