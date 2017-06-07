/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_SMOKY_RAMDISK,
    0x2028da3c,0x185e,0x48b4,0x95,0xff,0x0e,0x37,0x4f,0xc3,0x88,0x1d);
// {2028da3c-185e-48b4-95ff-0e374fc3881d}
