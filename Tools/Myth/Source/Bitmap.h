/**
 * Header for bitmap related functionality.
 */

#ifndef MYTH_BITMAP_H
#define MYTH_BITMAP_H

#include "FileSystem.h"

#include <stdio.h>

#define FS_BITMAP_BLOCK_FREE      UINT8_C(0)
#define FS_BITMAP_BLOCK_ALLOCATED UINT8_C(1)
#define FS_BITMAP_BLOCK_IVLD      UINT8_C(2) // Non-possible value, used to indicate errors from FS functions.

/** Structure representing a positin within the bitmap. */
typedef struct
{
    block_t  Block;
    uint16_t ByteOffset;
    uint8_t  BitOffset;
} bitmappos_t;

bitmappos_t FsBitmapResolveFromBlock(const FsMeta* pMeta, block_t block);
block_t FsBitmapResolveToBlock(const FsMeta* pMeta, bitmappos_t pos);

uint8_t FsBitmapCheckBlock(FILE* pDisk, const FsMeta* pMeta, block_t block);
uint8_t FsBitmapSetBlock(FILE* pDisk, const FsMeta* pMeta, block_t block, uint8_t status);

// Loads entire bitmap section from the disk. Must be free'd manually by the caller.
uint8_t* FsLoadBitmap(FILE* pDisk, const FsMeta* pMeta);

#endif // !MYTH_BITMAP_H
