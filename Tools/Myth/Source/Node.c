#include "Node.h"

#include "Utils/Math.h"
#include "Bitmap.h"
#include "Disk.h"

#include <stdlib.h>
#include <memory.h>
#include <math.h>

#define DOCASE(x) case x: return #x

const char* FsWriteNodeDataResultToString(write_node_data_result_t result)
{
    switch (result)
    {
        DOCASE(FS_WRITE_DATA_SUCCESSFUL);
        DOCASE(FS_WRITE_DATA_NODE_DOES_NOT_EXIST);
        DOCASE(FS_WRITE_DATA_DISK_ERROR);
        DOCASE(FS_WRITE_DATA_ALLOCATION_ERROR);
        DOCASE(FS_WRITE_DATA_INSUFFICIENT_DISK_SPACE);
        DOCASE(FS_WRITE_DATA_TOO_BIG);
    default: break;
    }

    return "((Invalid, Non-Standard Result))";
}

const char* FsCreateNodeResultToString(create_node_result_t result)
{
    switch (result)
    {
        DOCASE(FS_MAKE_NODE_SUCCESSFUL);
        DOCASE(FS_MAKE_NODE_INTERMEDIATE_ERROR);
        DOCASE(FS_MAKE_NODE_EXISTS);
        DOCASE(FS_MAKE_NODE_INVALID_ID);
        DOCASE(FS_MAKE_NODE_INVALID_TYPE);
        DOCASE(FS_MAKE_NODE_DISK_ERROR);
        DOCASE(FS_MAKE_NODE_ALLOCATION_ERROR);
        DOCASE(FS_MAKE_NODE_INSUFFICIENT_DISK_SPACE);
        DOCASE(FS_MAKE_NODE_DATA_TOO_BIG);
    default: break;
    }

    return "((Invalid, Non-Standard Result))";
}

nodepos_t FsResolveNodePos(const FsMeta* pMeta, nodeid_t nodeID)
{
    uint16_t nodesPerBlock = pMeta->BlockSize / FS_NODE_SIZE;

    nodepos_t pos;
    pos.TableBlock = pMeta->AddrNodeTable + (nodeID / nodesPerBlock);
    pos.Nest = nodeID % nodesPerBlock;
    pos.RawAddress = (pos.TableBlock * pMeta->BlockSize) + (pos.Nest * FS_NODE_SIZE);

    return pos;
}

nodeid_t FsResolveNodeID(const FsMeta* pMeta, nodepos_t pos)
{
    uint16_t nodesPerBlock = pMeta->BlockSize / FS_NODE_SIZE;
    block_t nodeBlock = pos.TableBlock - pMeta->AddrNodeTable;
    return nodeBlock * nodesPerBlock + pos.Nest;
}

uint16_t FsFindNodeNest(FILE* pDisk, const FsMeta* pMeta, block_t nodeBlock)
{
    if (nodeBlock < pMeta->AddrNodeTable || nodeBlock > (pMeta->AddrNodeTable - pMeta->AddrData))
    {
        printf("FsFindNodeNest failed, given node block %lu is not within the node table range.\n", nodeBlock);
        return 0xFFFF;
    }

    if (fseek(pDisk, nodeBlock * pMeta->BlockSize, SEEK_SET) != 0)
    {
        printf("FsFindNodeNest failed, couldn't seek to node block %lu on disk.\n", nodeBlock);
        return 0xFFFF;
    }

    FsNode* nodes = malloc(pMeta->BlockSize);
    if (!nodes)
    {
        printf("FsFindNodeNest failed, couldn't allocate memory to store block from node block %lu.\n", nodeBlock);
        return 0xFFFF;
    }
    if (fread(nodes, 1, pMeta->BlockSize, pDisk) != pMeta->BlockSize)
    {
        printf("FsFindNodeNest failed, couldn't read node block %lu on disk.\n", nodeBlock);
        free(nodes);
        return 0xFFFF;
    }

    uint16_t result = 0xFFFF;
    for (uint16_t i = nodeBlock == pMeta->AddrNodeTable ? 1 : 0; i < pMeta->BlockSize / FS_NODE_SIZE; i++)
    {
        if (nodes[i].ID == 0)
        {
            result = i;
            break;
        }
    }
    
    free(nodes);
    return result;
}

