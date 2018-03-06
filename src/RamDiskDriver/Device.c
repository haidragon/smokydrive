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
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_EXTENSION devext;
    WDFDEVICE device;
    NTSTATUS status;

    PAGED_CODE();

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_DISK);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
    WdfDeviceInitSetExclusive(DeviceInit, FALSE);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);
    deviceAttributes.EvtCleanupCallback = RamDiskDriverEvtDriverContextCleanup;
    DECLARE_CONST_UNICODE_STRING(nt_name, NT_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(dos_name, DOS_DEVICE_NAME);

    status = WdfDeviceInitAssignName(DeviceInit, &nt_name);
    if (!NT_SUCCESS(status))
        return status;

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status))
        return status;

    status = WdfDeviceCreateSymbolicLink(device, &dos_name);
    devext = DeviceGetExtension(device);
    status = InitDeviceExtension(devext);

    //
    // Create a device interface so that applications can find and talk
    // to us.
    //

    if (NT_SUCCESS(status))
    {
        status = WdfDeviceCreateDeviceInterface(
            device,
            &GUID_DEVINTERFACE_SMOKY_RAMDISK,
            NULL // ReferenceString
            );
    }
    //else
    //    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "AssignRamdiskDeviceName() failed %!STATUS!", status);

    if (NT_SUCCESS(status)) {
        //
        // Initialize the I/O Package and any Queues
        //
        status = RamDiskDriverQueueInitialize(device);
    }

    return status;
}


