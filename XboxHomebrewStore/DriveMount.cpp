#include "DriveMount.h"
#include "External.h"
#include "String.h"
#include "InputManager.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>

enum DriveKind
{
    DriveKindCdRom,
    DriveKindHdd,
    DriveKindMemoryUnit
};

struct DriveEntry
{
    std::string name;
    DriveKind kind;
    std::string devicePath;
    bool mounted;
};

typedef std::map<std::string, DriveEntry> DriveMap;

static DriveMap& GetDrives()
{
    static DriveMap s_drives;
    if (s_drives.empty())
    {
        static const DriveEntry s_table[] =
        {
            { "DVD-ROM",     DriveKindCdRom, "\\Device\\Cdrom0", false },
            { "HDD0-C",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition2", false },
            { "HDD0-E",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition1", false },
            { "HDD0-F",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition6", false },
            { "HDD0-G",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition7", false },
            { "HDD0-H",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition8", false },
            { "HDD0-I",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition9", false },
            { "HDD0-J",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition10", false },
            { "HDD0-K",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition11", false },
            { "HDD0-L",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition12", false },
            { "HDD0-M",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition13", false },
            { "HDD0-N",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition14", false },
            { "HDD0-X",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition3", false },
            { "HDD0-Y",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition4", false },
            { "HDD0-Z",      DriveKindHdd,   "\\Device\\Harddisk0\\Partition5", false },
            { "HDD1-C",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition2", false },
            { "HDD1-E",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition1", false },
            { "HDD1-F",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition6", false },
            { "HDD1-G",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition7", false },
            { "HDD1-H",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition8", false },
            { "HDD1-I",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition9", false },
            { "HDD1-J",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition10", false },
            { "HDD1-K",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition11", false },
            { "HDD1-L",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition12", false },
            { "HDD1-M",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition13", false },
            { "HDD1-N",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition14", false },
            { "HDD1-X",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition3", false },
            { "HDD1-Y",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition4", false },
            { "HDD1-Z",      DriveKindHdd,   "\\Device\\Harddisk1\\Partition5", false },
            { "MMU0",        DriveKindMemoryUnit, "H", false },
            { "MMU1",        DriveKindMemoryUnit, "I", false },
            { "MMU2",        DriveKindMemoryUnit, "J", false },
            { "MMU3",        DriveKindMemoryUnit, "K", false },
            { "MMU4",        DriveKindMemoryUnit, "L", false },
            { "MMU5",        DriveKindMemoryUnit, "M", false },
            { "MMU6",        DriveKindMemoryUnit, "N", false },
            { "MMU7",        DriveKindMemoryUnit, "O", false },
        };
        for (size_t i = 0; i < sizeof(s_table) / sizeof(s_table[0]); i++)
        {
            s_drives[s_table[i].name] = s_table[i];
        }
    }
    return s_drives;
}

static std::string NormalizeDriveName(const std::string& driveName)
{
    std::string key = String::ToUpper(driveName);
    if (key.length() == 1 && key[0] >= 'H' && key[0] <= 'O')
    {
        return String::Format("MMU%d", key[0] - 'H');
    }
    return key;
}

static DriveEntry* FindDrive(const std::string& driveName)
{
    if (driveName.empty())
    {
        return NULL;
    }
    DriveMap& drives = GetDrives();
    std::string key = NormalizeDriveName(driveName);
    DriveMap::iterator it = drives.find(key);
    if (it == drives.end())
    {
        return NULL;
    }
    return &it->second;
}

static bool DoUnmount(DriveEntry* ent)
{
    if (ent->kind == DriveKindMemoryUnit)
    {
        return true;
    }

    std::string symlink = String::Format("\\??\\%s:", ent->name.c_str());
    if (symlink.length() == 0 || symlink.length() >= 64)
    {
        return false;
    }

    size_t deviceLen = ent->devicePath.length();
    if (ent->kind == DriveKindCdRom && deviceLen > 0 && ent->devicePath[deviceLen - 1] == '\\')
    {
        deviceLen--;
    }

    STRING sSymlink = { (USHORT)symlink.length(), (USHORT)symlink.length() + 1, (PSTR)symlink.c_str() };
    STRING sDevice  = { (USHORT)deviceLen, (USHORT)deviceLen + 1, (PSTR)ent->devicePath.c_str() };

    LONG r = IoDeleteSymbolicLink(&sSymlink);
    if (ent->kind == DriveKindCdRom)
    {
        r |= IoDismountVolumeByName(&sDevice);
    }
    if (r == STATUS_SUCCESS && ent->kind == DriveKindHdd)
    {
        ent->mounted = false;
    }
    return (r == STATUS_SUCCESS);
}

static bool DoMount(DriveEntry* ent)
{
    if (ent->kind == DriveKindMemoryUnit)
    {
        if (ent->devicePath.empty())
        {
            return false;
        }
        return InputManager::IsMemoryUnitMounted(ent->devicePath[0]);
    }

    if (ent->kind == DriveKindCdRom)
    {
        DoUnmount(ent);
    }

    std::string symlink = String::Format("\\??\\%s:", ent->name.c_str());
    if (symlink.length() == 0 || symlink.length() >= 64)
    {
        return false;
    }

    const bool alreadyMountedHdd = (ent->kind == DriveKindHdd && ent->mounted);

    if (!alreadyMountedHdd)
    {
        size_t deviceLen = ent->devicePath.length();
        STRING sSymlink = { (USHORT)symlink.length(), (USHORT)symlink.length() + 1, (PSTR)symlink.c_str() };
        STRING sDevice  = { (USHORT)deviceLen, (USHORT)deviceLen + 1, (PSTR)ent->devicePath.c_str() };
        if (IoCreateSymbolicLink(&sSymlink, &sDevice) != STATUS_SUCCESS)
        {
            return false;
        }
        if (ent->kind == DriveKindHdd)
        {
            ent->mounted = true;
        }
    }

    if (ent->kind == DriveKindCdRom)
    {
        unsigned long trayState = 0;
        if (HalReadSMCTrayState(&trayState, NULL) == STATUS_SUCCESS && trayState == SMC_TRAY_STATE_MEDIA_DETECT)
        {
            return true;
        }
        return true;
    }

    std::string path = String::Format("%s:\\", ent->name.c_str());
    ULARGE_INTEGER totalBytes;
    totalBytes.QuadPart = 0;
    return GetDiskFreeSpaceExA(path.c_str(), NULL, &totalBytes, NULL) ? true : false;
}

bool DriveMount::Mount(std::string driveName)
{
    DriveEntry* ent = FindDrive(driveName);
    if (ent == NULL)
    {
        return false;
    }
    return DoMount(ent);
}

bool DriveMount::Unmount(std::string driveName)
{
    DriveEntry* ent = FindDrive(driveName);
    if (ent == NULL)
    {
        return false;
    }
    return DoUnmount(ent);
}
