/*
 * IDE ATA/ATAPI emulation
 */

#ifndef DOSBOX_IDE_H
#define DOSBOX_IDE_H

constexpr int MAX_IDE_CONTROLLERS = 4;

extern const char *ide_names[MAX_IDE_CONTROLLERS];
extern void (*ide_inits[MAX_IDE_CONTROLLERS])(Section *);

void IDE_Auto(int8_t &index,bool &slave);
void IDE_CDROM_Attach(int8_t index,bool slave,uint8_t drive_index);
void IDE_CDROM_Detach(uint8_t drive_index);
void IDE_CDROM_Detach_Ret(int8_t &indexret,bool &slaveret,int8_t drive_index);
void IDE_Hard_Disk_Attach(int8_t index,bool slave,uint8_t bios_disk_index);
void IDE_Hard_Disk_Detach(uint8_t bios_disk_index);
void IDE_ResetDiskByBIOS(uint8_t disk);

#endif
