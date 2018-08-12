/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, RamDiskDriverCreateDevice)
#endif


NTSTATUS
RamDiskDriverCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_EXTENSION devext;
    WDFDEVICE device;
    NTSTATUS status;
    KdPrint((" RamDiskDriverCreateDevice CALLed!\r\n"));

    PAGED_CODE();

    //sequence:
    //Assign NT Device Name
    //set DeviceType
    //set I/O type
    //set exclusive
    //create device
    //get device extension
    //get sequential io queue
    //set IOCtrl / Read / Write handler function
    //set queue attribute to queue extension (WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE)
    //create i/o queue
    //get queue extension
    //set queue extension's device extension
    //set SetForwardProgressOnQueue() if KMDF_VER >=1.9
    //fill device extension
    //set DosName as Symbolic Link

    DECLARE_CONST_UNICODE_STRING(nt_name, NT_DEVICE_NAME);
    status = WdfDeviceInitAssignName(DeviceInit, &nt_name);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfDeviceInitAssignName() failed 0x%08X", status));
        return status;
    }

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_DISK);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
    WdfDeviceInitSetExclusive(DeviceInit, FALSE);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);
    deviceAttributes.EvtCleanupCallback = RamDiskDriverEvtDriverContextCleanup;

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status))
        KdPrint(("WdfDeviceCreate() failed 0x%08X", status));
        //return status;

    devext = DeviceGetExtension(device);

    status = RamDiskDriverQueueInitialize(device);
    if (!NT_SUCCESS(status)) 
        KdPrint(("RamDiskDriverQueueInitialize() failed 0x%08X", status));

    status = InitDeviceExtension(devext);
    if (!NT_SUCCESS(status))
        KdPrint(("InitDeviceExtension() failed 0x%08X", status));

    status = WdfDeviceCreateSymbolicLink(device, &devext->SymbolicLink);
    if (!NT_SUCCESS(status))
        KdPrint(("WdfDeviceCreateSymbolicLink() failed 0x%08X", status));

    return status;
}


