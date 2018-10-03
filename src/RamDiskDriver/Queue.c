#include "driver.h"
#include "queue.tmh"
#include "CommonDefines.h"
#include <mountmgr.h>
#include <ntdddisk.h>
#include <ntddvol.h>
#include <mountdev.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, RamDiskDriverQueueInitialize)
#endif

NTSTATUS
RamDiskDriverQueueInitialize(
    _In_ WDFDEVICE Device
    )
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;
    WDF_OBJECT_ATTRIBUTES  queue_attr;
    PAGED_CODE();
    
    KdPrint((" RamDiskDriverQueueInitialize CALLed!\r\n"));

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchSequential);  //todo: read-write lock to accept parallel io requests.

    queueConfig.EvtIoDeviceControl = RamDiskDriverEvtIoDeviceControl;
    //queueConfig.EvtIoStop = RamDiskDriverEvtIoStop;
    queueConfig.EvtIoRead = RamDiskDriverEvtIoRead;
    queueConfig.EvtIoWrite = RamDiskDriverEvtIoWrite;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queue_attr, QUEUE_CONTEXT);

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 &queue_attr,
                 &queue
                 );

    if( !NT_SUCCESS(status) ) 
    {
        KdPrint(("WdfIoQueueCreate() failed 0x%08X", status));
        return status;
    }
    
    PQUEUE_CONTEXT ctx = NULL;
    ctx = QueueGetContext(queue);
    ctx->Device = Device;
    PDEVICE_EXTENSION devext = DeviceGetExtension(Device);
    ctx->DevExt = devext;

    return status;
}

