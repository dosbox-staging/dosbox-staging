/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 */

#include "drives.h"

#include <algorithm>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "dos_inc.h"
#include "string_utils.h"
#include "cross.h"
#include "inout.h"
#include "timer.h"
#include "fs_utils.h"
#include "std_filesystem.h"

#define OVERLAY_DIR 1
bool logoverlay = false;
using namespace std;

#if defined (WIN32)
#define CROSS_DOSFILENAME(blah)
#else
//Convert back to DOS PATH
#define	CROSS_DOSFILENAME(blah) strreplace(blah,'/','\\')
#endif

/* 
 * design principles/limitations/requirements:
 * 1) All filenames inside the overlay directories are UPPERCASE and conform to the 8.3 standard except for the special DBOVERLAY files.
 * 2) Renaming directories is currently not supported.
 *
 * Point 2 is still being worked on.
 */

/* New rename for base directories:
 * Alter shortname in the drive_cache: take care of order and long names. 
 * update stored deleted files list in overlay. 
 */

//TODO recheck directories under linux with the filename_cache (as one adds the dos name (and runs cross_filename on the other))


//TODO Check: Maybe handle file redirection in ccc (opening the new file), (call update datetime host there ?)


/* For rename/delete(unlink)/makedir/removedir we need to rebuild the cache. (shouldn't be needed, 
 * but cacheout/delete entry currently throw away the cached folder and rebuild it on read. 
 * so we have to ensure the rebuilding is controlled through the overlay.
 * In order to not reread the overlay directory contents, the information in there is cached and updated when
 * it changes (when deleting a file or adding one)
 */


//directories that exist only in overlay can not be added to the drive_cache currently. 
//Either upgrade addentry to support directories. (without actually caching stuff in! (code in testing))
//Or create an empty directory in local drive base. 

bool Overlay_Drive::RemoveDir(char * dir) {
	//DOS_RemoveDir checks if directory exists.
#if OVERLAY_DIR
	if (logoverlay) LOG_MSG("Overlay: trying to remove directory: %s",dir);
#else
	E_Exit("Overlay: trying to remove directory: %s",dir);
#endif
	/* Overlay: Check if folder is empty (findfirst/next, skipping . and .. and breaking on first file found ?), if so, then it is not too tricky. */
	if (is_dir_only_in_overlay(dir)) {
		//The simple case
		char odir[CROSS_LEN];
		safe_strcpy(odir, overlaydir);
		safe_strcat(odir, dir);
		CROSS_FILENAME(odir);
		int temp = rmdir(odir);
		if (temp == 0) {
			remove_DOSdir_from_cache(dir);
			char newdir[CROSS_LEN];
			safe_strcpy(newdir, basedir);
			safe_strcat(newdir, dir);
			CROSS_FILENAME(newdir);
			dirCache.DeleteEntry(newdir,true);
			update_cache(false);
		}
		return (temp == 0);
	} else {
		uint16_t olderror = dos.errorcode; //FindFirst/Next always set an errorcode, while RemoveDir itself shouldn't touch it if successful
		DOS_DTA dta(dos.tables.tempdta);
		char stardotstar[4] = {'*', '.', '*', 0};
		dta.SetupSearch(0,(0xff & ~DOS_ATTR_VOLUME),stardotstar); //Fake drive as we don't use it.
		bool ret = this->FindFirst(dir,dta,false);// DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			//Path not found. Should not be possible due to removedir doing a testdir, but lets be correct
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}
		bool empty = true;
		do {
			char name[DOS_NAMELENGTH_ASCII];uint32_t size;uint16_t date;uint16_t time;uint8_t attr;
			dta.GetResult(name,size,date,time,attr);
			if (logoverlay) LOG_MSG("RemoveDir found %s",name);
			if (empty && strcmp(".",name ) && strcmp("..",name)) 
				empty = false; //Neither . or .. so directory not empty.
		} while (this->FindNext(dta));
		//Always exhaust list, so drive_cache entry gets invalidated/reused.
		//FindNext is done, restore error code to old value. DOS_RemoveDir will set the right one if needed.
		dos.errorcode = olderror;

		if (!empty) return false;
		if (logoverlay) LOG_MSG("directory empty! Hide it.");
		//Directory is empty, mark it as deleted and create DBOVERLAY file.
		//Ensure that overlap folder can not be created.
		add_deleted_path(dir,true);
		return true;
	}
}

bool Overlay_Drive::MakeDir(char * dir) {
	//DOS_MakeDir tries first, before checking if the directory already exists, so doing it here as well, so that case is handled.
	if (TestDir(dir)) return false;
	if (overlap_folder == dir) return false; //TODO Test
#if OVERLAY_DIR
	if (logoverlay) LOG_MSG("Overlay trying to make directory: %s",dir);
#else
	E_Exit("Overlay trying to make directory: %s",dir);
#endif
	/* Overlay: Create in Overlay only and add it to drive_cache + some entries else the drive_cache will try to access it. Needs an AddEntry for directories. */ 

	//Check if leading dir is marked as deleted.
	if (check_if_leading_is_deleted(dir)) return false;

	//Check if directory itself is marked as deleted
	if (is_deleted_path(dir) && localDrive::TestDir(dir)) {
		//Was deleted before and exists (last one is safety check)
		remove_deleted_path(dir,true);
		return true;
	}
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, overlaydir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	const int temp = create_dir(newdir, 0775);
	if (temp==0) {
		char fakename[CROSS_LEN];
		safe_strcpy(fakename, basedir);
		safe_strcat(fakename, dir);
		CROSS_FILENAME(fakename);
		dirCache.AddEntryDirOverlay(fakename,true);
		add_DOSdir_to_cache(dir);
	}

	return (temp == 0);// || ((temp!=0) && (errno==EEXIST));
}

