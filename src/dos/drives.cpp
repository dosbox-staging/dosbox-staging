/*
 *  Copyright (C) 2002-2020  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Wengier: LFN support
 */

#include "drives.h"

#include "support.h"

bool wild_match(char *haystack, char *needle) {
	size_t max, i;
    for (; *needle != '\0'; ++needle) {
        switch (*needle) {
        case '?':
            if (*haystack == '\0')
                return false;
            ++haystack;
            break;
        case '*':
            if (needle[1] == '\0')
                return true;
            max = strlen(haystack);
            for (i = 0; i < max; i++)
                if (wild_match(haystack + i, needle + 1))
                    return true;
            return false;
        default:
            if (toupper(*haystack) != *needle)
                return false;
            ++haystack;
        }
    }
    return *haystack == '\0';
}

bool WildFileCmp(const char * file, const char * wild) 
{
	char file_name[9];
	char file_ext[4];
    char wild_name[10];
    char wild_ext[5];
	const char * find_ext;
	Bitu r;

    for (r=0;r<9;r++) {
        file_name[r]=0;
        wild_name[r]=0;
    }
    wild_name[9]=0;
    for (r=0;r<4;r++) {
        file_ext[r]=0;
        wild_ext[r]=0;
    }
    wild_ext[4]=0;

	find_ext=strrchr(file,'.');
	if (find_ext) {
		Bitu size=(Bitu)(find_ext-file);
		if (size>8) size=8;
		memcpy(file_name,file,size);
		find_ext++;
		memcpy(file_ext,find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
	} else {
		memcpy(file_name,file,(strlen(file) > 8) ? 8 : strlen(file));
	}
	upcase(file_name);upcase(file_ext);
	find_ext=strrchr(wild,'.');
	if (find_ext) {
		Bitu size=(Bitu)(find_ext-wild);
		if (size>9) size=9;
		memcpy(wild_name,wild,size);
		find_ext++;
		memcpy(wild_ext,find_ext,(strlen(find_ext)>4) ? 4 : strlen(find_ext));
	} else {
		memcpy(wild_name,wild,(strlen(wild) > 9) ? 9 : strlen(wild));
	}
	upcase(wild_name);upcase(wild_ext);
	/* Names are right do some checking */
	if (uselfn&&strchr(wild_name, '*')) {
		if (strchr(wild,'.')) {
			if (!wild_match(file_name, wild_name)) return false;
			goto checkext;
		} else
			return wild_match((char *)file, wild_name);
	} else {
		r=0;
		while (r<8) {
			if (wild_name[r]=='*') goto checkext;
			if (wild_name[r]!='?' && wild_name[r]!=file_name[r]) return false;
			r++;
		}
		if (wild_name[r]&&wild_name[r]!='*') return false;
	}
checkext:
	if (uselfn&&strchr(wild_ext, '*'))
		return wild_match(file_ext, wild_ext);
	else {
		r=0;
		while (r<3) {
			if (wild_ext[r]=='*') return true;
			if (wild_ext[r]!='?' && wild_ext[r]!=file_ext[r]) return false;
			r++;
		}
		if (wild_ext[r]&&wild_ext[r]!='*') return false;
		return true;
	}
}

bool LWildFileCmp(const char * file, const char * wild)
{
    if (!uselfn||*file == 0) return false;
    char file_name[256];
    char file_ext[256];
    char wild_name[256];
    char wild_ext[256];
    const char * find_ext;
    Bitu r;

    for (r=0;r<256;r++) {
      file_name[r]=0;
      wild_name[r]=0;
    }
    for (r=0;r<256;r++) {
      file_ext[r]=0;
      wild_ext[r]=0;
    }

    Bitu size,elen;
    find_ext=strrchr(file,'.');
    if (find_ext) {
            size=(Bitu)(find_ext-file);
            if (size>255) size=255;
            memcpy(file_name,file,size);
            find_ext++;
            elen=strlen(find_ext);
            memcpy(file_ext,find_ext,(strlen(find_ext)>255) ? 255 : strlen(find_ext));
    } else {
            size=strlen(file);
            elen=0;
            memcpy(file_name,file,(strlen(file) > 255) ? 255 : strlen(file));
    }
    upcase(file_name);upcase(file_ext);
    char nwild[LFN_NAMELENGTH+2];
    strcpy(nwild,wild);
    if (strrchr(nwild,'*')&&strrchr(nwild,'.')==NULL) strcat(nwild,".*");
    find_ext=strrchr(nwild,'.');
    if (find_ext) {
            Bitu size=(Bitu)(find_ext-nwild);
            if (size>255) size=255;
            memcpy(wild_name,nwild,size);
            find_ext++;
            memcpy(wild_ext,find_ext,(strlen(find_ext)>255) ? 255 : strlen(find_ext));
    } else {
            memcpy(wild_name,nwild,(strlen(nwild) > 255) ? 255 : strlen(nwild));
    }
    upcase(wild_name);upcase(wild_ext);
    /* Names are right do some checking */
	if (strchr(wild_name, '*')) {
		if (strchr(wild,'.')) {
			if (!wild_match(file_name, wild_name)) return false;
			goto checkext;
		} else
			return wild_match((char *)file, wild_name);
	} else {
		r=0;
		while (r<size) {
				if (wild_name[r]=='*') goto checkext;
				if (wild_name[r]!='?' && wild_name[r]!=file_name[r]) return false;
				r++;
		}
		if (wild_name[r]&&wild_name[r]!='*') return false;
	}
checkext:
	if (strchr(wild_ext, '*'))
		return wild_match(file_ext, wild_ext);
	else {
		r=0;
		while (r<elen) {
				if (wild_ext[r]=='*') return true;
				if (wild_ext[r]!='?' && wild_ext[r]!=file_ext[r]) return false;
				r++;
		}
		if (wild_ext[r]&&wild_ext[r]!='*') return false;
		return true;
	}
}

std::string To_Label(const char* name) {
	// Reformat the name per the DOS label specification:
	// - Upper-case, up to 11 ASCII characters
	// - Internal spaces allowed but no: tabs ? / \ | . , ; : + = [ ] < > " '
	std::string label(name);
	trim(label); // strip front-and-back white-space
	strip_punctuation(label); // strip all punctuation
	label.resize(11); // collapse remainder to (at-most) 11 chars
	upcase(label);
	return label;
}

void Set_Label(char const * const input, char * const output, bool cdrom) {
	Bitu togo     = 8;
	Bitu vnamePos = 0;
	Bitu labelPos = 0;
	bool point    = false;

	//spacepadding the filenamepart to include spaces after the terminating zero is more closely to the specs. (not doing this now)
	// HELLO\0' '' '

	while (togo > 0) {
		if (input[vnamePos]==0) break;
		if (!point && (input[vnamePos]=='.')) {	togo=4; point=true; }

		//another mscdex quirk. Label is not always uppercase. (Daggerfall)
		output[labelPos] = (cdrom?input[vnamePos]:toupper(input[vnamePos]));

		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) {
			if (input[vnamePos]=='.') vnamePos++;
			output[labelPos]='.'; labelPos++; point=true; togo=3;
		}
	};
	output[labelPos]=0;

	//Remove trailing dot. except when on cdrom and filename is exactly 8 (9 including the dot) letters. MSCDEX feature/bug (fifa96 cdrom detection)
	if((labelPos > 0) && (output[labelPos-1] == '.') && !(cdrom && labelPos ==9))
		output[labelPos-1] = 0;
}