nodeid_t FsFindNodeID(FILE* pDisk, const FsMeta* pMeta)
{
    uint8_t* bitmap = FsLoadBitmap(pDisk, pMeta);
    if (!bitmap)
    {
        puts("FsFindNodeID failed due to FsLoadBitmap failing.");
        return FS_MAKE_NODE_DISK_ERROR;
    }

    // TODO: Add quick allocation using LastAllocatedNodeID.

    nodeid_t result;
    for (uint64_t bitmapBlock = pMeta->AddrBitmap; bitmapBlock < pMeta->AddrNodeTable; bitmapBlock++)
    {
        for (uint16_t byteOffset = 0; byteOffset < pMeta->BlockSize; byteOffset++)
        {
            uint8_t byte = bitmap[(bitmapBlock - pMeta->AddrBitmap) * pMeta->BlockSize + byteOffset];

            // all bits set?
            if (byte == 0xFF)
            {
                continue;
            }

            for (uint8_t i = 0; i < 8; i++)
            {
                // bit set, all node nests are used.
                if (byte & (1 << i))
                {
                    continue;
                }

                nodepos_t pos;
                pos.TableBlock = FsBitmapResolveToBlock(pMeta, (bitmappos_t) { .Block = bitmapBlock, .ByteOffset = byteOffset, .BitOffset = i });
                pos.Nest = FsFindNodeNest(pDisk, pMeta, pos.TableBlock);

                if (pos.Nest == 0xFFFF)
                {
                    printf("FsFindNodeID failed, node block %lu has no free node nests, contrary to the bitmap. Possible corruption?\n", pos.TableBlock);
                    result = FS_NODE_ID_INVALID;
                    goto Finish;
                }

                result = FsResolveNodeID(pMeta, pos);
                if (result == FS_NODE_ID_INVALID || result == FS_NODE_ID_JOURNAL || result == FS_NODE_ID_ROOT)
                {
                    continue;
                }

                goto Finish;
            }
        }
    }

Finish:
    free(bitmap);
    return result;
}

FsNode FsInvalidNode(void)
{
    FsNode node;
    memset(&node, 0, FS_NODE_SIZE);
    return node;
}

bool FsNodeExists(FILE* pDisk, const FsMeta* pMeta, nodeid_t nodeID)
{
    nodepos_t pos = FsResolveNodePos(pMeta, nodeID);

    if (fseek(pDisk, pos.RawAddress, SEEK_SET) != 0)
    {
        printf("FsNodeExists failed, couldn't seek to node %u's position {block %lu, nest %u} on disk.\n", nodeID, pos.TableBlock, pos.Nest);
        return 0;
    }

    FsNode node;
    if (fread(&node, 1, FS_NODE_SIZE, pDisk) != FS_NODE_SIZE)
    {
        printf("FsNodeExists failed, couldn't read node %u from disk on block %lu, nest %u.\n", nodeID, pos.TableBlock, pos.Nest);
        return 0;
    }

    return node.ID != 0;
}

FsNode FsGetNode(FILE* pDisk, const FsMeta* pMeta, nodeid_t nodeID)
{
    nodepos_t pos = FsResolveNodePos(pMeta, nodeID);

    if (fseek(pDisk, pos.RawAddress, SEEK_SET) != 0)
    {
        printf("FsGetNode failed, couldn't seek to node %u's position {block %lu, nest %u} on disk.\n", nodeID, pos.TableBlock, pos.Nest);
        return FsInvalidNode();
    }

    FsNode node;
    if (fread(&node, 1, FS_NODE_SIZE, pDisk) != FS_NODE_SIZE)
    {
        printf("FsGetNode failed, couldn't read node %u from disk on block %lu, nest %u.\n", nodeID, pos.TableBlock, pos.Nest);
        return FsInvalidNode();
    }

    if (node.ID == FS_NODE_ID_INVALID)
    {
        printf("FsGetNode failed, node %u doesn't exist.\n", nodeID);
        return FsInvalidNode();
    }

    return node;
}

void FsiBitmapSetBlocksBySinglyIndirect(FILE* pDisk, FsMeta* pMeta, block_t addrIndirect, uint8_t state)
{
    if (!addrIndirect || !FsBitmapCheckBlock(pDisk, pMeta, addrIndirect))
    {
        return;
    }
    FsBitmapSetBlock(pDisk, pMeta, addrIndirect, state);
    
    uint64_t ptrsPerBlock  = pMeta->BlockSize / sizeof(block_t);
    uint64_t totalBlocks   = ptrsPerBlock;

    for (uint64_t i = 0; i < totalBlocks; i++)
    {
        if (fseek(pDisk, addrIndirect + i * sizeof(block_t), SEEK_SET) != 0)
        {
            return;
        }
        block_t block;
        if (fread(&block, 1, sizeof(block_t), pDisk) != sizeof(block_t))
        {
            return;
        }
        FsiBitmapSetBlocksBySinglyIndirect(pDisk, pMeta, block, state);
    }
}