bool Overlay_Drive::TestDir(char * dir) {
	//First check if directory exist exclusively in the overlay. 
	//Currently using the update_cache cache, alternatively access the directory itself.

	//Directories are stored without a trailing backslash
	char tempdir[CROSS_LEN];
	safe_strcpy(tempdir, dir);
	size_t templen = strlen(dir);
	if (templen && tempdir[templen-1] == '\\') tempdir[templen-1] = 0;

#if OVERLAY_DIR
	if (is_dir_only_in_overlay(tempdir)) return true;
#endif

	//Next Check if the directory is marked as deleted or one of its leading directories is.
	//(it still might exists in the localDrive)

	if (is_deleted_path(tempdir)) return false; 

	// Not exclusive to overlay nor marked as deleted. Pass on to LocalDrive
	return localDrive::TestDir(dir);
}

class OverlayFile final : public localFile {
public:
	OverlayFile(const char *name, FILE *handle, const char *basedir)
	        : localFile(name, handle, basedir),
	          overlay_active(false)
	{
		if (logoverlay)
			LOG_MSG("constructing OverlayFile: %s", name);
	}

	bool Write(uint8_t * data,uint16_t * size) {
		uint32_t f = flags&0xf;
		if (!overlay_active && (f == OPEN_READWRITE || f == OPEN_WRITE)) {
			if (logoverlay) LOG_MSG("write detected, switching file for %s",GetName());
			if (*data == 0) {
				if (logoverlay) LOG_MSG("OPTIMISE: truncate on switch!!!!");
			}
			const auto a = logoverlay ? GetTicks() : 0;
			bool r = create_copy();
			const auto b = logoverlay ? GetTicksSince(a) : 0;
			if (b > 2)
				LOG_MSG("OPTIMISE: switching took %d", b);
			if (!r) return false;
			overlay_active = true;
			
		}
		return localFile::Write(data,size);
	}
	bool create_copy();
//private:
	bool overlay_active;
};

//Create leading directories of a file being overlayed if they exist in the original (localDrive).
//This function is used to create copies of existing files, so all leading directories exist in the original.

FILE *Overlay_Drive::create_file_in_overlay(const char *dos_filename, char const *mode)
{
	if (logoverlay) LOG_MSG("create_file_in_overlay called %s %s",dos_filename,mode);
	char newname[CROSS_LEN];
	safe_strcpy(newname, overlaydir); // TODO GOG make part of class and
	                                  // join in
	safe_strcat(newname, dos_filename); // HERE we need to convert it to
	                                    // Linux TODO
	CROSS_FILENAME(newname);

	FILE* f = fopen_wrap(newname,mode);
	//Check if a directories are part of the name:
	const char *dir = strrchr(dos_filename, '\\');

	if (!f && dir && *dir) {
		if (logoverlay) LOG_MSG("Overlay: warning creating a file inside a directory %s",dos_filename);
		//ensure they exist, else make them in the overlay if they exist in the original....
		Sync_leading_dirs(dos_filename);
		//try again
		f = fopen_wrap(newname,mode);
	}

	return f;
}

#ifndef BUFSIZ
#define BUFSIZ 2048
#endif

bool OverlayFile::create_copy() {
	//test if open/valid/etc
	//ensure file position
	if (logoverlay) LOG_MSG("create_copy called %s",GetName());

	FILE* lhandle = this->fhandle;
	assert(lhandle);

	const auto lhandle_pos = ftell(lhandle);
	if (lhandle_pos < 0) {
		LOG_ERR("OVERLAY: Failed getting current position in file '%s': %s",
		        GetName(), strerror(errno));
		fclose(lhandle);
		return false;
	}
	if (fseek(lhandle, lhandle_pos, SEEK_SET) != 0) {
		LOG_ERR("OVERLAY: Failed seeking to position %ld in file '%s': %s",
		        lhandle_pos, GetName(), strerror(errno));
		fclose(lhandle);
		return false;
	}

	const auto location_in_old_file = ftell(lhandle);
	if (location_in_old_file < 0) {
		LOG_ERR("OVERLAY: Failed getting current position in file '%s': %s",
		        GetName(), strerror(errno));
		fclose(lhandle);
		return false;
	}
	if (fseek(lhandle, 0L, SEEK_SET) != 0) {
		LOG_ERR("OVERLAY: Failed seeking to the beginning of file '%s': %s",
		        GetName(), strerror(errno));
		fclose(lhandle);
		return false;
	}

	FILE* newhandle = NULL;
	uint8_t drive_set = GetDrive();
	if (drive_set != 0xff && drive_set < DOS_DRIVES && Drives[drive_set]){
		Overlay_Drive* od = dynamic_cast<Overlay_Drive*>(Drives[drive_set]);
		if (od) {
			newhandle = od->create_file_in_overlay(GetName(),"wb+"); //todo check wb+
		}
	}
 
	if (!newhandle) return false;
	char buffer[BUFSIZ];
	size_t s;
	while ( (s = fread(buffer,1,BUFSIZ,lhandle)) != 0 ) fwrite(buffer, 1, s, newhandle);
	fclose(lhandle);

	//Set copied file handle to position of the old one
	if (fseek(newhandle, location_in_old_file, SEEK_SET) != 0) {
		LOG_ERR("OVERLAY: Failed seeking to position %ld in file '%s': %s",
		        location_in_old_file, GetName(), strerror(errno));
		fclose(newhandle);
		return false;
	}
	this->fhandle = newhandle;
	//Flags ?
	if (logoverlay) LOG_MSG("success");
	return true;
}



