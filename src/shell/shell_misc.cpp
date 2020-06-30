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
 */

#include "shell.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include "regs.h"
#include "callback.h"
#include "support.h"

void DOS_Shell::ShowPrompt(void) {
	Bit8u drive=DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	dir[0] = 0; //DOS_GetCurrentDir doesn't always return something. (if drive is messed up)
	DOS_GetCurrentDir(0,dir);
	InjectMissingNewline();
	WriteOut("%c:\\%s>", drive, dir);
	ResetLastWrittenChar('\n'); // prevents excessive newline if cmd prints nothing
}

static void outc(Bit8u c) {
	Bit16u n=1;
	DOS_WriteFile(STDOUT,&c,&n);
}

void DOS_Shell::InputCommand(char * line) {
	Bitu size=CMD_MAXLINE-2; //lastcharacter+0
	Bit8u c;Bit16u n=1;
	Bitu str_len=0;Bitu str_index=0;
	Bit16u len=0;
	bool current_hist=false; // current command stored in history?

	line[0] = '\0';

	std::list<std::string>::iterator it_history = l_history.begin(), it_completion = l_completion.begin();

	while (size) {
		dos.echo=false;
		while(!DOS_ReadFile(input_handle,&c,&n)) {
			Bit16u dummy;
			DOS_CloseFile(input_handle);
			DOS_OpenFile("con",2,&dummy);
			LOG(LOG_MISC,LOG_ERROR)("Reopening the input handle. This is a bug!");
		}
		if (!n) {
			size=0;			//Kill the while loop
			continue;
		}
		switch (c) {
		case 0x00:				/* Extended Keys */
			{
				DOS_ReadFile(input_handle,&c,&n);
				switch (c) {

				case 0x3d:		/* F3 */
					if (!l_history.size()) break;
					it_history = l_history.begin();
					if (it_history != l_history.end() && it_history->length() > str_len) {
						const char *reader = &(it_history->c_str())[str_len];
						while ((c = *reader++)) {
							line[str_index ++] = c;
							DOS_WriteFile(STDOUT,&c,&n);
						}
						str_len = str_index = (Bitu)it_history->length();
						size = CMD_MAXLINE - str_index - 2;
						line[str_len] = 0;
					}
					break;

				case 0x4B:	/* LEFT */
					if (str_index) {
						outc(8);
						str_index --;
					}
					break;

				case 0x4D:	/* RIGHT */
					if (str_index < str_len) {
						outc(line[str_index++]);
					}
					break;

				case 0x47:	/* HOME */
					while (str_index) {
						outc(8);
						str_index--;
					}
					break;

				case 0x4F:	/* END */
					while (str_index < str_len) {
						outc(line[str_index++]);
					}
					break;

				case 0x48:	/* UP */
					if (l_history.empty() || it_history == l_history.end()) break;

					// store current command in history if we are at beginning
					if (it_history == l_history.begin() && !current_hist) {
						current_hist=true;
						l_history.push_front(line);
					}

					for (;str_index>0; str_index--) {
						// removes all characters
						outc(8); outc(' '); outc(8);
					}
					strcpy(line, it_history->c_str());
					len = (Bit16u)it_history->length();
					str_len = str_index = len;
					size = CMD_MAXLINE - str_index - 2;
					DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
					it_history ++;
					break;

				case 0x50:	/* DOWN */
					if (l_history.empty() || it_history == l_history.begin()) break;

					// not very nice but works ..
					it_history --;
					if (it_history == l_history.begin()) {
						// no previous commands in history
						it_history ++;

						// remove current command from history
						if (current_hist) {
							current_hist=false;
							l_history.pop_front();
						}
						break;
					} else it_history --;

					for (;str_index>0; str_index--) {
						// removes all characters
						outc(8); outc(' '); outc(8);
					}
					strcpy(line, it_history->c_str());
					len = (Bit16u)it_history->length();
					str_len = str_index = len;
					size = CMD_MAXLINE - str_index - 2;
					DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
					it_history ++;

					break;
				case 0x53:/* DELETE */
					{
						if(str_index>=str_len) break;
						Bit16u a=str_len-str_index-1;
						Bit8u* text=reinterpret_cast<Bit8u*>(&line[str_index+1]);
						DOS_WriteFile(STDOUT,text,&a);//write buffer to screen
						outc(' ');outc(8);
						for(Bitu i=str_index;i<str_len-1;i++) {
							line[i]=line[i+1];
							outc(8);
						}
						line[--str_len]=0;
						size++;
					}
					break;
				case 15:		/* Shift-Tab */
					if (l_completion.size()) {
						if (it_completion == l_completion.begin()) it_completion = l_completion.end (); 
						it_completion--;
		
						if (it_completion->length()) {
							for (;str_index > completion_index; str_index--) {
								// removes all characters
								outc(8); outc(' '); outc(8);
							}

							strcpy(&line[completion_index], it_completion->c_str());
							len = (Bit16u)it_completion->length();
							str_len = str_index = completion_index + len;
							size = CMD_MAXLINE - str_index - 2;
							DOS_WriteFile(STDOUT, (Bit8u *)it_completion->c_str(), &len);
						}
					}
				default:
					break;
				}
			};
			break;
		case 0x08:				/* BackSpace */
			if (str_index) {
				outc(8);
				Bit32u str_remain=str_len - str_index;
				size++;
				if (str_remain) {
					memmove(&line[str_index-1],&line[str_index],str_remain);
					line[--str_len]=0;
					str_index --;
					/* Go back to redraw */
					for (Bit16u i=str_index; i < str_len; i++)
						outc(line[i]);
				} else {
					line[--str_index] = '\0';
					str_len--;
				}
				outc(' ');	outc(8);
				// moves the cursor left
				while (str_remain--) outc(8);
			}
			if (l_completion.size()) l_completion.clear();
			break;
		case 0x0a:				/* New Line not handled */
			/* Don't care */
			break;
		case 0x0d:				/* Return */
			outc('\r');
			outc('\n');
			size=0;			//Kill the while loop
			break;
		case'\t':
			{
				if (l_completion.size()) {
					it_completion ++;
					if (it_completion == l_completion.end()) it_completion = l_completion.begin();
				} else {
					// build new completion list
					// Lines starting with CD will only get directories in the list
					bool dir_only = (strncasecmp(line,"CD ",3)==0);

					// get completion mask
					char *p_completion_start = strrchr(line, ' ');

					if (p_completion_start) {
						p_completion_start ++;
						completion_index = (Bit16u)(str_len - strlen(p_completion_start));
					} else {
						p_completion_start = line;
						completion_index = 0;
					}

					char *path;
					if ((path = strrchr(line+completion_index,'\\'))) completion_index = (Bit16u)(path-line+1);
					if ((path = strrchr(line+completion_index,'/'))) completion_index = (Bit16u)(path-line+1);

					// build the completion list
					char mask[DOS_PATHLENGTH] = {0};
					if (p_completion_start) {
						if (strlen(p_completion_start) + 3 >= DOS_PATHLENGTH) {
							//Beep;
							break;
						}
						safe_strncpy(mask, p_completion_start,DOS_PATHLENGTH);
						char* dot_pos=strrchr(mask,'.');
						char* bs_pos=strrchr(mask,'\\');
						char* fs_pos=strrchr(mask,'/');
						char* cl_pos=strrchr(mask,':');
						// not perfect when line already contains wildcards, but works
						if ((dot_pos-bs_pos>0) && (dot_pos-fs_pos>0) && (dot_pos-cl_pos>0))
							strncat(mask, "*",DOS_PATHLENGTH - 1);
						else strncat(mask, "*.*",DOS_PATHLENGTH - 1);
					} else {
					        safe_strcpy(mask, "*.*");
				        }

					RealPt save_dta=dos.dta();
					dos.dta(dos.tables.tempdta);

					bool res = DOS_FindFirst(mask, 0xffff & ~DOS_ATTR_VOLUME);
					if (!res) {
						dos.dta(save_dta);
						break;	// TODO: beep
					}

					DOS_DTA dta(dos.dta());
					char name[DOS_NAMELENGTH_ASCII];Bit32u sz;Bit16u date;Bit16u time;Bit8u att;

					std::list<std::string> executable;
					while (res) {
						dta.GetResult(name,sz,date,time,att);
						// add result to completion list

						if (strcmp(name, ".") && strcmp(name, "..")) {
							if (dir_only) { //Handle the dir only case different (line starts with cd)
								if(att & DOS_ATTR_DIRECTORY) l_completion.push_back(name);
							} else {
								if (is_executable(std::string(name)))
									// we add executables to the a seperate list and place that list infront of the normal files
									executable.push_front(name);
								else
									l_completion.push_back(name);
							}
						}
						res=DOS_FindNext();
					}
					/* Add executable list to front of completion list. */
					std::copy(executable.begin(),executable.end(),std::front_inserter(l_completion));
					it_completion = l_completion.begin();
					dos.dta(save_dta);
				}

				if (l_completion.size() && it_completion->length()) {
					for (;str_index > completion_index; str_index--) {
						// removes all characters
						outc(8); outc(' '); outc(8);
					}

					strcpy(&line[completion_index], it_completion->c_str());
					len = (Bit16u)it_completion->length();
					str_len = str_index = completion_index + len;
					size = CMD_MAXLINE - str_index - 2;
					DOS_WriteFile(STDOUT, (Bit8u *)it_completion->c_str(), &len);
				}
			}
			break;
		case 0x1b:   /* ESC */
			//write a backslash and return to the next line
			outc('\\');
			outc('\r');
			outc('\n');
			*line = 0;      // reset the line.
			if (l_completion.size()) l_completion.clear(); //reset the completion list.
			this->InputCommand(line);	//Get the NEW line.
			size = 0;       // stop the next loop
			str_len = 0;    // prevent multiple adds of the same line
			break;
		default:
			if (l_completion.size()) l_completion.clear();
			if(str_index < str_len && true) { //mem_readb(BIOS_KEYBOARD_FLAGS1)&0x80) dev_con.h ?
				outc(' ');//move cursor one to the right.
				Bit16u a = str_len - str_index;
				Bit8u* text=reinterpret_cast<Bit8u*>(&line[str_index]);
				DOS_WriteFile(STDOUT,text,&a);//write buffer to screen
				outc(8);//undo the cursor the right.
				for(Bitu i=str_len;i>str_index;i--) {
					line[i]=line[i-1]; //move internal buffer
					outc(8); //move cursor back (from write buffer to screen)
				}
				line[++str_len]=0;//new end (as the internal buffer moved one place to the right
				size--;
			};
		   
			line[str_index]=c;
			str_index ++;
			if (str_index > str_len){ 
				line[str_index] = '\0';
				str_len++;
				size--;
			}
			DOS_WriteFile(STDOUT,&c,&n);
			break;
		}
	}

	if (!str_len) return;
	str_len++;

	// remove current command from history if it's there
	if (current_hist) {
		// current_hist=false;
		l_history.pop_front();
	}

	// add command line to history
	l_history.push_front(line); it_history = l_history.begin();
	if (l_completion.size()) l_completion.clear();
}

