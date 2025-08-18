#include "FileSystem.h"
#include "Bitmap.h"
#include "Disk.h"
#include "Node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ACTION_MAKE_FILE_SYSTEM "MakeFS"
#define ACTION_READ_FILE_SYSTEM "ReadFS"
#define ACTION_READ_NODE        "ReadNode"
#define ACTION_CREATE_ON_ROOT   "CreateOnRoot"

int CliMakeFileSystem(int argc, char** argv);
int CliReadFileSystem(int argc, char** argv);
int CliReadNode(int argc, char** argv);
int CliCreateOnRoot(int argc, char** argv);

int main(int argc, char** argv)
{
    if (sizeof(FsNode) != FS_NODE_SIZE)
    {
        puts("Internal program configuration error, size of FsNode does not match size defined by the macro. Fix the code if you can or report the issue.");
        return 1;
    }

    if (argc <= 1)
    {
        puts("Nothing to do.");
        return 0;
    }

    // Seed the random generator
    srand(time(NULL));
    
    char** localizedActionArgV = argv + 2;
    int    localizedActionArgC = argc - 2;
    
    char* action = argv[1];
#define CHECKCASE(a, fn) if (strcmp(action, a) == 0) { return fn(localizedActionArgC, localizedActionArgV); }
    CHECKCASE(ACTION_MAKE_FILE_SYSTEM, CliMakeFileSystem);
    CHECKCASE(ACTION_READ_FILE_SYSTEM, CliReadFileSystem);
    CHECKCASE(ACTION_READ_NODE       , CliReadNode);
    CHECKCASE(ACTION_CREATE_ON_ROOT  , CliCreateOnRoot);
#undef CHECKCASE
    
    printf("Unrecognized action '%s'.\n", action);
    return 1;
}

int CliMakeFileSystem(int argc, char** argv)
{
    printf(ACTION_MAKE_FILE_SYSTEM " usage: "
            "[DiskPath: str] [BlockSize: int] [FileSystemOffset (in blocks): int] [VolumeName (max size " FS_STRINGIZE(FS_VOLUME_NAME_SIZE) "): str] "
            "[BytesPerNodeRatio (default 16384, 16KiB)]: int\n");

    if (argc < 4)
    {
        puts("Too few arguments.");
        return 1;
    }
    if (argc > 5)
    {
        puts("Too many arguments.");
        return 1;
    }

    char* diskPath  =      argv[0];
    int   blockSize = atoi(argv[1]);
    int   fsOffset  = atoi(argv[2]);
    char* volName   =      argv[3];

    if (blockSize < 0 || blockSize > 0xffff)
    {
        printf("MakeFS fail, BlockSize must be between 0 and 0xffff but the provided value was %d.\n", blockSize);
        return 1;
    }

    int bytesPerNodeRatio = 16384;
    if (argc == 5)
    {
        bytesPerNodeRatio = atoi(argv[4]);
    }

    {
        size_t volNameLen = strlen(volName);
        if (volNameLen > FS_VOLUME_NAME_SIZE)
        {
            printf("MakeFS failed, the provided VolumeName has a length of %zu, but Myth accepts a maximum of " FS_STRINGIZE(FS_VOLUME_NAME_SIZE) ".", volNameLen);
            return 1;
        }
    }

    FILE* pDisk = fopen(diskPath, "r+b");
    if (!pDisk)
    {
        printf("MakeFS fail, couldn't open disk from path '%s'.\n", diskPath);
        return 1;
    }
    
    fseek(pDisk, 0, SEEK_END);
    long rawDiskSize = ftell(pDisk);
    long numBlocks = rawDiskSize / blockSize;

    FsMeta meta;
    memset(&meta, 0, sizeof(FsMeta));
    
    strncpy(meta.VendorID,   "MythFsTool", sizeof("MythFsTool")-1);
    strncpy(meta.VolumeName, volName,     FS_VOLUME_NAME_SIZE);
    
    meta.FsMajor   = FS_LATEST_MAJOR;
    meta.Revision  = FS_LATEST_REVISION;
    meta.BlockSize = (uint16_t) blockSize;
    meta.Size      = (uint64_t) numBlocks;
    meta.Origin    = fsOffset;
    meta.Flags     = 0;

    makefs_status_t makeStatus = FsMakeFileSystem(pDisk, &meta, bytesPerNodeRatio);
    if (makeStatus != FS_MAKE_FILE_SYSTEM_SUCCESSFUL)
    {
        printf("MakeFs failed, FsMakeFileSystem returned code %u (%s).\n", makeStatus, FsMakeFsStatusToString(makeStatus));
        fclose(pDisk);
        return 1;
    }
    
    // Create root node. per Myth Standard definition, it is resolved by "FS/" at the beginning of a PATH.
    puts("File System was made successfully, trying to create root node...");
    FsNode node;
    memset(&node, 0, FS_NODE_SIZE);

    node.ID    = FS_NODE_ID_ROOT;
    node.Type  = FS_NODE_TYPE_DIRECTORY;
    node.Flags = FS_NODE_FLAG_SYSTEM;
    node.CreatorID = FS_CREATOR_MYTH_TOOL;
    node.Owner = 0xffffffff;

    // By default, empty directories have no data, and our root directory has no entries as of now so leave data NULL.
    FsMakeNode(pDisk, &meta, &node, NULL, 0);

    puts("MakeFS succeeded, the file system was made successfully.");
    fclose(pDisk);

    return 0;
}