void FsiBitmapSetBlocksByDoublyIndirect(FILE* pDisk, FsMeta* pMeta, block_t addrIndirect, uint8_t state)
{
    if (!addrIndirect || !FsBitmapCheckBlock(pDisk, pMeta, addrIndirect))
    {
        return;
    }
    FsBitmapSetBlock(pDisk, pMeta, addrIndirect, state);
    
    uint64_t ptrsPerBlock  = pMeta->BlockSize / sizeof(block_t);
    uint64_t totalSinglies = ptrsPerBlock * ptrsPerBlock;

    for (uint64_t i = 0; i < totalSinglies; i++)
    {
        if (fseek(pDisk, addrIndirect + i * sizeof(block_t), SEEK_SET) != 0)
        {
            return;
        }
        block_t block;
        if (fread(&block, 1, sizeof(block_t), pDisk) != sizeof(block_t))
        {
            return;
        }
        FsiBitmapSetBlocksBySinglyIndirect(pDisk, pMeta, block, state);
    }
}

void FsiBitmapSetBlocksByTriplyIndirect(FILE* pDisk, FsMeta* pMeta, block_t addrIndirect, uint8_t state)
{
    if (!addrIndirect || !FsBitmapCheckBlock(pDisk, pMeta, addrIndirect))
    {
        return;
    }
    FsBitmapSetBlock(pDisk, pMeta, addrIndirect, state);
    
    uint64_t ptrsPerBlock  = pMeta->BlockSize / sizeof(block_t);
    uint64_t totalDoublies = ptrsPerBlock * ptrsPerBlock * ptrsPerBlock;

    for (uint64_t i = 0; i < totalDoublies; i++)
    {
        if (fseek(pDisk, addrIndirect + i * sizeof(block_t), SEEK_SET) != 0)
        {
            return;
        }
        block_t block;
        if (fread(&block, 1, sizeof(block_t), pDisk) != sizeof(block_t))
        {
            return;
        }
        FsiBitmapSetBlocksByDoublyIndirect(pDisk, pMeta, block, state);
    }
}

typedef struct
{
    uint64_t Size;              // Raw byte size.
    uint64_t DataBlocks;  // Total data blocks needed.
    uint64_t TotalBlocks; // Total blocks (including all indirections) needed.
} data_storage_t;

data_storage_t FsiCalculateDataStorage(FsMeta* pMeta, uint64_t size)
{
    // Number of block pointers one singly indirect block can point to.
    uint64_t ptrsPerBlock = pMeta->BlockSize / sizeof(block_t);
    
    data_storage_t storage;
    storage.Size        = size;
    storage.DataBlocks  = FS_DIV(size, pMeta->BlockSize); // FS_DIV divides to ceiling.
    storage.TotalBlocks = 0;

    if (storage.DataBlocks <= FS_NODE_DIRECT_DATA_BLOCKS)
    {
        // data fits inside direct data blocks.
        storage.TotalBlocks = storage.DataBlocks;
    }
    else
    {
        uint64_t remainingDataBlocks = storage.DataBlocks; // used for reference and counting
        
        // We already know that data doesn't fit into direct from the if check above
        // So no need to check this and also no need to check the singlies, we know we need at least singly indirect.
        storage.TotalBlocks += FS_NODE_DIRECT_DATA_BLOCKS;
        remainingDataBlocks -= FS_NODE_DIRECT_DATA_BLOCKS;

        // Singly indirect block itself.
        storage.TotalBlocks++;

        uint64_t singlyAddressingCapacity = ptrsPerBlock;
        uint64_t totalSinglySinglies = FS_MIN(remainingDataBlocks, singlyAddressingCapacity);
        storage.TotalBlocks += totalSinglySinglies;
        remainingDataBlocks -= totalSinglySinglies;

        // still got more data? its doubly indirection time.
        if (remainingDataBlocks)
        {
            // doubly block itself
            storage.TotalBlocks++;

            uint64_t doublyAddressingCapacity = singlyAddressingCapacity * ptrsPerBlock;
            uint64_t totalPtrsByDoubly = FS_MIN(remainingDataBlocks, doublyAddressingCapacity);
            uint64_t totalDoublySinglies = (totalPtrsByDoubly + ptrsPerBlock - 1) / ptrsPerBlock;

            storage.TotalBlocks += totalPtrsByDoubly + totalDoublySinglies;
            remainingDataBlocks -= totalPtrsByDoubly;

            // say my name
            // ... triply indirect.
            // you're goddamn right.
            // (coding this makes me wanna commit self shutdown)
            if (remainingDataBlocks)
            {
                // triply block itself
                storage.TotalBlocks++;

                uint64_t triplyAddressingCapacity = doublyAddressingCapacity * ptrsPerBlock;
                uint64_t totalPtrsByTriply = FS_MIN(remainingDataBlocks, triplyAddressingCapacity);
                uint64_t totalTriplyDoublies = (totalPtrsByTriply + doublyAddressingCapacity - 1) / doublyAddressingCapacity;
                uint64_t totalTriplySinglies = (totalPtrsByTriply + ptrsPerBlock - 1) / ptrsPerBlock;

                storage.TotalBlocks += totalPtrsByTriply + totalTriplyDoublies + totalTriplySinglies;
                remainingDataBlocks -= totalPtrsByTriply;

                // Still got data?
                if (remainingDataBlocks)
                {
                    // Indicate error by returning empty struct.
                    return (data_storage_t) {.Size=0,.DataBlocks=0,.TotalBlocks=0};
                }
            }
        }
    }

    return storage;
}