std::string full_arguments = "";
bool DOS_Shell::Execute(char * name,char * args) {
/* return true  => don't check for hardware changes in do_command 
 * return false =>       check for hardware changes in do_command */
	char fullname[DOS_PATHLENGTH+4]; //stores results from Which
	char line[CMD_MAXLINE];
	if(strlen(args)!= 0){
		if(*args != ' '){ //put a space in front
			line[0]=' ';line[1]=0;
			strncat(line,args,CMD_MAXLINE-2);
			line[CMD_MAXLINE-1]=0;
		}
		else
		{
			safe_strncpy(line,args,CMD_MAXLINE);
		}
	}else{
		line[0]=0;
	};

	/* check for a drive change */
	if (((strcmp(name + 1, ":") == 0) || (strcmp(name + 1, ":\\") == 0)) && isalpha(*name))
	{
		if (!DOS_SetDrive(toupper(name[0])-'A')) {
			WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0]));
		}
		return true;
	}

	/* Check for a full name */
	const char *p_fullname = Which(name);
	if (!p_fullname)
		return false;
	safe_strcpy(fullname, p_fullname);

	// Always disallow files without extension from being executed.
	// Only internal commands can be run this way and they never get in
	// this handler.
	const char *extension = strrchr(fullname, '.');
	if (!extension) {
		// Check if the result will fit in the parameters.
		if (strlen(fullname) > (DOS_PATHLENGTH - 1))
			return false;

		// Try to add .COM, .EXE and .BAT extensions to the filename
		char temp_name[DOS_PATHLENGTH + 4];
		for (const char *ext : {".COM", ".EXE", ".BAT"}) {
			snprintf(temp_name, sizeof(temp_name), "%s%s", fullname, ext);
			const char *temp_fullname = Which(temp_name);
			if (temp_fullname) {
				extension = ext;
				safe_strcpy(fullname, temp_fullname);
				break;
			}
		}

		if (!extension)
			return false;
	}

	if (strcasecmp(extension, ".bat") == 0) 
	{	/* Run the .bat file */
		/* delete old batch file if call is not active*/
		bool temp_echo=echo; /*keep the current echostate (as delete bf might change it )*/
		if(bf && !call) delete bf;
		bf=new BatchFile(this,fullname,name,line);
		echo=temp_echo; //restore it.
	} 
	else 
	{	/* only .bat .exe .com extensions maybe be executed by the shell */
		if(strcasecmp(extension, ".com") !=0) 
		{
			if(strcasecmp(extension, ".exe") !=0) return false;
		}
		/* Run the .exe or .com file from the shell */
		/* Allocate some stack space for tables in physical memory */
		reg_sp-=0x200;
		//Add Parameter block
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);
		block.Clear();
		//Add a filename
		RealPt file_name=RealMakeSeg(ss,reg_sp+0x20);
		MEM_BlockWrite(Real2Phys(file_name),fullname,(Bitu)(strlen(fullname)+1));

		/* HACK: Store full commandline for mount and imgmount */
		full_arguments.assign(line);

		/* Fill the command line */
		CommandTail cmdtail;
		cmdtail.count = 0;
		memset(&cmdtail.buffer,0,127); //Else some part of the string is unitialized (valgrind)
		if (strlen(line)>126) line[126]=0;
		cmdtail.count=(Bit8u)strlen(line);
		memcpy(cmdtail.buffer,line,strlen(line));
		cmdtail.buffer[strlen(line)]=0xd;
		/* Copy command line in stack block too */
		MEM_BlockWrite(SegPhys(ss)+reg_sp+0x100,&cmdtail,128);

		
		/* Split input line up into parameters, using a few special rules, most notable the one for /AAA => A\0AA
		 * Qbix: It is extremly messy, but this was the only way I could get things like /:aa and :/aa to work correctly */
		
		//Prepare string first
		char parseline[258] = { 0 };
		for(char *pl = line,*q = parseline; *pl ;pl++,q++) {
			if (*pl == '=' || *pl == ';' || *pl ==',' || *pl == '\t' || *pl == ' ') *q = 0; else *q = *pl; //Replace command seperators with 0.
		} //No end of string \0 needed as parseline is larger than line

		for(char* p = parseline; (p-parseline) < 250 ;p++) { //Stay relaxed within boundaries as we have plenty of room
			if (*p == '/') { //Transform /Hello into H\0ello
				*p = 0;
				p++;
				while ( *p == 0 && (p-parseline) < 250) p++; //Skip empty fields
				if ((p-parseline) < 250) { //Found something. Lets get the first letter and break it up
					p++;
					memmove(static_cast<void*>(p + 1),static_cast<void*>(p),(250-(p-parseline)));
					if ((p-parseline) < 250) *p = 0;
				}
			}
		}
		parseline[255] = parseline[256] = parseline[257] = 0; //Just to be safe.

		/* Parse FCB (first two parameters) and put them into the current DOS_PSP */
		Bit8u add;
		Bit16u skip = 0;
		//find first argument, we end up at parseline[256] if there is only one argument (similar for the second), which exists and is 0.
		while(skip < 256 && parseline[skip] == 0) skip++;
		FCB_Parsename(dos.psp(),0x5C,0x01,parseline + skip,&add);
		skip += add;
		
		//Move to next argument if it exists
		while(parseline[skip] != 0) skip++;  //This is safe as there is always a 0 in parseline at the end.
		while(skip < 256 && parseline[skip] == 0) skip++; //Which is higher than 256
		FCB_Parsename(dos.psp(),0x6C,0x01,parseline + skip,&add); 

		block.exec.fcb1=RealMake(dos.psp(),0x5C);
		block.exec.fcb2=RealMake(dos.psp(),0x6C);
		/* Set the command line in the block and save it */
		block.exec.cmdtail=RealMakeSeg(ss,reg_sp+0x100);
		block.SaveData();