static OverlayFile* ccc(DOS_File* file) {
	localFile* l = dynamic_cast<localFile*>(file);
	if (!l) E_Exit("overlay input file is not a localFile");
	//Create an overlayFile
	OverlayFile *ret = new OverlayFile(l->GetName(), l->fhandle,
	                                   l->GetBaseDir());
	ret->flags = l->flags;
	ret->refCtr = l->refCtr;
	delete l;
	return ret;
}

Overlay_Drive::Overlay_Drive(const char *startdir,
                             const char *overlay,
                             uint16_t _bytes_sector,
                             uint8_t _sectors_cluster,
                             uint16_t _total_clusters,
                             uint16_t _free_clusters,
                             uint8_t _mediaid,
                             uint8_t &error)
        : localDrive(startdir,
                     _bytes_sector,
                     _sectors_cluster,
                     _total_clusters,
                     _free_clusters,
                     _mediaid),
          deleted_files_in_base{},
          deleted_paths_in_base{},
          overlap_folder(),
          DOSnames_cache{},
          DOSdirs_cache{},
          special_prefix("DBOVERLAY")
{
	//Currently this flag does nothing, as the current behavior is to not reread due to caching everything.
#if defined (WIN32)	
	if (strcasecmp(startdir,overlay) == 0) {
#else 
	if (strcmp(startdir,overlay) == 0) {
#endif
		//overlay directory can not be the base directory
		error = 2;
		return;
	}

	std::string s(startdir);
	std::string o(overlay);
	bool s_absolute = Cross::IsPathAbsolute(s);
	bool o_absolute = Cross::IsPathAbsolute(o);
	error = 0;
	if (s_absolute != o_absolute) { 
		error = 1;
		return;
	}
	safe_strcpy(overlaydir, overlay);
	char dirname[CROSS_LEN] = { 0 };
	//Determine if overlaydir is part of the startdir.
	convert_overlay_to_DOSname_in_base(dirname);

	size_t dirlen = safe_strlen(dirname);
	if(dirlen && dirname[dirlen - 1] == '\\') dirname[dirlen - 1] = 0;
			
	//add_deleted_path(dirname); //update_cache will add the overlap_folder
	overlap_folder = dirname;

	update_cache(true);
}

void Overlay_Drive::convert_overlay_to_DOSname_in_base(char* dirname ) 
{
	dirname[0] = 0;//ensure good return string
	if (safe_strlen(overlaydir) >= safe_strlen(basedir) ) {
		//Needs to be longer at least.
#if defined (WIN32)
		if (strncasecmp(overlaydir,basedir,strlen(basedir)) == 0) {
#else
		if (strncmp(overlaydir,basedir,strlen(basedir)) == 0) {
#endif
			//Beginning is the same.
			char t[CROSS_LEN];
			safe_strcpy(t, overlaydir + safe_strlen(basedir));

			char* p = t;
			char* b = t;

			while ( (p =strchr(p,CROSS_FILESPLIT)) ) {
				char directoryname[CROSS_LEN]={0};
				char dosboxdirname[CROSS_LEN]={0};
				safe_strcpy(directoryname, dirname);
				assert(p >= b);
				strncat(directoryname, b,
				        static_cast<size_t>(p - b));

				char d[CROSS_LEN];
				safe_strcpy(d, basedir);
				safe_strcat(d, directoryname);
				CROSS_FILENAME(d);
				//Try to find the corresponding directoryname in DOSBox.
				if(!dirCache.GetShortName(d,dosboxdirname) ) {
					//Not a long name, assume it is a short name instead
					assert(p >= b);
					strncpy(dosboxdirname, b,
					        static_cast<size_t>(p - b));
					upcase(dosboxdirname);
				}


				strcat(dirname,dosboxdirname);
				strcat(dirname,"\\");

				if (logoverlay) LOG_MSG("HIDE directory: %s",dirname);


				b=++p;

			}
		}
	}
}

bool Overlay_Drive::FileOpen(DOS_File * * file,char * name,uint32_t flags) {
	const char* type;
	switch (flags&0xf) {
	case OPEN_READ:        type = "rb" ; break;
	case OPEN_WRITE:       type = "rb+"; break;
	case OPEN_READWRITE:   type = "rb+"; break;
	case OPEN_READ_NO_MOD: type = "rb" ; break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}

	//Flush the buffer of handles for the same file. (Betrayal in Antara)
	uint8_t drive = DOS_DRIVES;
	for (uint8_t i = 0; i < DOS_DRIVES; ++i) {
		if (Drives[i]==this) {
			drive=i;
			break;
		}
	}
	for (uint8_t i = 0; i < DOS_FILES; ++i) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive()==drive && Files[i]->IsName(name)) {
			localFile *lfp = dynamic_cast<localFile *>(Files[i]);
			if (lfp) lfp->Flush();
		}
	}

	//Todo check name first against local tree
	//if name exists, use that one instead!
	//overlay file.
	char newname[CROSS_LEN];
	safe_strcpy(newname, overlaydir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);

	FILE * hand = fopen_wrap(newname,type);
	bool fileopened = false;
	if (hand) {
		if (logoverlay) LOG_MSG("overlay file opened %s",newname);
		*file = new localFile(name, hand, overlaydir);
		(*file)->flags=flags;
		fileopened = true;
	} else {
		; //TODO error handling!!!! (maybe check if it exists and read only (should not happen with overlays)
	}
	bool overlayed = fileopened;

	//File not present in overlay, try normal drive
	//TODO take care of file being marked deleted.

	if (!fileopened && !is_deleted_file(name)) fileopened = localDrive::FileOpen(file,name, OPEN_READ);


	if (fileopened) {
		if (logoverlay) LOG_MSG("file opened %s",name);
		//Convert file to OverlayFile
		OverlayFile* f = ccc(*file);
		f->flags = flags; //ccc copies the flags of the localfile, which were not correct in this case
		f->overlay_active = overlayed; //No need to switch if already in overlayed.
		*file = f;
	}
	return fileopened;
}


