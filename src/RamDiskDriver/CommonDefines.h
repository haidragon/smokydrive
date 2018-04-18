#ifndef __SMOKY_RAMDISK_COMMON_DEFINES__
#define __SMOKY_RAMDISK_COMMON_DEFINES__

#define RAMDISK_NAME                    L"SmokyDisk" 
#define DRIVE_LETTER_LEN    16
#define NT_DEVICE_NAME                  L"\\Device\\SmokyDisk"
#define DOS_DEVICE_NAME                 L"\\DosDevices\\Global\\Z:"
#define MY_POOLTAG                     'YKMS'  // => "SMKY"

#define BYTES_PER_SECTOR            4096
#define SECTORS_PER_TRACK           1024
#define TRACKS_PER_CYLINDER         4
#define CYLINDERS                   64

//default 1GB
//#define DEFAULT_DISK_SIZE           CYLINDERS*TRACKS_PER_CYLINDER*SECTORS_PER_TRACK*BYTES_PER_SECTOR
#define DEFAULT_DISK_SIZE           1048576*256

//devExt->DiskGeometry.SectorsPerTrack = 32;     // Using Ramdisk value
//devExt->DiskGeometry.TracksPerCylinder = 2;    // Using Ramdisk value

typedef struct _SMOKYDISK_SETTING
{
    LARGE_INTEGER DiskSize;        //in bytes
    DISK_GEOMETRY Geometry;
    //UINT32 Cylinders;
    //UINT32 TracksPerCylinder;
    //UINT32 SectorsPerTrack;
    //UINT32 BytesPerSector;   //in bytes

}SMOKYDISK_SETTING, *PSMOKYDISK_SETTING;


#endif __SMOKY_RAMDISK_COMMON_DEFINES__