#if 0
		/* Save CS:IP to some point where i can return them from */
		Bit32u oldeip=reg_eip;
		Bit16u oldcs=SegValue(cs);
		RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
		SegSet16(cs,RealSeg(newcsip));
		reg_ip=RealOff(newcsip);
#endif
		/* Start up a dos execute interrupt */
		reg_ax=0x4b00;
		//Filename pointer
		SegSet16(ds,SegValue(ss));
		reg_dx=RealOff(file_name);
		//Paramblock
		SegSet16(es,SegValue(ss));
		reg_bx=reg_sp;
		SETFLAGBIT(IF,false);
		CALLBACK_RunRealInt(0x21);
		/* Restore CS:IP and the stack */
		reg_sp+=0x200;
#if 0
		reg_eip=oldeip;
		SegSet16(cs,oldcs);
#endif
	}
	return true; //Executable started
}

static char which_ret[DOS_PATHLENGTH+4];

const char *DOS_Shell::Which(const char *name) const
{
	const size_t name_len = strlen(name);
	if (name_len >= DOS_PATHLENGTH)
		return 0;

	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */

	if (DOS_FileExists(name))
		return name;

	/* try to find .com .exe .bat */
	for (const char *ext_fmt : {"%s.COM", "%s.EXE", "%s.BAT"}) {
		snprintf(which_ret, sizeof(which_ret), ext_fmt, name);
		if (DOS_FileExists(which_ret))
			return which_ret;
	}

	/* No Path in filename look through path environment string */
	char path[DOS_PATHLENGTH];std::string temp;
	if (!GetEnvStr("PATH",temp)) return 0;
	const char * pathenv=temp.c_str();
	if (!pathenv) return 0;
	pathenv = strchr(pathenv,'=');
	if (!pathenv) return 0;
	pathenv++;
	Bitu i_path = 0;
	while (*pathenv) {
		/* remove ; and ;; at the beginning. (and from the second entry etc) */
		while(*pathenv == ';')
			pathenv++;

		/* get next entry */
		i_path = 0; /* reset writer */
		while(*pathenv && (*pathenv !=';') && (i_path < DOS_PATHLENGTH) )
			path[i_path++] = *pathenv++;

		if(i_path == DOS_PATHLENGTH) {
			/* If max size. move till next ; and terminate path */
			while(*pathenv && (*pathenv != ';')) 
				pathenv++;
			path[DOS_PATHLENGTH - 1] = 0;
		} else path[i_path] = 0;


		/* check entry */
		if(size_t len = strlen(path)){
			if(len >= (DOS_PATHLENGTH - 2)) continue;

			if(path[len - 1] != '\\') {
				safe_strcat(path, "\\");
				len++;
			}

			//If name too long =>next
			if((name_len + len + 1) >= DOS_PATHLENGTH) continue;

			safe_strcat(path, name);
			safe_strcpy(which_ret, path);
			if (DOS_FileExists(which_ret))
				return which_ret;

			for (const char *ext_fmt : {"%s.COM", "%s.EXE", "%s.BAT"}) {
				snprintf(which_ret, sizeof(which_ret), ext_fmt, path);
				if (DOS_FileExists(which_ret))
					return which_ret;
			}
		}
	}
	return 0;
}