bool Overlay_Drive::FileCreate(DOS_File * * file,char * name,uint16_t /*attributes*/) {
	//TODO Check if it exists in the dirCache ? // fix addentry ?  or just double check (ld and overlay)
	//AddEntry looks sound to me.. 
	
	//check if leading part of filename is a deleted directory
	if (check_if_leading_is_deleted(name)) return false;

	FILE* f = create_file_in_overlay(name,"wb+");
	if(!f) {
		if (logoverlay) LOG_MSG("File creation in overlay system failed %s",name);
		return false;
	}
	*file = new localFile(name, f, overlaydir);
	(*file)->flags = OPEN_READWRITE;
	OverlayFile* of = ccc(*file);
	of->overlay_active = true;
	of->flags = OPEN_READWRITE;
	*file = of;
	//create fake name for the drive cache
	char fakename[CROSS_LEN];
	safe_strcpy(fakename, overlaydir);
	safe_strcat(fakename, name);
	CROSS_FILENAME(fakename);
	dirCache.AddEntry(fakename,true); //add it.
	add_DOSname_to_cache(name);
	remove_deleted_file(name,true);
	return true;
}
void Overlay_Drive::add_DOSname_to_cache(const char* name) {
	for (std::vector<std::string>::const_iterator itc = DOSnames_cache.begin(); itc != DOSnames_cache.end(); ++itc){
		if (name == (*itc)) return;
	}
	DOSnames_cache.push_back(name);
}
void Overlay_Drive::remove_DOSname_from_cache(const char* name) {
	for (std::vector<std::string>::iterator it = DOSnames_cache.begin(); it != DOSnames_cache.end(); ++it) {
		if (name == (*it)) { DOSnames_cache.erase(it); return;}
	}

}

