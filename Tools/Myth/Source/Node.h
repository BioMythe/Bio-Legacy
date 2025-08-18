/**
 * Header for node-related functions.
 */

#ifndef MYTH_NODE_H
#define MYTH_NODE_H

#include "FileSystem.h"

#include <stdio.h>
#include <stdbool.h>

typedef struct
{
    block_t  TableBlock;
    uint16_t Nest; // Index in node table block. Multiply with blocksize to get byte offset.
    uint64_t RawAddress;
} nodepos_t;

nodepos_t FsResolveNodePos(const FsMeta* pMeta, nodeid_t nodeID);
nodeid_t FsResolveNodeID(const FsMeta* pMeta, nodepos_t pos);

// Finds the first unused node nest (byte offset within block) within a node block. Returns 0xFFFF if all nests are full.
uint16_t FsFindNodeNest(FILE* pDisk, const FsMeta* pMeta, block_t nodeBlock);

// Finds an unused node ID within FS.
nodeid_t FsFindNodeID(FILE* pDisk, const FsMeta* pMeta);

FsNode FsInvalidNode(void);
bool   FsNodeExists(FILE* pDisk, const FsMeta* pMeta, nodeid_t nodeID);
FsNode FsGetNode(FILE* pDisk, const FsMeta* pMeta, nodeid_t nodeID);

typedef enum
{
    FS_WRITE_DATA_SUCCESSFUL              = 0,
    FS_WRITE_DATA_NODE_DOES_NOT_EXIST     = 1,
    FS_WRITE_DATA_DISK_ERROR              = 2, // I/O failure.
    FS_WRITE_DATA_ALLOCATION_ERROR        = 3, // FS unrelated, allocation error on host device.
    FS_WRITE_DATA_INSUFFICIENT_DISK_SPACE = 4, // FS does not have enough space to contain the data.
    FS_WRITE_DATA_TOO_BIG                 = 5  // FS cannot handle a node this big with the current configuration.
} write_node_data_result_t;
const char* FsWriteNodeDataResultToString(write_node_data_result_t result);

write_node_data_result_t FsWriteNodeData(FILE* pDisk, FsMeta* pMeta, nodeid_t nodeID, const void* pData, uint64_t szData);

typedef enum
{
    FS_MAKE_NODE_SUCCESSFUL              = 0,
    FS_MAKE_NODE_INTERMEDIATE_ERROR      = 1,
    FS_MAKE_NODE_EXISTS                  = 2, // FS already has this node.
    FS_MAKE_NODE_INVALID_ID              = 3, // ID 0 is unusable.
    FS_MAKE_NODE_INVALID_TYPE            = 4, // FS doesn't recognize given node type.
    FS_MAKE_NODE_DISK_ERROR              = 5, // I/O failure.
    FS_MAKE_NODE_ALLOCATION_ERROR        = 6, // FS unrelated, allocation error on host device.
    FS_MAKE_NODE_INSUFFICIENT_DISK_SPACE = 7, // FS does not have enough space to contain the data.
    FS_MAKE_NODE_DATA_TOO_BIG            = 8  // FS cannot handle a node this big with the current configuration.
} create_node_result_t;
const char* FsCreateNodeResultToString(create_node_result_t result);

create_node_result_t FsMakeNode(FILE* pDisk, FsMeta* pMeta, FsNode* pNode, const void* pData, uint64_t szData);
bool FsDeleteNode(FILE* pDisk, FsMeta* pMeta, nodeid_t nodeID);

typedef enum
{
    FS_REGISTER_NODE_SUCCESSFUL               = 0,
    FS_REGISTER_NODE_DIRECTORY_DOES_NOT_EXIST = 1, // The directory to enter to doesn't exist.
    FS_REGISTER_NODE_DOES_NOT_EXIST           = 2, // The node to register doesn't exist.
    FS_REGISTER_NODE_NOT_DIRECTORY            = 3, // Specified nodeID for the directory node is not a directory node.
} register_node_result_t;

// Registers the node to be apart of a directory.
register_node_result_t FsRegisterNode(FILE* pDisk, FsMeta* pMeta, nodeid_t dirNodeID, nodeid_t nodeID);

#endif // !MYTH_NODE_H
