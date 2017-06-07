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
    //UNICODE_STRING      SymbolicLink;               // Dos symbolic name; Drive letter
    //WCHAR               SymLinkBuffer[DRIVE_LETTER_LEN];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

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

