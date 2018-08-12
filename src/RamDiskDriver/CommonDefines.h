#ifndef __SMOKY_RAMDISK_COMMON_DEFINES__
#define __SMOKY_RAMDISK_COMMON_DEFINES__

#define RAMDISK_NAME                    L"SmokyDisk" 
#define DRIVE_LETTER_LEN            16
#define SYMLINK_BUFFER_SIZE         128
#define NT_DEVICE_NAME                  L"\\Device\\SmokyDisk"
#define DOS_DEVICE_NAME                 L"\\DosDevices\\Z:"
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

typedef struct  _BOOT_SECTOR
{
    UCHAR       Jump[3];          // x86 jmp instruction, checked by FS
    CCHAR       OemName[8];       // OEM name of formatter
    USHORT      BytesPerSec;      // Bytes per Sector
    UCHAR       SecPerClus;       // Sectors per Cluster
    USHORT      ResSectors;       // Reserved Sectors
    UCHAR       FATs;             // Number of FATs - we always use 1
    USHORT      RootDirEnts;      // Number of Root Dir Entries
    USHORT      Sectors;          // Number of Sectors
    UCHAR       Media;            // Media type - we use RAMDISK_MEDIA_TYPE
    USHORT      FATsecs;          // Number of FAT sectors
    USHORT      SecPerTrack;      // Sectors per Track - we use 32
    USHORT      Heads;            // Number of Heads - we use 2
    ULONG       HiddenSecs;       // Hidden Sectors - we set to 0
    ULONG       HugeSectors;      // Number of Sectors if > 32 MB size
    UCHAR       DriveNumber;      // Drive Number - not used
    UCHAR       Reserved1;        // Reserved
    UCHAR       BootSignature;    // New Format Boot Signature - 0x29
    ULONG       VolumeID;         // VolumeID - set to 0x12345678
    CCHAR       Label[11];        // Label - set to RamDisk
    CCHAR       FileSystemType[8];// File System Type - FAT12 or FAT16
    CCHAR       Reserved2[448];   // Reserved
    UCHAR       Sig2[2];          // Originial Boot Signature - 0x55, 0xAA
}   BOOT_SECTOR, *LPBOOT_SECTOR;

#endif __SMOKY_RAMDISK_COMMON_DEFINES__