int CliReadFileSystem(int argc, char** argv)
{
    puts(ACTION_READ_FILE_SYSTEM " usage: [DiskPath: str]");

    if (argc < 1)
    {
        puts("Too few arguments.");
        return 1;
    }
    if (argc > 1)
    {
        puts("Too many arguments.");
        return 1;
    }

    char* pDiskPath = argv[0];
    FileSystemOnDisk fsOnDisk = FsLoadFileSystemOnDisk(pDiskPath);
    if (!fsOnDisk.bLoaded)
    {
        puts(ACTION_READ_NODE " failed, FsLoadFileSystemOnDisk failed.");
        return 1;
    }

    printf( "File System Metadata Information:\n"
            " Header: %." FS_STRINGIZE(FS_HEADER_SIZE) "s\n"
            " UniqueID: %." FS_STRINGIZE(FS_UNIQUE_ID_SIZE) "s\n"
            " Flags: %x (decimal %u)\n"
            " FsMajor: %u\n"
            " Revision: %u\n"
            " VendorID: %." FS_STRINGIZE(FS_VENDOR_ID_SIZE) "s\n"
            " BlockSize: %u\n"
            " Size: %lu\n"
            " NodeCapacity: %u\n"
            " Origin: %lu\n"
            " NumAllocatedBlocks: %lu\n"
            " NumAllocatedNodes: %u\n"
            " VolumeName: %." FS_STRINGIZE(FS_VOLUME_NAME_SIZE) "s\n"
            " CreatorID: %u (%s)\n"
            " TsCreated: %lu\n"
            " TsMounted: %lu\n"
            " ErrorState: %u (%s)\n"
            " ErrorAction: %u (%s)\n"
            " AddrBitmap: %lu\n"
            " AddrNodeTable: %lu\n"
            " AddrData: %lu\n"
            " AddrExtension: %lu\n"
            " LastAllocatedNodeID: %u\n"
            " LastAllocatedDataBlock: %lu\n"
            " Tail: %x\n"
            " Checksum: %x (decimal %u)\n",

        fsOnDisk.Meta.Header, fsOnDisk.Meta.UniqueID, fsOnDisk.Meta.Flags, fsOnDisk.Meta.Flags, fsOnDisk.Meta.FsMajor, fsOnDisk.Meta.Revision, fsOnDisk.Meta.VendorID, fsOnDisk.Meta.BlockSize, fsOnDisk.Meta.Size, fsOnDisk.Meta.NodeCapacity, fsOnDisk.Meta.Origin,
        fsOnDisk.Meta.NumAllocatedBlocks, fsOnDisk.Meta.NumAllocatedNodes, fsOnDisk.Meta.VolumeName, fsOnDisk.Meta.CreatorID, FsCreatorIDToString(fsOnDisk.Meta.CreatorID), fsOnDisk.Meta.TsCreated,
        fsOnDisk.Meta.TsMounted, fsOnDisk.Meta.ErrorState, FsErrorStateToString(fsOnDisk.Meta.ErrorState), fsOnDisk.Meta.ErrorAction, FsErrorActionToString(fsOnDisk.Meta.ErrorAction),
        fsOnDisk.Meta.AddrBitmap, fsOnDisk.Meta.AddrNodeTable, fsOnDisk.Meta.AddrData, fsOnDisk.Meta.AddrExtension, fsOnDisk.Meta.LastAllocatedNodeID, fsOnDisk.Meta.LastAllocatedDataBlock,
        fsOnDisk.Meta.Tail, fsOnDisk.Meta.Checksum, fsOnDisk.Meta.Checksum
    );

    puts("ReadFS succeeded, the file system was read successfully.");
    FsCloseDisk(fsOnDisk);

    return 0;
}

