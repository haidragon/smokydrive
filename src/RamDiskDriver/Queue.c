#include "driver.h"
#include "queue.tmh"
#include <mountmgr.h>

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
    queueConfig.EvtIoStop = RamDiskDriverEvtIoStop;
    queueConfig.EvtIoRead = RamDiskDriverEvtIoRead;
    queueConfig.EvtIoWrite = RamDiskDriverEvtIoWrite;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queue_attr, QUEUE_CONTEXT);

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 &queue_attr,
                 &queue
                 );

    if( !NT_SUCCESS(status) ) {
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
    WdfRequestComplete(Request, STATUS_SUCCESS);
    
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
    //不知道為何有這個REQUEST.... wdk sample裡面沒處理這個code
        case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
        {
            PMOUNTDEV_NAME outbuf = NULL;
            info = sizeof(MOUNTDEV_NAME) + wcslen(DOS_DEVICE_NAME) * 2;
            status = WdfRequestRetrieveOutputBuffer(Request, info, &outbuf, &size);
            if (NT_SUCCESS(status)) 
            {
                outbuf->NameLength = (USHORT)wcslen(DOS_DEVICE_NAME) * 2;
                RtlCopyMemory(outbuf->Name, DOS_DEVICE_NAME, outbuf->NameLength);
                status = STATUS_SUCCESS;
            }
        }
        break;
        case IOCTL_DISK_GET_PARTITION_INFO: 
        {
            PPARTITION_INFORMATION outbuf;
            LPBOOT_SECTOR boot_sector = (LPBOOT_SECTOR)devext->DiskMemory;

            info = sizeof(PARTITION_INFORMATION);

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(PARTITION_INFORMATION), &outbuf, &size);
            if (NT_SUCCESS(status)) {

                outbuf->PartitionType = PARTITION_FAT32;
                outbuf->RecognizedPartition = TRUE;
                if (boot_sector->FileSystemType[0] == 0)
                {
                    outbuf->PartitionType = PARTITION_ENTRY_UNUSED;
                    outbuf->RecognizedPartition = FALSE;
                }
                outbuf->BootIndicator = FALSE;
                outbuf->RewritePartition = FALSE;
                outbuf->StartingOffset.QuadPart = 0;
                outbuf->PartitionLength.QuadPart = devext->DiskSize.QuadPart;
                outbuf->HiddenSectors = (ULONG)(1L);
                outbuf->PartitionNumber = (ULONG)(-1L);

                status = STATUS_SUCCESS;
            }
        }
        break;

        case IOCTL_DISK_GET_DRIVE_GEOMETRY:  
        {
            PDISK_GEOMETRY outbuf;
            //
            // Return the drive geometry for the ram disk. Note that
            // we return values which were made up to suit the disk size.
            //
            info = sizeof(DISK_GEOMETRY);

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DISK_GEOMETRY), &outbuf, &size);
            if (NT_SUCCESS(status) && size >= sizeof(DISK_GEOMETRY)) 
            {
                RtlCopyMemory(outbuf, &(devext->Geometry), sizeof(DISK_GEOMETRY));
                status = STATUS_SUCCESS;
            }
        }
        break;

        case IOCTL_DISK_CHECK_VERIFY:
        case IOCTL_DISK_IS_WRITABLE:

            //
            // Return status success
            //

            status = STATUS_SUCCESS;
            break;
    }

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

    KdPrint((" RamDiskDriverEvtIoStop CALLed!\r\n"));

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