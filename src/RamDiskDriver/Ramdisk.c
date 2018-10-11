#include "driver.h"
#include "ramdisk.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, LoadSetting)
#pragma alloc_text (PAGE, InitDeviceExtension)
#pragma alloc_text (PAGE, AssignRamdiskDeviceName)
#pragma alloc_text (PAGE, IsValidIoParams)
#endif

NTSTATUS RegisterRamdiskDeviceName(WDFDEVICE device, PWDFDEVICE_INIT devinit)
{
    KdPrint((" RegisterRamdiskDeviceName CALLed!\r\n"));
    NTSTATUS status = 0;

    //assign device name. 
    //If name is not unique, we can't install ramdisk more than 1 instance.
    DECLARE_CONST_UNICODE_STRING(nt_name, NT_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(dos_name, DOS_DEVICE_NAME);

    status = WdfDeviceInitAssignName(devinit, &nt_name);
    if(NT_SUCCESS(status))
        status = WdfDeviceCreateSymbolicLink(device, &dos_name);
    else
        KdPrint(("WdfDeviceInitAssignName() failed 0x%08X", status));

        //TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceInitAssignName() failed [%!STATUS!]", status);
    
    if (!NT_SUCCESS(status))
        KdPrint(("WdfDeviceCreateSymbolicLink() failed 0x%08X", status));

        //TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceCreateSymbolicLink() failed [%!STATUS!]", status);
    return status;
}

NTSTATUS InitDeviceExtension(PDEVICE_EXTENSION devext)
{
    NTSTATUS status = 0;
    SMOKYDISK_SETTING setting = {0};

    KdPrint((" InitDeviceExtension CALLed!\r\n"));

    LoadSetting(&setting);
    SIZE_T size = (SIZE_T)setting.DiskSize.QuadPart;
    
    //Storage Device Number
    //why begin from 99?
    devext->StorageNumber = 99 + 1;

    devext->DiskMemory = ExAllocatePoolWithTag(NonPagedPool, size, MY_POOLTAG);
    if (NULL == devext->DiskMemory)
    {
        KdPrint(("ExAllocatePoolWithTag() failed 0x%08X", status));
        //TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ExAllocatePoolWithTag() failed");
        return STATUS_MEMORY_NOT_ALLOCATED;
    }
    devext->DiskSize.QuadPart = setting.DiskSize.QuadPart;
    RtlCopyMemory(&devext->Geometry, &setting.Geometry, sizeof(DISK_GEOMETRY));
    //devext->SymbolicLink.Buffer = devext->SymLinkBuffer;
    //devext->SymbolicLink.MaximumLength = sizeof(devext->SymLinkBuffer);

    //USHORT str_size = (USHORT)wcslen(DOS_DEVICE_NAME)*sizeof(WCHAR);
    //RtlCopyMemory(devext->SymLinkBuffer, DOS_DEVICE_NAME, str_size);
    //devext->SymbolicLink.Length = str_size;
    
    ExUuidCreate(&devext->DeviceGUID);
    devext->HotPlugInfo.Size = sizeof(STORAGE_HOTPLUG_INFO);
    devext->HotPlugInfo.DeviceHotplug = TRUE;
    devext->HotPlugInfo.MediaHotplug = TRUE;
    devext->HotPlugInfo.MediaRemovable = TRUE;
    devext->HotPlugInfo.WriteCacheEnableOverride = FALSE;   //cache policy無法在DeviceManager頁面修改


    return status;
}

NTSTATUS RegisterInterface(IN WDFDEVICE dev)
{
    NTSTATUS status = 0;
    LPGUID guid = (LPGUID)&GUID_DEVINTERFACE_VOLUME;
    //查詢DEVICE INTERFACE SYMBOLIC LINK (GUID) 並存在 non paged pool
    UNICODE_STRING link;
    WDFSTRING  str_handle;

    status = WdfDeviceCreateDeviceInterface(dev, guid, NULL);

    if (NT_SUCCESS(status)) 
    {
        // Retrieve the symbolic link name (a GUID) that the system assigned to the device interface registered for the VirtVol device
        // and save it in the device extension for later use
        status = WdfStringCreate(NULL, WDF_NO_OBJECT_ATTRIBUTES, &str_handle);
        if (!NT_SUCCESS(status))
            return status;

        status = WdfDeviceRetrieveDeviceInterfaceString(dev, guid, NULL, str_handle);
        if (!NT_SUCCESS(status))
            return status;
        PDEVICE_EXTENSION devext = DeviceGetExtension(dev);

        WdfStringGetUnicodeString(str_handle, &link);
        devext->DeviceInterfaceSymbolicLink.MaximumLength = link.Length + sizeof(UNICODE_NULL);
        devext->DeviceInterfaceSymbolicLink.Length = link.Length;
        devext->DeviceInterfaceSymbolicLink.Buffer = ExAllocatePoolWithTag(NonPagedPool, devext->DeviceInterfaceSymbolicLink.MaximumLength, MY_POOLTAG);

        if (NULL == devext->DeviceInterfaceSymbolicLink.Buffer)
            return STATUS_INSUFFICIENT_RESOURCES;

        RtlCopyUnicodeString(&devext->DeviceInterfaceSymbolicLink, &link);
        // Enable the device interface registered for the virtvol device
        WdfDeviceSetDeviceInterfaceState(dev, guid, NULL, TRUE);
    }
}

void LoadSetting(PSMOKYDISK_SETTING setting)
{
    KdPrint((" LoadSetting CALLed!\r\n"));

//todo: read registry to load settings
    setting->DiskSize.QuadPart = DEFAULT_DISK_SIZE;
    setting->Geometry.TracksPerCylinder = TRACKS_PER_CYLINDER;
    setting->Geometry.SectorsPerTrack = SECTORS_PER_TRACK;
    setting->Geometry.BytesPerSector = BYTES_PER_SECTOR;

    //DiskSize == BytesPerSector * SectorsPerTrack * TracksPerCylinder * Cylinders
    //Here we determine Cylinders by DiskTotalSize.
    //setting->Geometry.Cylinders.QuadPart = 
    //                setting->DiskSize.QuadPart 
    //                        / setting->Geometry.TracksPerCylinder 
    //                        / setting->Geometry.SectorsPerTrack 
    //                        / setting->Geometry.BytesPerSector;
}

BOOLEAN IsValidIoParams(
    IN PDEVICE_EXTENSION DevExt,
    IN LARGE_INTEGER ByteOffset,
    IN size_t Length)
{
//  1.end of IO range should not exceed total size range.
//  2.begin of IO range should be fit in disk.
//  3.IO offset should be positive number.
    KdPrint((" IsValidIoParams CALLed!\r\n"));

    LARGE_INTEGER begin = ByteOffset;
    LARGE_INTEGER end = ByteOffset;
    end.QuadPart = end.QuadPart + Length;

    if (ByteOffset.QuadPart < 0)
        return FALSE;
    if (begin.QuadPart < 0 || begin.QuadPart > (DevExt->DiskSize.QuadPart-1))
        return FALSE;
    if (end.QuadPart <= 0 || end.QuadPart > (DevExt->DiskSize.QuadPart - 1))
        return FALSE;

    return TRUE;
}

