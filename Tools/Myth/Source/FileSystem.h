#ifndef MYTH_FILE_SYSTEM_H
#define MYTH_FILE_SYSTEM_H

#include "Utils/MiscDefs.h"
#include "Utils/BioTime.h"

#define FS_CONFIG_HEADER_STRING "MYTH"
#define FS_CONFIG_HEADER_SIZE    4

#define FS_HEADER_STRING "FSMETA"
#define FS_HEADER_SIZE    6
#define FS_TAIL           0xB10F5CC7

#define FS_VOLUME_NAME_SIZE 32
#define FS_UNIQUE_ID_SIZE   16
#define FS_VENDOR_ID_SIZE   12

#define FS_UNIQUE_ID_CHARSET "0123456789" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#define FS_MINIMUM_BLOCK_SIZE UINT16_C(512)  // Every block size must be a multiple of this size.
#define FS_DEFAULT_BLOCK_SIZE UINT16_C(4096)

#define FS_CREATOR_WILDCARD  UINT8_C(0)
#define FS_CREATOR_MYTH_TOOL UINT8_C(1)
#define FS_CREATOR_BIO_OS    UINT8_C(2)

#define FS_ERROR_STATE_PRENORMAL  UINT8_C(0)
#define FS_ERROR_STATE_NORMAL     UINT8_C(1)
#define FS_ERROR_STATE_ABNORMAL   UINT8_C(2)

#define FS_ERROR_ACTION_NONE      UINT8_C(0)
#define FS_ERROR_ACTION_READ_ONLY UINT8_C(1)
#define FS_ERROR_ACTION_BIOIZATE  UINT8_C(2)

#define FS_INITIAL_MAJOR    UINT16_C(1)
#define FS_LATEST_MAJOR     FS_INITIAL_MAJOR

#define FS_INITIAL_REVISION UINT16_C(0)
#define FS_LATEST_REVISION  FS_INITIAL_REVISION

typedef uint32_t nodeid_t;
typedef uint64_t block_t;

#define FS_NODE_ID_INVALID ((nodeid_t) 0)
#define FS_NODE_ID_JOURNAL ((nodeid_t) 1)
#define FS_NODE_ID_ROOT    ((nodeid_t) 2)

typedef struct __attribute__((packed))
{
    char     Header[FS_CONFIG_HEADER_SIZE];
    uint16_t BytesPerBlock;
    uint64_t FileSystemOffset;
} FsConfigChunk;

typedef struct __attribute__((packed))
{
    char      Header[FS_HEADER_SIZE];
    uint32_t  Flags;
    uint16_t  FsMajor;
    uint16_t  Revision; // Must be set back to FS_INITIAL_REVISION per each FsMajor increment.
    char      VendorID[FS_VENDOR_ID_SIZE];
    uint16_t  BlockSize;
    uint64_t  Size;
    uint32_t  NodeCapacity;
    block_t   Origin;
    uint64_t  NumAllocatedBlocks;
    uint32_t  NumAllocatedNodes;
    char      VolumeName[FS_VOLUME_NAME_SIZE];
    uint8_t   CreatorID;
    biotime_t TsCreated;
    biotime_t TsMounted;
    char      UniqueID[FS_UNIQUE_ID_SIZE];
    uint8_t   ErrorState;
    uint8_t   ErrorAction;
    block_t   AddrBitmap;
    block_t   AddrNodeTable;
    block_t   AddrData;
    block_t   AddrExtension; // Currently no fs size extension support, reserved for future.
    nodeid_t  LastAllocatedNodeID;
    block_t   LastAllocatedDataBlock;
    uint32_t  Tail;
    uint32_t  Checksum; // Checksum of all member variables before itself, uses CRC32.
} FsMeta;

#define FS_NODE_TYPE_FILE      UINT16_C(1)
#define FS_NODE_TYPE_DIRECTORY UINT16_C(2)
#define FS_NODE_TYPE_SOFT_LINK UINT16_C(3)
#define FS_NODE_TYPE_HARD_LINK UINT16_C(4) // Not yet implemented.

#define FS_NODE_FLAG_CLEAR     UINT32_C(0)
#define FS_NODE_FLAG_SYSTEM    UINT32_C(1)
#define FS_NODE_FLAG_READ_ONLY UINT32_C(1 << 1)
#define FS_NODE_FLAG_HIDDEN    UINT32_C(1 << 2)

#define FS_NODE_INLINE_DATA_SIZE   64
#define FS_NODE_DIRECT_DATA_BLOCKS 12

#define FS_NODE_SIZE               256
typedef struct __attribute__((packed))
{
    nodeid_t  ID;
    uint16_t  Type;
    uint32_t  Flags;
    uint64_t  Size;
    uint8_t   CreatorID;
    biotime_t TsCreated;
    biotime_t TsAccessed;
    biotime_t TsModified;
    int32_t   Owner; // 0xffffffff = no owner and/or was created externally,
                     // 0 = belongs to system directly,
                     // 1 = highest privileged system user,
                     // any positive value = user id,
                     // any negative value = group id when turned positive.
    uint32_t  HardLinkCount; // Hard links aren't implemented as of now. For future usage.
    uint8_t   InlineData[FS_NODE_INLINE_DATA_SIZE];
    block_t   DirectData[FS_NODE_DIRECT_DATA_BLOCKS];
    block_t   AddrSinglyIndirect;
    block_t   AddrDoublyIndirect;
    block_t   AddrTriplyIndirect;

    char      Padding[21];
} FsNode;

typedef struct __attribute__((packed))
{
    nodeid_t NodeID;     // ID of the node pointed to by this entry.
    uint16_t NodeType;   // The type of the node pointed to by NodeID. When NodeID is resolved to a FsNode structure, FsNode.Type field
                         // exactly matches with this field. This is here for faster type lookup, avoiding hitting the FsNode on disk.
    uint16_t EntrySize;  // Size of this entry. Must be a multiple of 4.
    uint8_t  NameLength; // Length of the entry's name.
    // FROM offsetof(FsEntry, NameLength) TO offsetof(FsEntry, NameLength) + NameLength = ENTRY_NAME.
} FsEntry;

const char* FsCreatorIDToString(uint8_t ID);
const char* FsErrorStateToString(uint8_t state);
const char* FsErrorActionToString(uint8_t action);
const char* FsNodeTypeToString(uint16_t type);
const char* FsOwnerToString(int32_t owner);

#endif // !MYTH_FILE_SYSTEM_H
