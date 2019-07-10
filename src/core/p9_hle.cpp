#include <sstream>
#include <iomanip>
#include "p9_hle.hpp"

using namespace std;

#define GET_MOD_SIZE(module) sizeof(module) / sizeof(P9_Cmd)

namespace P9_HLE
{

struct P9_Cmd
{
    const char* name;
    uint32_t header;
};

P9_Cmd mc_cmds[] =
{

};

P9_Cmd fs_cmds[] =
{
    {"OpenFile", 0x000101C2},
    {"DeleteFile", 0x00020142},
    {"RenameFile", 0x00030244},
    {"DeleteDirectory", 0x00040142},
    {"CreateFile", 0x00050202},
    {"CreateDirectory", 0x00060182},
    {"RenameDirectory", 0x00070244},
    {"OpenDirectory", 0x00080102},
    {"ReadFile", 0x00090142},
    {"CalculateFileHashSHA256", 0x000A00C2},
    {"WriteFile", 0x000B0182},
    {"CalcSavegameMAC", 0x000C0104},
    {"GetFileSize", 0x000D0080},
    {"SetFileSize", 0x000E0100},
    {"CloseFile", 0x000F0080},
    {"ReadDirectory", 0x001000C2},
    {"CloseDirectory", 0x00110080},
    {"OpenArchive", 0x001200C2},
    {"0x00130102", 0x00130102},
    {"0x00140102", 0x00140102},
    {"CommitSaveData", 0x001500C0},
    {"CloseArchive", 0x00160080},
    {"0x00170080", 0x00170080},
    {"GetCardType", 0x00180000},
    {"GetSdmcArchiveResource", 0x00190000},
    {"GetNandArchiveResource", 0x001A0000},
    {"GetSdmcFatFsError", 0x001B0000},
    {"IsSdmcDetected", 0x001C0000},
    {"IsSdmcWritable", 0x001D0000},
    {"GetSdmcCid", 0x001E0042},
    {"GetNandCid", 0x001F0042},
    {"GetSdmcSpeedInfo", 0x00200000},
    {"GetNandSpeedInfo", 0x00210000},
    {"GetSdmcLog", 0x00220042},
    {"GetNandLog", 0x00230042},
    {"ClearSdmcLog", 0x00240000},
    {"ClearNandLog", 0x00250000},
    {"CardSlotIsInserted", 0x00260000},
    {"CardSlotPowerOn", 0x00270000},
    {"CardSlotPowerOff", 0x00280000},
    {"CardSlotGetIFPowerStatus", 0x00290000},
    {"CardNorDirectCommand", 0x002A0040},
    {"CardNorDirectCommandWithAddress", 0x002B0080},
    {"CardNorDirectRead", 0x002C0082},
    {"CardNorDirectReadWithAddress", 0x002D00C2},
    {"CardNorDirectWrite", 0x002E0082},
    {"CardNorDirectWriteWithAddress", 0x002F00C2},
    {"CardNorDirectRead_4xIO", 0x003000C2},
    {"CardNorDirectCpuWriteWithoutVerify", 0x00310082},
    {"CardNorDirectSectorEraseWithoutVerify", 0x00320040},
    {"GetProductInfo", 0x00330080},
    {"SetCardSpiBaudrate", 0x00340040},
    {"SetCardSpiBusMode", 0x00350040},
    {"SendInitializeInfoTo9", 0x00360040},
    {"CreateExtSaveData", 0x00370100},
    {"DeleteExtSaveData", 0x00380100},
    {"EnumerateExtSaveData", 0x00390102},
    {"GetSpecialContentIndex", 0x003A0100},
    {"GetLegacyRomHeader", 0x003B00C2},
    {"GetLegacyBannerData", 0x003C0102},
    {"0x003D0040", 0x003D0040},
    {"DeleteSdmcRoot", 0x003E0000},
    {"DeleteAllExtSaveDataOnNand", 0x003F0040},
    {"InitializeCtrFilesystem", 0x00400000},
    {"CreateSeed", 0x00410000},
    {"GetSdmcCtrRootPath", 0x00420042},
    {"GetArchiveResource", 0x00430040},
    {"ExportIntegrityVerificationSeed", 0x00440002},
    {"ImportIntegrityVerificationSeed", 0x00450002},
    {"GetLegacySubBannerData", 0x00460102},
    {"0x00470042", 0x00470042},
    {"GetFileLastModified", 0x004800C2},
    {"ReadSpecialFile", 0x00490102},
    {"GetSpecialFileSize", 0x004A0040},
    {"StartDeviceMoveAsSource", 0x004B0000},
    {"StartDeviceMoveAsDestination", 0x004C0240},
    {"ReadFileSHA256", 0x004D01C4},
    {"WriteFileSHA256", 0x004E0204},
    {"0x004F0080", 0x004F0080},
    {"SetPriority", 0x00500040},
    {"SwitchCleanupInvalidSaveData", 0x00510040},
    {"EnumerateSystemSaveData", 0x00520042},
    {"0x00530000", 0x00530000},
    {"0x00540000", 0x00540000},
    {"ReadNandReport", 0x00550082},
    {"0x00560102", 0x00560102}
};

P9_Cmd pm_cmds[] =
{
    {"GetProgramInfo", 0x00010082},
    {"RegisterProgram", 0x00020200},
    {"UnregisterProgram", 0x00030080}
};

P9_Cmd dev_cmds[] =
{

};

P9_Cmd am_cmds[] =
{

};

P9_Cmd ps_cmds[] =
{

};

bool is_function_registered(P9_Cmd* module, int module_size, uint32_t command)
{
    int id = command >> 16;
    if (id < 1)
        return false;
    if (id > module_size - 1)
        return false;
    return true;
}

string get_function_name(uint32_t *buffer, int len)
{
    const static char* module_names[] = {
        "PXIMC", "PXIFS", "PXIFS", "PXIFS", "PXIFS", "PXIPM", "PXIDEV", "PXIAM", "PXIPS"};
    stringstream output;

    uint32_t module_id = buffer[0];
    string name;

    P9_Cmd* module;
    int module_size;

    switch (module_id)
    {
        case 0:
            module = mc_cmds;
            module_size = GET_MOD_SIZE(mc_cmds);
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            module = fs_cmds;
            module_size = GET_MOD_SIZE(fs_cmds);
            break;
        case 5:
            module = pm_cmds;
            module_size = GET_MOD_SIZE(pm_cmds);
            break;
        case 6:
            module = dev_cmds;
            module_size = GET_MOD_SIZE(dev_cmds);
            break;
        case 7:
            module = am_cmds;
            module_size = GET_MOD_SIZE(am_cmds);
            break;
        case 8:
            module = ps_cmds;
            module_size = GET_MOD_SIZE(ps_cmds);
            break;
        default:
            output << "P9 ERROR: Unrecognized module " << module_id;
            return output.str();
    }

    output << module_names[module_id] << ":";

    uint32_t command_header = buffer[1];

    if (is_function_registered(module, module_size, command_header))
        output << module[(command_header >> 16) - 1].name;
    else
        output << "0x" << setfill('0') << setw(8) << uppercase << hex << command_header;

    output << "(";

    for (int i = 2; i < len; i++)
    {
        output << "0x" << setfill('0') << setw(8) << uppercase << hex << buffer[i];
        if (i < len - 1)
            output << ",";
    }

    output << ")";

    return output.str();
}

};
