
#include <ntdddisk.h>
#include <wdf.h>

NTSTATUS AssignRamdiskDeviceName(WDFDEVICE device, PWDFDEVICE_INIT DeviceInit);
NTSTATUS InitDeviceExtension(PDEVICE_EXTENSION devext);
void LoadSetting(PSMOKYDISK_SETTING setting);
BOOLEAN IsValidIoParams(IN PDEVICE_EXTENSION devExt, IN LARGE_INTEGER ByteOffset, IN size_t Length);
