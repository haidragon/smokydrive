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

    DECLARE_CONST_UNICODE_STRING(nt_name, NT_DEVICE_NAME);
    status = WdfDeviceInitAssignName(DeviceInit, &nt_name);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfDeviceInitAssignName() failed 0x%08X", status));
        return status;
    }

    WDF_PNPPOWER_EVENT_CALLBACKS    power_callbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&power_callbacks);
    power_callbacks.EvtDevicePrepareHardware = RamDiskEvtDevicePrepareHardware;
    power_callbacks.EvtDeviceD0Entry = RamDiskEvtDeviceD0Entry;
    power_callbacks.EvtDeviceReleaseHardware = RamDiskEvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &power_callbacks);

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

NTSTATUS RamDiskEvtDeviceD0Entry(IN WDFDEVICE Device, IN WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);
    KdPrint((" RamDiskEvtDeviceD0Entry CALLed!\r\n"));

    return STATUS_SUCCESS;
}

NTSTATUS RamDiskEvtDevicePrepareHardware(IN WDFDEVICE Device, IN WDFCMRESLIST ResourcesRaw, IN WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    KdPrint((" RamDiskEvtDevicePrepareHardware CALLed!\r\n"));
    return STATUS_SUCCESS;
}
NTSTATUS RamDiskEvtDeviceReleaseHardware(IN WDFDEVICE Device, IN WDFCMRESLIST ResourceListTranslated)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourceListTranslated);
    KdPrint((" RamDiskEvtDeviceReleaseHardware CALLed!\r\n"));
    return STATUS_SUCCESS;
}
