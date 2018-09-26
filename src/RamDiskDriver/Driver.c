/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "driver.tmh"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, RamDiskDriverEvtDeviceAdd)
#pragma alloc_text (PAGE, RamDiskDriverEvtDriverContextCleanup)
#pragma alloc_text (PAGE, RamdiskDriverUnload)
#endif


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    KdPrint((" DriverEntry CALLed!\r\n"));

    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_DRIVER_CONFIG_INIT(&config, RamDiskDriverEvtDeviceAdd);
    config.EvtDriverUnload = RamDiskDriverEvtDriverUnload;
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) 
        KdPrint((" WdfDriverCreate() Failed! 0x%08X" , status));
    //    return status;
    //}
    return status;
}

NTSTATUS
RamDiskDriverEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();
    KdPrint((" RamDiskDriverEvtDeviceAdd CALLed!\r\n"));

    status = RamDiskDriverCreateDevice(DeviceInit);
    return status;
}

//VOID RamdiskDriverUnload(WDFDRIVER   pDriverObject)
//{
//    UNREFERENCED_PARAMETER(pDriverObject);
//    PAGED_CODE();
//}


VOID
RamDiskDriverEvtDriverContextCleanup(
    _In_ WDFOBJECT Device
    )
{
    PAGED_CODE ();
    PDEVICE_EXTENSION pDevExt = DeviceGetExtension(Device);

    KdPrint((" RamDiskDriverEvtDriverContextCleanup CALLed!\r\n"));

    if (pDevExt->DiskMemory)
    {
        ExFreePool(pDevExt->DiskMemory);
        pDevExt->DiskMemory = NULL;
    }
}

VOID RamDiskDriverEvtDriverUnload(IN WDFDRIVER Driver)
{
    DbgPrint("RamDiskDriverEvtDriverUnload: \n");
    UNREFERENCED_PARAMETER(Driver);
    return;
}