bool Overlay_Drive::Sync_leading_dirs(const char* dos_filename){
	const char* lastdir = strrchr(dos_filename,'\\');
	//If there are no directories, return success.
	if (!lastdir) return true; 
	
	const char* leaddir = dos_filename;
	while ( (leaddir=strchr(leaddir,'\\')) != 0) {
		char dirname[CROSS_LEN] = {0};

		assert(leaddir >= dos_filename);
		strncpy(dirname, dos_filename,
		        static_cast<size_t>(leaddir - dos_filename));

		if (logoverlay) LOG_MSG("syncdir: %s",dirname);
		//Test if directory exist in base.
		char dirnamebase[CROSS_LEN] ={0};
		safe_strcpy(dirnamebase, basedir);
		safe_strcat(dirnamebase, dirname);
		CROSS_FILENAME(dirnamebase);
		struct stat basetest;
		if (stat(dirCache.GetExpandName(dirnamebase),&basetest) == 0 && basetest.st_mode & S_IFDIR) {
			if (logoverlay) LOG_MSG("base exists: %s",dirnamebase);
			//Directory exists in base folder.
			//Ensure it exists in overlay as well

			struct stat overlaytest;
			char dirnameoverlay[CROSS_LEN] ={0};
			safe_strcpy(dirnameoverlay, overlaydir);
			safe_strcat(dirnameoverlay, dirname);
			CROSS_FILENAME(dirnameoverlay);
			if (stat(dirnameoverlay,&overlaytest) == 0 ) {
				//item exist. Check if it is a folder, if not a folder =>fail!
				if ((overlaytest.st_mode & S_IFDIR) ==0) return false;
			} else {
				//folder does not exist, make it
				if (logoverlay) LOG_MSG("creating %s",dirnameoverlay);
				if (create_dir(dirnameoverlay, 0700) != 0)
					return false;
			}
		}
		leaddir = leaddir + 1; //Move to next
	} 

	return true;
}
void Overlay_Drive::update_cache(bool read_directory_contents) {
	const auto a = logoverlay ? GetTicks() : 0;
	std::vector<std::string> specials;
	std::vector<std::string> dirnames;
	std::vector<std::string> filenames;
	if (read_directory_contents) {
		//Clear all lists
		DOSnames_cache.clear();
		DOSdirs_cache.clear();
		deleted_files_in_base.clear();
		deleted_paths_in_base.clear();
		//Ensure hiding of the folder that contains the overlay, if it is part of the base folder.
		add_deleted_path(overlap_folder.c_str(), false);
	}

	//Needs later to support stored renames and removals of files existing in the localDrive plane.
	//and by taking in account if the file names are actually already renamed. 
	//and taking in account that a file could have gotten an overlay version and then both need to be removed. 
	//
	//Also what about sequences were a base file gets copied to a working save game and then removed/renamed...
	//copy should be safe as then the link with the original doesn't exist.
	//however the working safe can be rather complicated after a rename and delete..

	//Currently directories existing only in the overlay can not be added to drive cache:
	//1. possible workaround create empty directory in base. Drawback would break the no-touching-of-base.
	//2. double up Addentry to support directories, (and adding . and .. to the newly directory so it counts as cachedin.. and won't be recached, as otherwise
	//   cache will realize we are faking it. 
	//Working on solution 2.

	//Random TODO: Does the root drive under DOS have . and .. ? 

	//This function needs to be called after any localDrive function calling cacheout/deleteentry, as those throw away directories.
	//either do this with a parameter stating the part that needs to be rebuild,(directory) or clear the cache by default and do it all.

	std::vector<std::string>::iterator i;
	std::string::size_type const prefix_lengh = special_prefix.length();
	if (read_directory_contents) {
		dir_information* dirp = open_directory(overlaydir);
		if (dirp == NULL) return;
		// Read complete directory
		char dir_name[CROSS_LEN];
		bool is_directory;
		if (read_directory_first(dirp, dir_name, is_directory)) {
			if ((safe_strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(dir_name);
			else if (is_directory) dirnames.push_back(dir_name);
			else filenames.push_back(dir_name);
			while (read_directory_next(dirp, dir_name, is_directory)) {
				if ((safe_strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(dir_name);
				else if (is_directory) dirnames.push_back(dir_name);
				else filenames.push_back(dir_name);
			}
		}
		close_directory(dirp);
		dirp = nullptr;

		// parse directories to add them.
		for (i = dirnames.begin(); i != dirnames.end(); ++i) {
			if ((*i) == ".") continue;
			if ((*i) == "..") continue;
			std::string testi(*i);
			std::string::size_type ll = testi.length();
			//TODO: Use the dirname\. and dirname\.. for creating fake directories in the driveCache.
			if( ll >2 && testi[ll-1] == '.' && testi[ll-2] == CROSS_FILESPLIT) continue; 
			if( ll >3 && testi[ll-1] == '.' && testi[ll-2] == '.' && testi[ll-3] == CROSS_FILESPLIT) continue;

#if OVERLAY_DIR
			char tdir[CROSS_LEN];
			safe_strcpy(tdir, (*i).c_str());
			CROSS_DOSFILENAME(tdir);
			bool dir_exists_in_base = localDrive::TestDir(tdir);
#endif

			char dir[CROSS_LEN];
			safe_strcpy(dir, overlaydir);
			safe_strcat(dir, (*i).c_str());
			char dirpush[CROSS_LEN];
			safe_strcpy(dirpush, (*i).c_str());
			static char end[2] = {CROSS_FILESPLIT,0};
			safe_strcat(dirpush, end); // Linux ?

			assert(dirp == nullptr);
			dirp = open_directory(dir);
			if (dirp == NULL) continue;

#if OVERLAY_DIR
			//Good directory, add to DOSdirs_cache if not existing in localDrive. tested earlier to prevent problems with opendir
			if (!dir_exists_in_base) add_DOSdir_to_cache(tdir);
#endif

			std::string backupi(*i);
			// Read complete directory
			if (read_directory_first(dirp, dir_name, is_directory)) {
				if ((safe_strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(string(dirpush)+dir_name);
				else if (is_directory) dirnames.push_back(string(dirpush)+dir_name);
				else filenames.push_back(string(dirpush)+dir_name);
				while (read_directory_next(dirp, dir_name, is_directory)) {
					if ((safe_strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(string(dirpush)+dir_name);
					else if (is_directory) dirnames.push_back(string(dirpush)+dir_name);
					else filenames.push_back(string(dirpush)+dir_name);
				}
			}
			close_directory(dirp);
			dirp = nullptr;

			// find current directory again, for the next round. But
			// if it's not there, then bail out before the next
			// round to avoid incrementing beyond the end.
			i = std::find(dirnames.begin(), dirnames.end(), backupi);
			if (i == dirnames.end())
				break;
		}
	}


	if (read_directory_contents) {
		for( i = filenames.begin(); i != filenames.end(); ++i) {
			char dosname[CROSS_LEN];
			safe_strcpy(dosname, (*i).c_str());
			upcase(dosname);  //Should not be really needed, as uppercase in the overlay is a requirement...
			CROSS_DOSFILENAME(dosname);
			if (logoverlay) LOG_MSG("update cache add dosname %s",dosname);
			DOSnames_cache.push_back(dosname);
		}
	}

#if OVERLAY_DIR
	for (i = DOSdirs_cache.begin(); i !=DOSdirs_cache.end(); ++i) {
		char fakename[CROSS_LEN];
		safe_strcpy(fakename, basedir);
		safe_strcat(fakename, (*i).c_str());
		CROSS_FILENAME(fakename);
		dirCache.AddEntryDirOverlay(fakename,true);
	}
#endif

	for (i = DOSnames_cache.begin(); i != DOSnames_cache.end(); ++i) {
		char fakename[CROSS_LEN];
		safe_strcpy(fakename, basedir);
		safe_strcat(fakename, (*i).c_str());
		CROSS_FILENAME(fakename);
		dirCache.AddEntry(fakename,true);
	}

	if (read_directory_contents) {
		for (i = specials.begin(); i != specials.end(); ++i) {
			//Specials look like this DBOVERLAY_YYY_FILENAME.EXT or DIRNAME[\/]DBOVERLAY_YYY_FILENAME.EXT where 
			//YYY is the operation involved. Currently only DEL is supported.
			//DEL = file marked as deleted, (but exists in localDrive!)
			std::string name(*i);
			std::string special_dir("");
			std::string special_file("");
			std::string special_operation("");
			std::string::size_type s = name.find(special_prefix);
			if (s == std::string::npos) continue;
			if (s) {
				special_dir = name.substr(0,s);
				name.erase(0,s);
			}
			name.erase(0,special_prefix.length()+1); //Erase DBOVERLAY_
			s = name.find('_');
			if (s == std::string::npos || s == 0) continue;
			special_operation = name.substr(0,s);
			name.erase(0,s + 1);
			special_file = name;
			if (special_file.length() == 0) continue;
			if (special_operation == "DEL") {
				name = special_dir + special_file;
				//CROSS_DOSFILENAME for strings:
				while ( (s = name.find('/')) != std::string::npos) name.replace(s,1,"\\");
				
				add_deleted_file(name.c_str(),false);
			} else if (special_operation == "RMD") {
				name = special_dir + special_file;
				//CROSS_DOSFILENAME for strings:
				while ( (s = name.find('/')) != std::string::npos) name.replace(s,1,"\\");
				add_deleted_path(name.c_str(),false);

			} else {
				if (logoverlay) LOG_MSG("unsupported operation %s on %s",special_operation.c_str(),(*i).c_str());
			}

		}
	}
	if (logoverlay)
		LOG_MSG("OPTIMISE: update cache took %d", GetTicksSince(a));
}

bool Overlay_Drive::FindNext(DOS_DTA & dta) {

	char * dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];
	char dir_entcopy[CROSS_LEN];

	uint8_t srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	uint8_t find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	uint16_t id = dta.GetDirID();

again:
	if (!dirCache.FindNext(id,dir_ent)) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if(!WildFileCmp(dir_ent,srch_pattern)) goto again;

	safe_strcpy(full_name, srchInfo[id].srch_dir);
	safe_strcat(full_name, dir_ent);

	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory 
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	safe_strcpy(dir_entcopy, dir_ent);

	//First try overlay:
	char ovname[CROSS_LEN];
	char relativename[CROSS_LEN];
	safe_strcpy(relativename, srchInfo[id].srch_dir);
	//strip off basedir: //TODO cleanup
	safe_strcpy(ovname, overlaydir);
	char* prel = full_name + safe_strlen(basedir);

	

#if 0
	//Check hidden/deleted directories first. TODO is this really needed. If the directory exist in the overlay things are weird anyway.
	//the deleted paths are added to the deleted_files list.
	if (is_deleted_dir(prel)) {
		LOG_MSG("skipping early out deleted dir %s",prel);
		goto again;
	}
#endif

	safe_strcat(ovname, prel);
	bool statok = ( stat(ovname,&stat_block)==0);

	if (logoverlay) LOG_MSG("listing %s",dir_entcopy);
	if (statok) {
		if (logoverlay) LOG_MSG("using overlay data for %s : %s",full_name, ovname);
	} else {
		char preldos[CROSS_LEN];
		safe_strcpy(preldos, prel);
		CROSS_DOSFILENAME(preldos);
		if (is_deleted_file(preldos)) { //dir.. maybe lower or keep it as is TODO
			if (logoverlay) LOG_MSG("skipping deleted file %s %s %s",preldos,full_name,ovname);
			goto again;
		}
		if (stat(dirCache.GetExpandName(full_name),&stat_block)!=0) {
			if (logoverlay) LOG_MSG("stat failed for %s . This should not happen.",dirCache.GetExpandName(full_name));
			goto again;//No symlinks and such
		}
	}

	if(stat_block.st_mode & S_IFDIR) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;

	/* file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII] = {};
	uint16_t find_date                   = 0;
	uint16_t find_time                   = 0;
	uint32_t find_size                   = 0;

	if(safe_strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII){
		safe_strcpy(find_name, dir_entcopy);
		upcase(find_name);
	} 

	find_size=(uint32_t) stat_block.st_size;
	struct tm datetime;
	if (cross::localtime_r(&stat_block.st_mtime, &datetime)) {
		find_date = DOS_PackDate(datetime);
		find_time = DOS_PackTime(datetime);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,find_size,find_date,find_time,find_attr);
	return true;
}



bool Overlay_Drive::FileUnlink(char * name) {
	// TODO check the basedir for file existence in order if we need to add the file to deleted file list.
	const auto a = logoverlay ? GetTicks() : 0;
	if (logoverlay)
		LOG_MSG("calling unlink on %s", name);
	char basename[CROSS_LEN];
	safe_strcpy(basename, basedir);
	safe_strcat(basename, name);
	CROSS_FILENAME(basename);

	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name);
	CROSS_FILENAME(overlayname);
//	char *fullname = dirCache.GetExpandName(newname);
	if (unlink(overlayname)) {
		//Unlink failed for some reason try finding it.
		struct stat buffer;
		if(stat(overlayname,&buffer)) {
			//file not found in overlay, check the basedrive
			//Check if file not already deleted 
			if (is_deleted_file(name)) {
				DOS_SetError(DOSERR_FILE_NOT_FOUND);
				return false;
			}


			char *fullname = dirCache.GetExpandName(basename);
			if (stat(fullname,&buffer)) {
				DOS_SetError(DOSERR_FILE_NOT_FOUND);
				return false; // File not found in either, return file false.
			}
			//File does exist in normal drive.
			//Maybe do something with the drive_cache.
			add_deleted_file(name,true);
			return true;
//			E_Exit("trying to remove existing non-overlay file %s",name);
		}

		//Do we have access?
		FILE* file_writable = fopen_wrap(overlayname,"rb+");
		if(!file_writable) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		fclose(file_writable);

		//File exists and can technically be deleted, nevertheless it failed.
		//This means that the file is probably open by some process.
		//See if We have it open.
		bool found_file = false;
		for (uint8_t i = 0; i < DOS_FILES; ++i) {
			if(Files[i] && Files[i]->IsName(name)) {
				uint8_t max = DOS_FILES;
				while(Files[i]->IsOpen() && max--) {
					Files[i]->Close();
					if (Files[i]->RemoveRef()<=0) break;
				}
				found_file=true;
			}
		}
		if(!found_file) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		if (std_fs::remove(overlayname)) {
			// Overlay file removed, mark basefile as deleted if it
			// exists:
			if (localDrive::FileExists(name))
				add_deleted_file(name, true);
			remove_DOSname_from_cache(name); // Should be an else ?
			                                 // although better safe
			                                 // than sorry.
			// Handle this better
			dirCache.DeleteEntry(basename);
			update_cache(false);
			//Check if it exists in the base dir as well
			
			return true;
		}
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	} else { //Removed from overlay.
		//TODO IF it exists in the basedir: and more locations above.
		if (localDrive::FileExists(name)) add_deleted_file(name,true);
		remove_DOSname_from_cache(name);
		//TODODO remove from the update_cache cache as well
		//Handle this better
		//Check if it exists in the base dir as well
		dirCache.DeleteEntry(basename);

		update_cache(false);
		if (logoverlay)
			LOG_MSG("OPTIMISE: unlink took %d", GetTicksSince(a));
		return true;
	}
}

bool Overlay_Drive::GetFileAttr(char *name, uint16_t *attr)
{
	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name);
	CROSS_FILENAME(overlayname);

	struct stat status;
	if (stat(overlayname, &status) == 0) {
		*attr = DOS_ATTR_ARCHIVE;
		if (status.st_mode & S_IFDIR)
			*attr |= DOS_ATTR_DIRECTORY;
		return true;
	}
	// Maybe check for deleted path as well
	if (is_deleted_file(name)) {
		*attr = 0;
		return false;
	}
	return localDrive::GetFileAttr(name, attr);
}

bool Overlay_Drive::SetFileAttr(const char *name, uint16_t /*attr*/)
{
	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name);
	CROSS_FILENAME(overlayname);

	struct stat status;
	if (stat(overlayname, &status) == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return true;
	}
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
}

void Overlay_Drive::add_deleted_file(const char* name,bool create_on_disk) {
	if (logoverlay) LOG_MSG("add del file %s",name);
	if (!is_deleted_file(name)) {
		deleted_files_in_base.push_back(name);
		if (create_on_disk) add_special_file_to_disk(name, "DEL");

	}
}

void Overlay_Drive::add_special_file_to_disk(const char* dosname, const char* operation) {
	std::string name = create_filename_of_special_operation(dosname, operation);
	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name.c_str());
	CROSS_FILENAME(overlayname);
	FILE* f = fopen_wrap(overlayname,"wb+");
	if (!f) {
		Sync_leading_dirs(dosname);
		f = fopen_wrap(overlayname,"wb+");
	}
	if (!f) E_Exit("Failed creation of %s",overlayname);
	char buf[5] = {'e','m','p','t','y'};
	fwrite(buf,5,1,f);
	fclose(f);
}

void Overlay_Drive::remove_special_file_from_disk(const char* dosname, const char* operation) {
	std::string name = create_filename_of_special_operation(dosname,operation);
	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name.c_str());
	CROSS_FILENAME(overlayname);
	if(unlink(overlayname) != 0) E_Exit("Failed removal of %s",overlayname);
}

std::string Overlay_Drive::create_filename_of_special_operation(const char* dosname, const char* operation) {
	std::string res(dosname);
	std::string::size_type s = res.rfind('\\'); //CHECK DOS or host endings.... on update_cache
	if (s == std::string::npos) s = 0; else s++;
	std::string oper = special_prefix + "_" + operation + "_";
	res.insert(s,oper);
	return res;
}


bool Overlay_Drive::is_dir_only_in_overlay(const char* name) {
	if (!name || !*name) return false;
	if (DOSdirs_cache.empty()) return false;
	for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); ++it) {
		if (*it == name) return true;
	}
	return false;
}

bool Overlay_Drive::is_deleted_file(const char* name) {
	if (!name || !*name) return false;
	if (deleted_files_in_base.empty()) return false;
	for(std::vector<std::string>::iterator it = deleted_files_in_base.begin(); it != deleted_files_in_base.end(); ++it) {
		if (*it == name) return true;
	}
	return false;
}

void Overlay_Drive::add_DOSdir_to_cache(const char* name) {
	if (!name || !*name ) return; //Skip empty file.
	LOG_MSG("Adding name to overlay_only_dir_cache %s",name);
	if (!is_dir_only_in_overlay(name)) {
		DOSdirs_cache.push_back(name); 
	}
}

void Overlay_Drive::remove_DOSdir_from_cache(const char* name) {
	for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); ++it) {
		if ( *it == name) {
			DOSdirs_cache.erase(it);
			return;
		}
	}
}

