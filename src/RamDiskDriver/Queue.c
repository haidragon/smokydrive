/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, RamDiskDriverQueueInitialize)
#endif

NTSTATUS
RamDiskDriverQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:


     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;

    PAGED_CODE();
    
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

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );

    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfIoQueueCreate() failed 0x%08X", status));
        //TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

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
    //TraceEvents(TRACE_LEVEL_INFORMATION, 
    //            TRACE_QUEUE, 
    //            "!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d", 
    //            Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);
    WdfRequestComplete(Request, STATUS_SUCCESS);

    return;
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