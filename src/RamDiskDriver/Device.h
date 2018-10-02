/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"
#include <ntdddisk.h>
#include <wdf.h>
#include "CommonDefines.h"

//typedef struct _DISK_INFO {
//    ULONG   DiskSize;           // Ramdisk size in bytes
//    ULONG   RootDirEntries;     // No. of root directory entries
//    ULONG   SectorsPerCluster;  // Sectors per cluster
//    UNICODE_STRING DriveLetter; // Drive letter to be used
//} DISK_INFO, *PDISK_INFO;


//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_EXTENSION
{
    //ULONG PrivateDeviceData;  // just a placeholder
    LARGE_INTEGER       DiskSize;
    BYTE                *DiskMemory;
    DISK_GEOMETRY       Geometry;
    ULONG               StorageNumber;
    //WCHAR       DriveLetterBuffer[DRIVE_LETTER_LEN];
    //WCHAR       DosDeviceNameBuffer[DOS_DEVNAME_BUFFER_SIZE];
    GUID                DeviceGUID;
    UNICODE_STRING      SymbolicLink;               // Dos symbolic name; Drive letter
    WCHAR               SymLinkBuffer[SYMLINK_BUFFER_SIZE];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#pragma pack(1)

//boot sectorµ²ºc
typedef struct  _BOOT_SECTOR
{
    UCHAR       bsJump[3];          // x86 jmp instruction, checked by FS
    CCHAR       bsOemName[8];       // OEM name of formatter
    USHORT      bsBytesPerSec;      // Bytes per Sector
    UCHAR       bsSecPerClus;       // Sectors per Cluster
    USHORT      bsResSectors;       // Reserved Sectors
    UCHAR       bsFATs;             // Number of FATs - we always use 1
    USHORT      bsRootDirEnts;      // Number of Root Dir Entries
    USHORT      bsSectors;          // Number of Sectors
    UCHAR       bsMedia;            // Media type - we use VIRTVOL_MEDIA_TYPE
    USHORT      bsFATsecs;          // Number of FAT sectors
    USHORT      bsSecPerTrack;      // Sectors per Track - we use 32
    USHORT      bsHeads;            // Number of Heads - we use 2
    ULONG       bsHiddenSecs;       // Hidden Sectors - we set to 0
    ULONG       bsHugeSectors;      // Number of Sectors if > 32 MB size
    UCHAR       bsDriveNumber;      // Drive Number - not used
    UCHAR       bsReserved1;        // Reserved
    UCHAR       bsBootSignature;    // New Format Boot Signature - 0x29
    ULONG       bsVolumeID;         // VolumeID - set to 0x12345678
    CCHAR       bsLabel[11];        // Label - set to Virtvol
    CCHAR       bsFileSystemType[8];// File System Type - FAT12 or FAT16
    CCHAR       bsReserved2[448];   // Reserved
    UCHAR       bsSig2[2];          // Originial Boot Signature - 0x55, 0xAA
}   BOOT_SECTOR, *PBOOT_SECTOR;

#pragma pack()

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, DeviceGetExtension)

//
// Function to initialize the device and its callbacks
//
NTSTATUS
RamDiskDriverCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