write_node_data_result_t FsWriteNodeData(FILE* pDisk, FsMeta* pMeta, nodeid_t nodeID, const void* pData, uint64_t szData)
{
    nodepos_t pos = FsResolveNodePos(pMeta, nodeID);
    if (fseek(pDisk, pos.RawAddress, SEEK_SET) != 0)
    {
        printf("FsWriteNodeData failed, couldn't seek to node %u's location on disk.\n", nodeID);
        return FS_WRITE_DATA_NODE_DOES_NOT_EXIST;
    }

    FsNode node;
    if (fread(&node, 1, FS_NODE_SIZE, pDisk) != FS_NODE_SIZE)
    {
        printf("FsWriteNodeData failed, couldn't read node %u on disk.\n", nodeID);
        return FS_WRITE_DATA_DISK_ERROR;
    }

    data_storage_t oldDataStorageInfo = FsiCalculateDataStorage(pMeta, node.Size);
    pMeta->NumAllocatedNodes -= oldDataStorageInfo.TotalBlocks;

    // Ensure size on node itself
    const uint8_t* data = (const uint8_t*) pData;
    node.Size = szData;

    // load bitmap, we'll need it
    uint8_t* bitmap = FsLoadBitmap(pDisk, pMeta);
    if (!bitmap)
    {
        puts("FsMakeNode failed due to FsLoadBitmap failing.");
        return FS_WRITE_DATA_ALLOCATION_ERROR;
    }

    // set every used data block as free
    for (uint16_t i = 0; i < FS_NODE_DIRECT_DATA_BLOCKS; i++)
    {
        if (node.DirectData[i])
        {
            FsBitmapSetBlock(pDisk, pMeta, node.DirectData[i], FS_BITMAP_BLOCK_FREE);
        }
    }
    FsiBitmapSetBlocksBySinglyIndirect(pDisk, pMeta, node.AddrSinglyIndirect, FS_BITMAP_BLOCK_FREE);
    FsiBitmapSetBlocksByDoublyIndirect(pDisk, pMeta, node.AddrDoublyIndirect, FS_BITMAP_BLOCK_FREE);
    FsiBitmapSetBlocksByTriplyIndirect(pDisk, pMeta, node.AddrTriplyIndirect, FS_BITMAP_BLOCK_FREE);

    // Make sure these are empty first as what we write to is szData-dependent and we don't want weird shit happening.
    memset(node.InlineData, 0, FS_NODE_INLINE_DATA_SIZE);
    memset(node.DirectData, 0, sizeof(block_t) * FS_NODE_DIRECT_DATA_BLOCKS);
    node.AddrSinglyIndirect = 0;
    node.AddrDoublyIndirect = 0;
    node.AddrTriplyIndirect = 0;

    // Write as much data as possible to inline section
    for (uint16_t i = 0; i < szData && i < FS_NODE_INLINE_DATA_SIZE; i++)
    {
        node.InlineData[i] = *data++;
    }
    
    // if data fit in the inline section, we can skip to writing the node directly.
    if (szData <= FS_NODE_INLINE_DATA_SIZE)
    {
        goto WriteNode;
    }

    // data didn't fit, remove the no. bytes we wrote to the inline section
    szData -= FS_NODE_INLINE_DATA_SIZE;
    
    //
    // TODO: Do quick calculation using LastAllocatedBlock
    //

    data_storage_t dataStorage = FsiCalculateDataStorage(pMeta, szData);
    block_t* pBlocks = malloc(dataStorage.TotalBlocks * sizeof(block_t));
    uint64_t tmpNumBlocksStored = 0; // represents num of elements in pBlocks.

    if (!pBlocks)
    {
        puts("FsWriteNodeData failed, couldn't allocate space to intermediately store clear data blocks.");
        return FS_WRITE_DATA_ALLOCATION_ERROR;
    }

    // locate clear blocks from bitmap
    bitmappos_t posCheck = FsBitmapResolveFromBlock(pMeta, pMeta->AddrData);

    for (block_t bitmapBlock = posCheck.Block; bitmapBlock < (pMeta->AddrNodeTable - pMeta->AddrBitmap); bitmapBlock++)
    {
        for (uint16_t byteOffset = posCheck.ByteOffset; byteOffset < pMeta->BlockSize; byteOffset++)
        {
            uint8_t byte = bitmap[(bitmapBlock - pMeta->AddrBitmap) * pMeta->BlockSize + byteOffset];
            
            if (byte == 0xFF)
            {
                continue;
            }

            for (uint8_t i = posCheck.BitOffset; i < 8; i++)
            {
                if (byte & (1 << i))
                {
                    continue;
                }

                // Block's clear! Save it.
                pBlocks[tmpNumBlocksStored++] = FsBitmapResolveToBlock(pMeta, (bitmappos_t) {
                    .Block = bitmapBlock, .ByteOffset = byteOffset, .BitOffset = i
                });

                if (tmpNumBlocksStored == dataStorage.TotalBlocks)
                {
                    // found enough sectors.
                    goto PostBitmapCheck;
                }
            }
        }

        // roll back to zero (needed because when initially getting posCheck byteOffset and bitOffset might be set already.)
        posCheck.ByteOffset = 0;
        posCheck.BitOffset  = 0;
    }
PostBitmapCheck:
    // No longer needed.
    free(bitmap);
    bitmap = NULL;
    
    // We either got here from out of the main bitmap block check loop or because we found enough sectors and escaped.
    // Check if we found enough sectors (with former, we didn't, with the latter, we did.)
    if (tmpNumBlocksStored != dataStorage.TotalBlocks)
    {
        free(pBlocks);
        printf("FsWriteNodeData failed, the disk doesn't have enough space to store a node worth %lu bytes of data.\n", node.Size);
        return FS_WRITE_DATA_INSUFFICIENT_DISK_SPACE;
    }

    uint64_t blocksLeft = dataStorage.TotalBlocks;
    // Write direct blocks. iBlocks is used to keep track of pBlocks and what sectors have already been assigned a task.
    uint64_t iBlocks;
    for (iBlocks = 0; iBlocks < dataStorage.TotalBlocks && iBlocks < FS_NODE_DIRECT_DATA_BLOCKS; iBlocks++)
    {
        node.DirectData[iBlocks] = pBlocks[iBlocks];
        
        uint64_t address = node.DirectData[iBlocks] * pMeta->BlockSize;
        if (fseek(pDisk, address, SEEK_SET) != 0)
        {
            puts("FsWriteNodeData failed, couldn't seek to direct block on disk.");
            return FS_WRITE_DATA_DISK_ERROR;
        }

        uint16_t bytesToWrite = FS_MIN(szData, pMeta->BlockSize);
        if (fwrite(data, 1, bytesToWrite, pDisk) != bytesToWrite)
        {
            puts("FsWriteNodeData failed, couldn't write to direct block on disk.");
            return FS_WRITE_DATA_DISK_ERROR;
        }
        FsBitmapSetBlock(pDisk, pMeta, node.DirectData[iBlocks], FS_BITMAP_BLOCK_ALLOCATED);
        data += bytesToWrite;
    }

    // Did it fit in the direct blocks?
    if (dataStorage.TotalBlocks <= FS_NODE_DIRECT_DATA_BLOCKS)
    {
        // go write the data to the blocks now.
        goto WriteNode;
    }
    
    // The data didn't fit into the direct blocks. Singly indirect time.
    //blocksLeft -= FS_NODE_DIRECT_DATA_BLOCKS;
    
    //pNode->AddrSinglyIndirect = pBlocks[iBlocks++]; // One block for the singly
    //blocksLeft -= 1 + FS_MIN(blocksLeft, ptrsPerBlock);

    pMeta->NumAllocatedBlocks += dataStorage.TotalBlocks;
    free(pBlocks);
WriteNode:
    
    node.TsCreated  = FsGetBioTime();
    node.TsAccessed = FsGetBioTime();
    node.TsModified = FsGetBioTime();
    
    if (fseek(pDisk, pos.RawAddress, SEEK_SET) != 0)
    {
        printf("FsWriteNodeData failed, couldn't seek to node nest on disk.\n");
        return FS_WRITE_DATA_DISK_ERROR;
    }

    if (fwrite(&node, 1, FS_NODE_SIZE, pDisk) != FS_NODE_SIZE)
    {
        printf("FsMakeNode failed, couldn't write node %u to it's position on disk (block %lu, nest %u).\n", node.ID, pos.TableBlock, pos.Nest);
        return FS_WRITE_DATA_DISK_ERROR;
    }

    pMeta->NumAllocatedNodes++;
    pMeta->LastAllocatedNodeID = node.ID;

    if (!FsWriteMeta(pDisk, pMeta))
    {
        puts("FsMakeNode failed, failed to overwrite file system metadata.");
        return FS_WRITE_DATA_DISK_ERROR;
    }

    return FS_MAKE_NODE_SUCCESSFUL;
}

