#pragma once

#include "Integers.h"
#include <xtl.h>

#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
#define FILE_NON_DIRECTORY_FILE 0x00000040
#define OBJ_CASE_INSENSITIVE 0x00000040L
#define SMC_TRAY_STATE_MEDIA_DETECT 1
#define STATUS_SUCCESS 0

#define SMBDEV_PIC16L 0x20
#define PIC16L_CMD_POWER 0x02
#define POWER_SUBCMD_RESET 0x01
#define POWER_SUBCMD_CYCLE 0x40
#define POWER_SUBCMD_POWER_OFF 0x80

typedef LONG NTSTATUS;

typedef struct STRING {
	WORD Length;
	WORD MaximumLength;
	PSTR Buffer;
} STRING;

typedef struct IO_STATUS_BLOCK
{
	NTSTATUS Status;
	ULONG Information;
} IO_STATUS_BLOCK;

typedef struct OBJECT_ATTRIBUTES
{
	HANDLE RootDirectory;
	STRING* ObjectName;
	ULONG Attributes;
} OBJECT_ATTRIBUTES;

typedef struct FILE_NETWORK_OPEN_INFORMATION {
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER AllocationSize;
	LARGE_INTEGER EndOfFile;
	ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION;

typedef enum FILE_INFORMATION_CLASS
{
	FileDirectoryInformation = 1,
	FileFullDirectoryInformation,
	FileBothDirectoryInformation,
	FileBasicInformation,
	FileStandardInformation,
	FileInternalInformation,
	FileEaInformation,
	FileAccessInformation,
	FileNameInformation,
	FileRenameInformation,
	FileLinkInformation,
	FileNamesInformation,
	FileDispositionInformation,
	FilePositionInformation,
	FileFullEaInformation,
	FileModeInformation,
	FileAlignmentInformation,
	FileAllInformation,
	FileAllocationInformation,
	FileEndOfFileInformation,
	FileAlternateNameInformation,
	FileStreamInformation,
	FilePipeInformation,
	FilePipeLocalInformation,
	FilePipeRemoteInformation,
	FileMailslotQueryInformation,
	FileMailslotSetInformation,
	FileCompressionInformation,
	FileCopyOnWriteInformation,
	FileCompletionInformation,
	FileMoveClusterInformation,
	FileQuotaInformation,
	FileReparsePointInformation,
	FileNetworkOpenInformation,
	FileObjectIdInformation,
	FileTrackingInformation,
	FileOleDirectoryInformation,
	FileContentIndexInformation,
	FileInheritContentIndexInformation,
	FileOleInformation,
	FileMaximumInformation
} FILE_INFORMATION_CLASS;

typedef struct DRIVER_OBJECT {
    const int16_t Type;
    const int16_t Size;
    struct DEVICE_OBJECT* DeviceObject;
} DRIVER_OBJECT;

typedef struct DEVICE_OBJECT {
    const int16_t Type;
    const uint16_t Size;
    int32_t ReferenceCount;
    DRIVER_OBJECT* DriverObject;
} DEVICE_OBJECT;

extern "C" {

    NTSTATUS WINAPI NtClose(HANDLE Handle);
	NTSTATUS WINAPI NtOpenFile(
		OUT PHANDLE pHandle,
		IN ACCESS_MASK DesiredAccess,
		IN OBJECT_ATTRIBUTES* ObjectAttributes,
		OUT IO_STATUS_BLOCK* IOStatusBlock,
		IN ULONG ShareAccess,
		IN ULONG OpenOptions
	);

    NTSTATUS WINAPI NtQueryInformationFile(
		HANDLE FileHandle,
		IO_STATUS_BLOCK* IoStatusBlock,
		PVOID FileInformation,
		ULONG FileInformationLength,
		FILE_INFORMATION_CLASS FileInformationClass
	);
    NTSTATUS WINAPI NtReadFile(
		HANDLE Handle,
		HANDLE Event,
		PVOID pApcRoutine,
		PVOID pApcContext,
		PVOID pIoStatusBlock,
		PVOID pBuffer,
		ULONG Length,
		PLARGE_INTEGER pByteOffset
	);

    NTSTATUS WINAPI NtSetSystemTime(PLARGE_INTEGER SystemTime, PLARGE_INTEGER PreviousTime);

    extern STRING* XeImageFileName;
    VOID WINAPI HalReturnToFirmware(ULONG value);
    NTSTATUS WINAPI HalWriteSMBusValue(UCHAR devddress, UCHAR offset, UCHAR writedw, DWORD data);
    NTSTATUS WINAPI HalReadSMBusValue(UCHAR devddress, UCHAR offset, UCHAR readdw, DWORD* pdata);
    NTSTATUS WINAPI HalReadSMCTrayState(ULONG* TrayState, ULONG* EjectCount);

    LONG WINAPI IoCreateSymbolicLink(STRING*, STRING*);
    LONG WINAPI IoDeleteSymbolicLink(STRING*);
    LONG WINAPI IoDismountVolumeByName(STRING*);
    LONG WINAPI IoDismountVolume(DEVICE_OBJECT*);

    NTSTATUS WINAPI MU_CreateDeviceObject(uint32_t port, uint32_t slot, STRING* deviceName);
    VOID WINAPI MU_CloseDeviceObject(uint32_t port, uint32_t slot);
    DEVICE_OBJECT* WINAPI MU_GetExistingDeviceObject(uint32_t port, uint32_t slot);
}

#define HalReadSMBusByte(SlaveAddress, CommandCode, DataValue)                                                         \
    HalReadSMBusValue(SlaveAddress, CommandCode, FALSE, DataValue)
#define HalWriteSMBusByte(SlaveAddress, CommandCode, DataValue)                                                        \
    HalWriteSMBusValue(SlaveAddress, CommandCode, FALSE, DataValue)