// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_BIOS_DISK_H
#define DOSBOX_BIOS_DISK_H

#include "dosbox.h"

#include <cstdio>
#include <array>
#include <memory>

#include "ints/bios.h"
#include "dos/dos_inc.h"
#include "hardware/memory.h"

/* The Section handling Bios Disk Access */
#define BIOS_MAX_DISK 10

#define MAX_SWAPPABLE_DISKS 20
struct diskGeo {
	uint32_t ksize;  /* Size in kilobytes */
	uint16_t secttrack; /* Sectors per track */
	uint16_t headscyl;  /* Heads per cylinder */
	uint16_t cylcount;  /* Cylinders per side */
	uint16_t biosval;   /* Type to return from BIOS */
};
extern diskGeo DiskGeometryList[];

class imageDisk  {
public:
	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data);
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data);
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data);
	uint8_t Write_AbsoluteSector(uint32_t sectnum, void * data);

	void Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize);
	void Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize);
	uint8_t GetBiosType(void);
	uint32_t getSectSize(void);

	imageDisk(FILE *img_file, const char *img_name, uint32_t img_size_k, bool is_hdd);
	imageDisk(const imageDisk&) = delete; // prevent copy
	imageDisk& operator=(const imageDisk&) = delete; // prevent assignment

	~imageDisk()
	{
		if (diskimg != nullptr)
			fclose(diskimg);
	}

	bool hardDrive;
	bool active;
	FILE *diskimg;
	char diskname[512];
	uint8_t floppytype;

	uint32_t sector_size;
	uint32_t heads,cylinders,sectors;
private:
	cross_off_t current_fpos;
	enum { NONE,READ,WRITE } last_action;
};

void updateDPT(void);
void incrementFDD(void);

#define MAX_HDD_IMAGES 2

#define MAX_DISK_IMAGES (2 + MAX_HDD_IMAGES)

extern std::array<std::shared_ptr<imageDisk>, MAX_DISK_IMAGES> imageDiskList;
extern std::array<std::shared_ptr<imageDisk>, MAX_SWAPPABLE_DISKS> diskSwap;

extern uint16_t imgDTASeg; /* Real memory location of temporary DTA pointer for fat image disk access */
extern RealPt imgDTAPtr; /* Real memory location of temporary DTA pointer for fat image disk access */
extern DOS_DTA *imgDTA;

/**
 * Insert 2 boot disks starting at swap_position into the drives A and B.
 *
 * Selected disks are wrapped around, so swapping in the last boot disk
 * will place the first disk into drive B.
 *
 * When there's only 1 disk, it will be placed into both A and B drives.
 *
 * When there's no boot disks loaded, this function has no effect.
 */
void swapInDisks(unsigned int swap_position);

void swapInNextDisk(void);
bool getSwapRequest(void);

#endif