int CliReadNode(int argc, char** argv)
{
    puts(ACTION_READ_NODE " usage: [DiskPath: str] [NodeID: int]");

    if (argc < 2)
    {
        puts("Too few arguments.");
        return 1;
    }
    if (argc > 2)
    {
        puts("Too many arguments.");
        return 1;
    }

    char* pDiskPath = argv[0];
    nodeid_t nodeID = (nodeid_t) atoi(argv[1]);

    FileSystemOnDisk fsOnDisk = FsLoadFileSystemOnDisk(pDiskPath);
    if (!fsOnDisk.bLoaded)
    {
        puts(ACTION_READ_NODE " failed, FsLoadFileSystemOnDisk failed.");
        return 1;
    }

    FsNode node = FsGetNode(fsOnDisk.pDisk, &fsOnDisk.Meta, nodeID);
    if (node.ID == FS_NODE_ID_INVALID)
    {
        printf(ACTION_READ_NODE " failed, node %u doesn't exist.\n", nodeID);
        FsCloseDisk(fsOnDisk);
        return 1;
    }

    printf( "File System Node Information:\n"
            " ID: %u (%s)\n"
            " Type: %u (%s)\n"
            " Flags: 0x%x (decimal %u)\n"
            " Size: %lu\n"
            " CreatorID: %u (%s)\n"
            " TsCreated: %lu\n"
            " TsAccessed: %lu\n"
            " TsModified: %lu\n"
            " Owner: %d ('%s')\n"
            " HardLinkCount: %u\n"
            " InlineData: ((Not Interface Presentable.))\n"
            " DirectData[0]: %lu\n"
            " DirectData[1]: %lu\n"
            " DirectData[2]: %lu\n"
            " DirectData[3]: %lu\n"
            " DirectData[4]: %lu\n"
            " DirectData[5]: %lu\n"
            " DirectData[6]: %lu\n"
            " DirectData[7]: %lu\n"
            " DirectData[8]: %lu\n"
            " DirectData[9]: %lu\n"
            " DirectData[10]: %lu\n"
            " DirectData[11]: %lu\n"
            " AddrSinglyIndirect: %lu\n"
            " AddrDoublyIndirect: %lu\n"
            " AddrTriplyIndirect: %lu\n",
        node.ID, node.ID == 1 ? "JR/" : node.ID == 2 ? "FS/" : "Standard File System Node",
        node.Type, FsNodeTypeToString(node.Type), node.Flags, node.Flags, node.Size, node.CreatorID,
        FsCreatorIDToString(node.CreatorID), node.TsCreated, node.TsAccessed, node.TsModified, node.Owner, FsOwnerToString(node.Owner),
        node.HardLinkCount,
        node.DirectData[0], node.DirectData[1], node.DirectData[2], node.DirectData[3], node.DirectData[4], node.DirectData[5],
        node.DirectData[6], node.DirectData[7], node.DirectData[8], node.DirectData[9], node.DirectData[10], node.DirectData[11],
        node.AddrSinglyIndirect, node.AddrDoublyIndirect, node.AddrTriplyIndirect
    );

    FsCloseDisk(fsOnDisk);
    puts(ACTION_READ_NODE " succeeded, node was read successfully");

    return 0;
}

int CliCreateOnRoot(int argc, char** argv)
{
    puts(ACTION_CREATE_ON_ROOT " usage: [DiskPath: str] [SourceFilePath: str] [IsSystemFile: bool]");

    if (argc < 3)
    {
        puts("Too few arguments.");
        return 1;
    }
    if (argc > 3)
    {
        puts("Too many arguments.");
        return 1;
    }

    char* pDiskPath       = argv[0];
    char* pSourceFilePath = argv[1];
    int   bIsSystemFile   = atoi(argv[2]);

    FileSystemOnDisk fsOnDisk = FsLoadFileSystemOnDisk(pDiskPath);
    if (!fsOnDisk.bLoaded)
    {
        puts(ACTION_CREATE_ON_ROOT " failed, FsLoadFileSystemOnDisk failed.");
        return 1;
    }

    FILE* pSourceFile = fopen(pSourceFilePath, "rb");
    if (!pSourceFile)
    {
        printf(ACTION_CREATE_ON_ROOT " failed, couldn't open source file %s.\n", pSourceFilePath);
        FsCloseDisk(fsOnDisk);
        return 1;
    }

    fseek(pSourceFile, 0, SEEK_END);
    long szSrcFile = ftell(pSourceFile);
    fseek(pSourceFile, 0, SEEK_SET);

    char* pFileData = malloc(szSrcFile);
    fread(pFileData, 1, szSrcFile, pSourceFile);
    fclose(pSourceFile);
    
    FsNode node;
    node.ID    = FsFindNodeID(fsOnDisk.pDisk, &fsOnDisk.Meta);
    node.Type  = FS_NODE_TYPE_FILE;
    node.Flags = bIsSystemFile ? FS_NODE_FLAG_SYSTEM : FS_NODE_FLAG_CLEAR;

    create_node_result_t createResult = FsMakeNode(fsOnDisk.pDisk, &fsOnDisk.Meta, &node, pFileData, szSrcFile);
    if (createResult != FS_MAKE_NODE_SUCCESSFUL)
    {
        printf(ACTION_CREATE_ON_ROOT " failed, FsMakeNode failed with code %u (%s).", createResult, FsCreateNodeResultToString(createResult));
        FsCloseDisk(fsOnDisk);
        fclose(pSourceFile);
        return 0;
    }

    FsCloseDisk(fsOnDisk);
    printf(ACTION_CREATE_ON_ROOT " succeeded, file was made successfully, node ID = %u.\n", node.ID);

    return 0;
}