create_node_result_t FsMakeNode(FILE* pDisk, FsMeta* pMeta, FsNode* pNode, const void* pData, uint64_t szData)
{
    /** Holy trio of checks */
    if (pNode->ID == FS_NODE_ID_INVALID)
    {
        puts("FsMakeNode failed, nodes cannot have the ID 0 because it represents invalidity.");
        return FS_MAKE_NODE_INVALID_ID;
    }
    if (FsNodeExists(pDisk, pMeta, pNode->ID))
    {
        printf("FsMakeNode failed, node %u already exists.\n", pNode->ID);
        return FS_MAKE_NODE_EXISTS;
    }
    if (pNode->Type != FS_NODE_TYPE_FILE && pNode->Type != FS_NODE_TYPE_DIRECTORY && pNode->Type != FS_NODE_TYPE_SOFT_LINK)
    {
        printf("FsMakeNode failed, node type %u doesn't correspond to any valid Myth File System node type.\n", pNode->Type);
        return FS_MAKE_NODE_INVALID_TYPE;
    }

    // Pseudo-write node to the table so FsWriteNodeData doesn't fail.
    nodepos_t pos = FsResolveNodePos(pMeta, pNode->ID);
    if (fseek(pDisk, pos.RawAddress, SEEK_SET) != 0)
    {
        printf("FsMakeNode failed, couldn't seek to node %u's location on disk.\n", pNode->ID);
        return FS_MAKE_NODE_DISK_ERROR;
    }

    if (fwrite(pNode, 1, FS_NODE_SIZE, pDisk) != FS_NODE_SIZE)
    {
        printf("FsMakeNode failed, couldn't write to node %u's location on disk.\n", pNode->ID);
        return FS_MAKE_NODE_DISK_ERROR;
    }

    write_node_data_result_t writeResult = FsWriteNodeData(pDisk, pMeta, pNode->ID, pData, szData);
    if (writeResult != FS_WRITE_DATA_SUCCESSFUL)
    {
        printf("FsMakeNode failed, FsWriteNodeData returned non-succesful return value %u (%s).\n", writeResult, FsWriteNodeDataResultToString(writeResult));
        switch (writeResult)
        {
        case FS_WRITE_DATA_NODE_DOES_NOT_EXIST:     return FS_MAKE_NODE_INTERMEDIATE_ERROR;
        case FS_WRITE_DATA_DISK_ERROR:              return FS_MAKE_NODE_DISK_ERROR;
        case FS_WRITE_DATA_ALLOCATION_ERROR:        return FS_MAKE_NODE_ALLOCATION_ERROR;
        case FS_WRITE_DATA_INSUFFICIENT_DISK_SPACE: return FS_MAKE_NODE_INSUFFICIENT_DISK_SPACE;
        case FS_WRITE_DATA_TOO_BIG:                 return FS_MAKE_NODE_DATA_TOO_BIG;
        default: return -1;
        }
    }

    return FS_MAKE_NODE_SUCCESSFUL;
}

bool FsDeleteNode(FILE* pDisk, FsMeta* pMeta, nodeid_t nodeID)
{
    puts("!!! not implemented !!!");
    return false;
}
