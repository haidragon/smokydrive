/*++

Module Name:

    queue.h

Abstract:

    This file contains the queue definitions.

Environment:

    Kernel-mode Driver Framework

--*/

//
// This is the context that can be placed per queue
// and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT {

//    ULONG PrivateDeviceData;  // just a placeholder
    WDFDEVICE Device;
    PDEVICE_EXTENSION DevExt;
} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)

NTSTATUS
RamDiskDriverQueueInitialize(
    _In_ WDFDEVICE hDevice
    );

//
// Events from the IoQueue object
//

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL RamDiskDriverEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_READ RamDiskDriverEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE RamDiskDriverEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_STOP RamDiskDriverEvtIoStop;