void Overlay_Drive::remove_deleted_file(const char* name,bool create_on_disk) {
	for(std::vector<std::string>::iterator it = deleted_files_in_base.begin(); it != deleted_files_in_base.end(); ++it) {
		if (*it == name) {
			deleted_files_in_base.erase(it);
			if (create_on_disk) remove_special_file_from_disk(name, "DEL");
			return;
		}
	}
}
void Overlay_Drive::add_deleted_path(const char* name, bool create_on_disk) {
	if (!name || !*name ) return; //Skip empty file.
	if (logoverlay) LOG_MSG("add del path %s",name);
	if (!is_deleted_path(name)) {
		deleted_paths_in_base.push_back(name);
		//Add it to deleted files as well, so it gets skipped in FindNext. 
		//Maybe revise that.
		if (create_on_disk) add_special_file_to_disk(name,"RMD");
		add_deleted_file(name,false);
	}
}
bool Overlay_Drive::is_deleted_path(const char* name) {
	if (!name || !*name) return false;
	if (deleted_paths_in_base.empty()) return false;
	std::string sname(name);
	std::string::size_type namelen = sname.length();;
	for(std::vector<std::string>::iterator it = deleted_paths_in_base.begin(); it != deleted_paths_in_base.end(); ++it) {
		std::string::size_type blockedlen = (*it).length();
		if (namelen < blockedlen) continue;
		//See if input starts with name. 
		std::string::size_type n = sname.find(*it);
		if (n == 0 && ((namelen == blockedlen) || *(name + blockedlen) == '\\' )) return true;
	}
	return false;
}