VOID
RamDiskDriverEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    
    NTSTATUS          status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR         info = 0;
    size_t            size;
    PDEVICE_EXTENSION devext = QueueGetContext(Queue)->DevExt;

    IOCTL_CODE code = { 0 };
    code.Value = IoControlCode;
    
    KdPrint(("IOCTL code[0x%08X]: Type=0x%08X, Function=0x%08X, Access=0x%08X, Method=0x%08X\r\n",
        IoControlCode, code.Fields.DeviceType, code.Fields.Function,
        code.Fields.Access, code.Fields.Method));

    switch (IoControlCode) 
    {
    //VOLUMN system 從VISTA後都會對VOLUMN device發這個code
        case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
            {
                PMOUNTDEV_NAME name = NULL;
                size = 0;
                if (OutputBufferLength < sizeof(MOUNTDEV_NAME))
                {
                //回應 STATUS_BUFFER_TOO_SMALL  => MountMgr就不會再鳥你
                //回應 STATUS_BUFFER_OVERFLOW => MountMgr 會調整buffer size再來問一次
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(MOUNTDEV_NAME);
                    KdPrint(("Output Buffer too small\r\n"));
                    break;
                }
            
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(MOUNTDEV_NAME), &name, &size);
                KdPrint(("WdfRequestRetrieveOutputBuffer() == 0x%08X%, size = %d\n", status, size));

                //query 會有3次，頭一次取得 NTDevice Name
                //但第二次只會有 wcslen() == 4 的 outputbuffer
                //第三次會依第二次回傳的值來配置
                if (NT_SUCCESS(status)) 
                {
                    size = sizeof(MOUNTDEV_NAME) + (wcslen(NT_DEVICE_NAME) + 1) * 2;
                    name->NameLength = (USHORT)size;
                    info = FIELD_OFFSET(MOUNTDEV_NAME, Name) + name->NameLength;

                    if (OutputBufferLength < size)
                    {
                        status = STATUS_BUFFER_OVERFLOW;
                        info = FIELD_OFFSET(MOUNTDEV_NAME, Name);
                        break;
                    }

                    RtlZeroMemory(name, size);
                    RtlCopyMemory(name->Name, NT_DEVICE_NAME, name->NameLength);
                    status = STATUS_SUCCESS;
                
                    KdPrint(("VirtVol IOCTL_MOUNTDEV_QUERY_DEVICE_NAME SUCCESS %ws\n", name->Name));
                }
            }
            break;

        //舊型 MBR 的 partition info
        case IOCTL_DISK_GET_PARTITION_INFO: 
            {
                PPARTITION_INFORMATION part_info;
                LPBOOT_SECTOR boot_sector = (LPBOOT_SECTOR)devext->DiskMemory;

                info = sizeof(PARTITION_INFORMATION);

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(PARTITION_INFORMATION), &part_info, &size);
                if (NT_SUCCESS(status)) {

                    part_info->PartitionType = PARTITION_FAT32;
                    part_info->RecognizedPartition = TRUE;
                    if (boot_sector->FileSystemType[0] == 0)
                    {
                        part_info->PartitionType = PARTITION_ENTRY_UNUSED;
                        part_info->RecognizedPartition = FALSE;
                    }
                    part_info->BootIndicator = FALSE;
                    part_info->RewritePartition = FALSE;
                    part_info->StartingOffset.QuadPart = 0;
                    part_info->PartitionLength.QuadPart = devext->DiskSize.QuadPart;
                    part_info->HiddenSectors = (ULONG)(1L);
                    part_info->PartitionNumber = (ULONG)(-1L);

                    status = STATUS_SUCCESS;
                }
            }
            break;

        //Get the disk geometry information.
        //e.g. Cylinders, sectors, tracks....etc.
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:  
            {
                PDISK_GEOMETRY geometry;
                //
                // Return the drive geometry for the ram disk. Note that
                // we return values which were made up to suit the disk size.
                //
                info = sizeof(DISK_GEOMETRY);

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DISK_GEOMETRY), &geometry, &size);
                if (NT_SUCCESS(status) && size >= sizeof(DISK_GEOMETRY)) 
                {
                    RtlCopyMemory(geometry, &(devext->Geometry), sizeof(DISK_GEOMETRY));
                    status = STATUS_SUCCESS;
                }
            }
            break;

        //一個Ramdisk Driver可以配置多個不同的 FDO....
        case IOCTL_STORAGE_GET_DEVICE_NUMBER:
            {
                if (OutputBufferLength < sizeof(STORAGE_DEVICE_NUMBER)) 
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(STORAGE_DEVICE_NUMBER);
                }
                else {
                    PSTORAGE_DEVICE_NUMBER device_no;

                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(STORAGE_DEVICE_NUMBER), &device_no, &size);
                    if (NT_SUCCESS(status)) {
                        device_no->DeviceType = FILE_DEVICE_DISK;
                        device_no->DeviceNumber = devext->StorageNumber;
                        device_no->PartitionNumber = (ULONG)(-1L);
                        status = STATUS_SUCCESS;
                        info = sizeof(STORAGE_DEVICE_NUMBER);
                    }
                }
            }
            break;

        //Query total size of disk
        case IOCTL_DISK_GET_LENGTH_INFO:
            {
                if (OutputBufferLength < sizeof(GET_LENGTH_INFORMATION)) {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(GET_LENGTH_INFORMATION);
                }
                else {
                    PGET_LENGTH_INFORMATION leninfo;

                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(GET_LENGTH_INFORMATION), &leninfo, &size);
                    if (NT_SUCCESS(status)) {
                        leninfo->Length.QuadPart = devext->DiskSize.QuadPart;
                        status = STATUS_SUCCESS;
                        info = sizeof(GET_LENGTH_INFORMATION);
                    }
                }
            }
            break;
        //新型 MBR/GPT 兩者兼用的 partition info
        case IOCTL_DISK_GET_PARTITION_INFO_EX:
            {
                if (OutputBufferLength < sizeof(PARTITION_INFORMATION_EX)) 
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(PARTITION_INFORMATION_EX);
                }
                else 
                {
                    PPARTITION_INFORMATION_EX part_info_ex;
                    //PBOOT_SECTOR bootSector = (PBOOT_SECTOR)devext->DiskMemory;
                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(PARTITION_INFORMATION_EX), &part_info_ex, &size);
                    if (NT_SUCCESS(status)) {
                        part_info_ex->PartitionStyle = PARTITION_STYLE_MBR;
                        part_info_ex->StartingOffset.QuadPart = 0;
                        part_info_ex->PartitionLength.QuadPart = devext->DiskSize.QuadPart;
                        part_info_ex->PartitionNumber = (ULONG)(-1L);
                        part_info_ex->RewritePartition = FALSE;
                        part_info_ex->Mbr.PartitionType = PARTITION_IFS;  //0x07 == IFS 或 NTFS 或 exFAT
                            //(bootSector->bsFileSystemType[4] == '6') ? PARTITION_FAT_16 : PARTITION_FAT_12;
                        part_info_ex->Mbr.BootIndicator = FALSE;
                        part_info_ex->Mbr.RecognizedPartition = FALSE;
                        part_info_ex->Mbr.HiddenSectors = (ULONG)(-1L);
                        status = STATUS_SUCCESS;
                        info = sizeof(PARTITION_INFORMATION_EX);
                    }
                }
            }
            break;
        case IOCTL_DISK_CHECK_VERIFY:
        case IOCTL_DISK_IS_WRITABLE:
        case IOCTL_VOLUME_IS_DYNAMIC:
        case IOCTL_VOLUME_ONLINE:
            status = STATUS_SUCCESS;
            break;

        //取得Disk Volume GUID => MountMgr 都用 GUID 操作 volume 的
        case IOCTL_MOUNTDEV_QUERY_STABLE_GUID:
            {
                if (OutputBufferLength < sizeof(MOUNTDEV_STABLE_GUID)) 
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(MOUNTDEV_STABLE_GUID);
                }
                else {
                    PMOUNTDEV_STABLE_GUID guid;
                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(MOUNTDEV_STABLE_GUID), &guid, &size);
                    if (NT_SUCCESS(status)) {
                        status = STATUS_SUCCESS;
                        guid->StableGuid = devext->DeviceGUID;
                        info = sizeof(MOUNTDEV_STABLE_GUID);
                    }
                }
            }
            break;
        //(optional) The mount manager uses this IOCTL to alert the client driver 
        //that a persistent name has been assigned to its volume. 
        //The input for this IOCTL is the persistent name assigned.
        case IOCTL_MOUNTDEV_LINK_CREATED:
            {
                PMOUNTDEV_NAME name;
                if (InputBufferLength >= sizeof(MOUNTDEV_NAME)) {
                    status = WdfRequestRetrieveInputBuffer(Request, sizeof(MOUNTDEV_NAME), &name, &size);
                    if (NT_SUCCESS(status)) 
                    {
                        KdPrint(("MOUNTDEV_LINK NAME = %S", name->Name));
                    }
                }
                status = STATUS_SUCCESS;
            }
            break;
        //取得Volume的 GPT 屬性
        case IOCTL_VOLUME_GET_GPT_ATTRIBUTES:
            {
                if (OutputBufferLength < sizeof(VOLUME_GET_GPT_ATTRIBUTES_INFORMATION)) {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(VOLUME_GET_GPT_ATTRIBUTES_INFORMATION);
                }
                else {
                    PVOLUME_GET_GPT_ATTRIBUTES_INFORMATION gpt_attrib;

                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(VOLUME_GET_GPT_ATTRIBUTES_INFORMATION), &gpt_attrib, &size);
                    if (NT_SUCCESS(status)) {
                        gpt_attrib = 0;
                        status = STATUS_SUCCESS;
                        info = sizeof(VOLUME_GET_GPT_ATTRIBUTES_INFORMATION);
                    }
                }
            }
            break;
        //取得這個Volume到底包含哪幾個Disk上的SectorRange聯合而成？
        case IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS:
            {
                PVOLUME_DISK_EXTENTS disk_exts;
                if (OutputBufferLength < sizeof(VOLUME_DISK_EXTENTS)) {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(VOLUME_DISK_EXTENTS);
                    KdPrint(("VirtVol IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS returning STATUS_BUFFER_TOO_SMALL\n"));
                    break;
                }
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(VOLUME_DISK_EXTENTS), &disk_exts, &size);
                if (NT_SUCCESS(status)) {
                    disk_exts->NumberOfDiskExtents = 1;
                    disk_exts->Extents[0].DiskNumber = devext->StorageNumber;
                    disk_exts->Extents[0].StartingOffset.QuadPart = 0;
                    disk_exts->Extents[0].ExtentLength.QuadPart = devext->DiskSize.QuadPart;
                    status = STATUS_SUCCESS;
                    info = sizeof(VOLUME_DISK_EXTENTS);
                }
            }
            break;

        //optional. MountMgr會來問Volume "你建議要把volume mount在哪一槽？"
        //如果volume設計成 auto-mount 就不需要這個，這種情形下mountmgr不會來問
        case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
            status = STATUS_INVALID_DEVICE_REQUEST;
            info = 0;
            break;
        // mountmgr 會來問這個Device的 GUID，後續操作以此GUID為DeviceInterface
        //Volume Interface GUID好像要用 GUID_DEVINTERFACE_VOLUME 去向系統取得？
        case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
            // Set the volume Unique ID to the symbolic link GUID that was returned when the driver registered
            // a GUID_DEVINTERFACE_VOLUME device interface (see the RegisterInterface function)
            {
                PMOUNTDEV_UNIQUE_ID uniqueId = NULL;
                status = STATUS_SUCCESS;
                if (!devext->DeviceInterfaceSymbolicLink.Buffer) 
                {
                    info = 0;
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                if (OutputBufferLength < sizeof(MOUNTDEV_UNIQUE_ID)) 
                {
                    info = sizeof(MOUNTDEV_UNIQUE_ID);
                    status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(MOUNTDEV_UNIQUE_ID), &uniqueId, NULL);
                if (NT_SUCCESS(status)) 
                {
                    RtlZeroMemory(uniqueId, sizeof(MOUNTDEV_UNIQUE_ID));
                    //GUID_DEVINTERFACE_VOLUME;
                    uniqueId->UniqueIdLength = devext->DeviceInterfaceSymbolicLink.Length;
                    if (OutputBufferLength < (sizeof(USHORT) + devext->DeviceInterfaceSymbolicLink.Length)) 
                    {
                        info = sizeof(MOUNTDEV_UNIQUE_ID);
                        status = STATUS_BUFFER_OVERFLOW;
                        break;
                    }
                    RtlCopyMemory(uniqueId->UniqueId, devExt->DeviceInterfaceSymbolicLink.Buffer, uniqueId->UniqueIdLength);
                    info = sizeof(USHORT) + uniqueId->UniqueIdLength;
                    status = STATUS_SUCCESS;
                }
            }
            break;
        case IOCTL_STORAGE_GET_HOTPLUG_INFO:
            {
                if (OutputBufferLength < sizeof(STORAGE_HOTPLUG_INFO)) 
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                    info = sizeof(STORAGE_HOTPLUG_INFO);
                    break;
                }
                else 
                {
                    PSTORAGE_HOTPLUG_INFO info = NULL;
                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(STORAGE_HOTPLUG_INFO), &info, &size);
                    if (NT_SUCCESS(status)) 
                    {
                        *info = devext->HotPlugInfo;
                        status = STATUS_SUCCESS;
                        info = sizeof(STORAGE_HOTPLUG_INFO);
                    }
                }
            }
            break;
    }

    KdPrint(("WdfRequestCompleteWithInformation() status == 0x%08X%, info = %d\n", status, info));
    WdfRequestCompleteWithInformation(Request, status, info);
}

