#include "FileSystem.h"

const char* FsCreatorIDToString(uint8_t ID)
{
    switch (ID)
    {
    case FS_CREATOR_WILDCARD:   return "Wildcard (Generic Creator)";
    case FS_CREATOR_MYTH_TOOL:  return "Myth File System Tool";
    case FS_CREATOR_BIO_OS:     return "BIO Operating System";
    default: break;
    }

    return "Unknown (Non-Standard Creator)";
}

const char* FsErrorStateToString(uint8_t state)
{
    switch (state)
    {
    case FS_ERROR_STATE_PRENORMAL: return "Prenormal";
    case FS_ERROR_STATE_NORMAL:    return "Normal";
    case FS_ERROR_STATE_ABNORMAL:  return "Abnormal";
    default: break;
    }

    return "((Invalid, Non-Standard Error State))";
}

const char* FsErrorActionToString(uint8_t action)
{
    switch (action)
    {
    case FS_ERROR_ACTION_NONE:      return "Do Nothing";
    case FS_ERROR_ACTION_READ_ONLY: return "Mount File System as Read Only";
    case FS_ERROR_ACTION_BIOIZATE:  return "Bioizate (aka Kernel Panic)";
    default: break;
    }

    return "((Invalid, Non-Standard Error Action))";
}

const char* FsNodeTypeToString(uint16_t type)
{
    switch (type)
    {
    case FS_NODE_TYPE_FILE:      return "File";
    case FS_NODE_TYPE_DIRECTORY: return "Directory";
    case FS_NODE_TYPE_SOFT_LINK: return "Soft Link";
    case FS_NODE_TYPE_HARD_LINK: return "Hard Link";
    default: break;    
    }

    return "((Invalid, Non-Standard Node Type))";
}

const char* FsOwnerToString(int32_t owner)
{
    if (owner == 0xffffffff)
    {
        return "Disowned";
    }
    if (owner == 1)
    {
        return "Highest Privilege User";
    }

    if (owner < 0)
    {
        return "Unknown Group";
    }
    if (owner > 0)
    {
        return "Unknown User";
    }

    // owner == 0
    return "System"; // Owner 0 is considered the "System" itself, belongs to the OS and no user.
}