void Overlay_Drive::remove_deleted_path(const char* name, bool create_on_disk) {
	for(std::vector<std::string>::iterator it = deleted_paths_in_base.begin(); it != deleted_paths_in_base.end(); ++it) {
		if (*it == name) {
			deleted_paths_in_base.erase(it);
			remove_deleted_file(name,false); //Rethink maybe.
			if (create_on_disk) remove_special_file_from_disk(name,"RMD");
			break;
		}
	}
}
bool Overlay_Drive::check_if_leading_is_deleted(const char* name){
	const char* dname = strrchr(name,'\\');
	if (dname != NULL) {
		char dirname[CROSS_LEN];

		assert(dname >= name);
		strncpy(dirname, name, static_cast<size_t>(dname - name));
		dirname[dname - name] = 0;
		if (is_deleted_path(dirname)) return true;
	}
	return false;
}

bool Overlay_Drive::FileExists(const char* name) {
	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name);
	CROSS_FILENAME(overlayname);
	struct stat temp_stat;
	if(stat(overlayname,&temp_stat)==0 && (temp_stat.st_mode & S_IFDIR)==0) return true;
	
	if (is_deleted_file(name)) return false;

	return localDrive::FileExists(name);
}

#if 1
bool Overlay_Drive::Rename(char * oldname,char * newname) {
	//TODO with cache function!
	//Tricky function.
	//Renaming directories is currently not supported, due the drive_cache not handling that smoothly.
	//So oldname is directory => Exit!
	//If oldname is on overlay => simple rename.
	//if oldname is on base => copy file to overlay with new name and mark old file as deleted. 
	//More advanced version. keep track of the file being renamed in order to detect that the file is being renamed back. 
	
	uint16_t attr = 0;
	if (!GetFileAttr(oldname,&attr)) E_Exit("rename, but source doesn't exist, should not happen %s",oldname);
	if (attr&DOS_ATTR_DIRECTORY) {
		//See if the directory exists only in the overlay, then it should be possible.
#if OVERLAY_DIR
		if (localDrive::TestDir(oldname)) E_Exit("Overlay: renaming base directory %s to %s not yet supported", oldname,newname);
#endif
		E_Exit("renaming directory %s to %s . Not yet supported in Overlay",oldname,newname); //TODO
	}

	const auto a = logoverlay ? GetTicks() : 0;
	//First generate overlay names.
	char overlaynameold[CROSS_LEN];
	safe_strcpy(overlaynameold, overlaydir);
	safe_strcat(overlaynameold, oldname);
	CROSS_FILENAME(overlaynameold);

	char overlaynamenew[CROSS_LEN];
	safe_strcpy(overlaynamenew, overlaydir);
	safe_strcat(overlaynamenew, newname);
	CROSS_FILENAME(overlaynamenew);

	//No need to check if the original is marked as deleted, as GetFileAttr would fail if it did.

	bool result = false;

	// check if overlaynameold exista and if so rename it to overlaynamenew
	if (std_fs::exists(overlaynameold)) {
		std_fs::rename(overlaynameold, overlaynamenew);
		result = true; // indicate that the rename succeeded

		// Overlay file renamed: mark the old base file as deleted.
		if (localDrive::FileExists(oldname)) {
			add_deleted_file(oldname, true);
		}
	} else {
		const auto aa = logoverlay ? GetTicks() : 0;
		//File exists in the basedrive. Make a copy and mark old one as deleted.
		char newold[CROSS_LEN];
		safe_strcpy(newold, basedir);
		safe_strcat(newold, oldname);
		CROSS_FILENAME(newold);
		dirCache.ExpandName(newold);
		FILE* o = fopen_wrap(newold,"rb");
		if (!o) return false;
		FILE* n = create_file_in_overlay(newname,"wb+");
		if (!n) {fclose(o); return false;}
		char buffer[BUFSIZ];
		size_t s;
		while ( (s = fread(buffer,1,BUFSIZ,o)) ) fwrite(buffer, 1, s, n);
		fclose(o); fclose(n);

		//File copied.
		//Mark old file as deleted
		add_deleted_file(oldname,true);
		result = true; //success
		if (logoverlay)
			LOG_MSG("OPTIMISE: update rename with copy took %d",
			        GetTicksSince(aa));
	}
	if (result) {
		//handle the drive_cache (a bit better)
		//Ensure that the file is not marked as deleted anymore.
		if (is_deleted_file(newname)) remove_deleted_file(newname,true);
		dirCache.EmptyCache();
		update_cache(true);
		if (logoverlay)
			LOG_MSG("OPTIMISE: rename took %d", GetTicksSince(a));
	}
	return result;

}
#endif