VOID
RamDiskDriverEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(ActionFlags);
    //TraceEvents(TRACE_LEVEL_INFORMATION, 
    //            TRACE_QUEUE, 
    //            "!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
    //            Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
    //   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
    //   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
    //   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //
    //   Before it can call these methods safely, the driver must make sure that
    //   its implementation of EvtIoStop has exclusive access to the request.
    //
    //   In order to do that, the driver must synchronize access to the request
    //   to prevent other threads from manipulating the request concurrently.
    //   The synchronization method you choose will depend on your driver's design.
    //
    //   For example, if the request is held in a shared context, the EvtIoStop callback
    //   might acquire an internal driver lock, take the request from the shared context,
    //   and then release the lock. At this point, the EvtIoStop callback owns the request
    //   and can safely complete or requeue the request.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge with
    //   a Requeue value of FALSE.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time.
    //
    // In this case, the framework waits until the specified request is complete
    // before moving the device (or system) to a lower power state or removing the device.
    // Potentially, this inaction can prevent a system from entering its hibernation state
    // or another low system power state. In extreme cases, it can cause the system
    // to crash with bugcheck code 9F.
    //

    KdPrint((" RamDiskDriverEvtIoStop CaLLed!\r\n"));

    return;
}

VOID
RamDiskDriverEvtIoRead(
IN WDFQUEUE Queue,
IN WDFREQUEST Request,
IN size_t Length
)
{
    PDEVICE_EXTENSION      devext = QueueGetContext(Queue)->DevExt;
    NTSTATUS               status = STATUS_INVALID_PARAMETER;
    WDF_REQUEST_PARAMETERS params;
    LARGE_INTEGER          offset;
    WDFMEMORY              hmem;

    KdPrint(("RamDiskDriverEvtIoRead Called!\r\n"));

    UNREFERENCED_PARAMETER(Queue);
    _Analysis_assume_(Length > 0);

    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);
    
    offset.QuadPart = params.Parameters.Read.DeviceOffset;
    if (IsValidIoParams(devext, offset, Length))
    {
        status = WdfRequestRetrieveOutputMemory(Request, &hmem);
        if (NT_SUCCESS(status))
        {
            PVOID src_addr = devext->DiskMemory + offset.LowPart;
            status = WdfMemoryCopyFromBuffer(hmem, 0, src_addr, Length);

        }
    }

    WdfRequestCompleteWithInformation(Request, status, (ULONG_PTR)Length);
}

VOID RamDiskDriverEvtIoWrite(
IN WDFQUEUE Queue,
IN WDFREQUEST Request,
IN size_t Length)
{
    PDEVICE_EXTENSION      devext = QueueGetContext(Queue)->DevExt;
    NTSTATUS               status = STATUS_INVALID_PARAMETER;
    WDF_REQUEST_PARAMETERS params;
    LARGE_INTEGER          offset;
    WDFMEMORY              hmem;

    _Analysis_assume_(Length > 0);

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(devext);
    UNREFERENCED_PARAMETER(hmem);

    KdPrint(("RamDiskDriverEvtIoWrite Called!\r\n"));

    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    offset.QuadPart = params.Parameters.Write.DeviceOffset;

    if (IsValidIoParams(devext, offset, Length))
    {
        status = WdfRequestRetrieveInputMemory(Request, &hmem);
        if (NT_SUCCESS(status))
        {
            PVOID dest_addr = devext->DiskMemory + offset.LowPart;
            status = WdfMemoryCopyToBuffer(hmem, 0, dest_addr, Length);

        }
    }

    WdfRequestCompleteWithInformation(Request, status, (ULONG_PTR)Length);
}