DOS_Drive::DOS_Drive()
	: dirCache()
{
	curdir[0] = '\0';
	info[0] = '\0';
}

// static members variables
int DriveManager::currentDrive;
DriveManager::DriveInfo DriveManager::driveInfos[26];

void DriveManager::AppendDisk(int drive, DOS_Drive* disk) {
	driveInfos[drive].disks.push_back(disk);
}

void DriveManager::InitializeDrive(int drive) {
	currentDrive = drive;
	DriveInfo& driveInfo = driveInfos[currentDrive];
	if (driveInfo.disks.size() > 0) {
		driveInfo.currentDisk = 0;
		DOS_Drive* disk = driveInfo.disks[driveInfo.currentDisk];
		Drives[currentDrive] = disk;
		if (driveInfo.disks.size() > 1) disk->Activate();
	}
}

/*
void DriveManager::CycleDrive(bool pressed) {
	if (!pressed) return;
		
	// do one round through all drives or stop at the next drive with multiple disks
	int oldDrive = currentDrive;
	do {
		currentDrive = (currentDrive + 1) % DOS_DRIVES;
		int numDisks = driveInfos[currentDrive].disks.size();
		if (numDisks > 1) break;
	} while (currentDrive != oldDrive);
}

void DriveManager::CycleDisk(bool pressed) {
	if (!pressed) return;
	
	int numDisks = driveInfos[currentDrive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[currentDrive].currentDisk;
		DOS_Drive* oldDisk = driveInfos[currentDrive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;		
		DOS_Drive* newDisk = driveInfos[currentDrive].disks[currentDisk];
		driveInfos[currentDrive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[currentDrive] = newDisk;
	}
}
*/

void DriveManager::CycleDisks(int drive, bool notify) {
	int numDisks = (int)driveInfos[drive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[drive].currentDisk;
		DOS_Drive* oldDisk = driveInfos[drive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;		
		DOS_Drive* newDisk = driveInfos[drive].disks[currentDisk];
		driveInfos[drive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[drive] = newDisk;
		if (notify) LOG_MSG("Drive %c: disk %d of %d now active", 'A'+drive, currentDisk+1, numDisks);
	}
}

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<DOS_DRIVES; idrive++) CycleDisks(idrive, true);
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;
	// unmanaged drive
	if (driveInfos[drive].disks.size() == 0) {
		result = Drives[drive]->UnMount();
	} else {
		// managed drive
		int currentDisk = driveInfos[drive].currentDisk;
		result = driveInfos[drive].disks[currentDisk]->UnMount();
		// only delete on success, current disk set to NULL because of UnMount
		if (result == 0) {
			driveInfos[drive].disks[currentDisk] = NULL;
			for (int i = 0; i < (int)driveInfos[drive].disks.size(); i++) {
				delete driveInfos[drive].disks[i];
			}
			driveInfos[drive].disks.clear();
		}
	}
	
	return result;
}

void DriveManager::Init(Section* /* sec */) {
	
	// setup driveInfos structure
	currentDrive = 0;
	for(int i = 0; i < DOS_DRIVES; i++) {
		driveInfos[i].currentDisk = 0;
	}
	
//	MAPPER_AddHandler(&CycleDisk, MK_f3, MMOD1, "cycledisk", "Cycle Disk");
//	MAPPER_AddHandler(&CycleDrive, MK_f3, MMOD2, "cycledrive", "Cycle Drv");
}

void DRIVES_Init(Section* sec) {
	DriveManager::Init(sec);
}