bool Overlay_Drive::FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	if (logoverlay) LOG_MSG("FindFirst in %s",_dir);
	
	if (is_deleted_path(_dir)) {
		//No accidental listing of files in there.
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}

	return localDrive::FindFirst(_dir,dta,fcb_findfirst);
}

bool Overlay_Drive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char overlayname[CROSS_LEN];
	safe_strcpy(overlayname, overlaydir);
	safe_strcat(overlayname, name);
	CROSS_FILENAME(overlayname);
	struct stat temp_stat;
	if(stat(overlayname,&temp_stat) != 0) {
		if (is_deleted_file(name)) return false;
		return localDrive::FileStat(name,stat_block);
	}
	/* Convert the stat to a FileStat */
	struct tm datetime;
	if (cross::localtime_r(&temp_stat.st_mtime, &datetime)) {
		stat_block->time = DOS_PackTime(datetime);
		stat_block->date = DOS_PackDate(datetime);
	} else {
		LOG_MSG("OVERLAY: Error while converting date in: %s", name);
	}
	stat_block->size=(uint32_t)temp_stat.st_size;
	return true;
}

Bits Overlay_Drive::UnMount(void) { 
	delete this;
	return 0; 
}
void Overlay_Drive::EmptyCache(void){
	localDrive::EmptyCache();
	update_cache(true);//lets rebuild it.
}

