/*
 *  Copyright (C) 2002-2004  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: setup.h,v 1.15 2004-01-29 09:26:43 qbix79 Exp $ */

#ifndef _SETUP_H_
#define _SETUP_H_

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif

#include "cross.h"
#include <string>
#include <list>

class CommandLine {
public:
	CommandLine(int argc,char * argv[]);
	CommandLine(char * name,char * cmdline);
	const char * GetFileName(){ return file_name.c_str();}	
    
	bool FindExist(char * name,bool remove=false);
	bool FindHex(char * name,int & value,bool remove=false);
	bool FindInt(char * name,int & value,bool remove=false);
	bool FindString(char * name,std::string & value,bool remove=false);
	bool FindCommand(unsigned int which,std::string & value);
	bool FindStringBegin(char * begin,std::string & value, bool remove=false);
	bool FindStringRemain(char * name,std::string & value);
	unsigned int GetCount(void);
private:
	typedef std::list<std::string>::iterator cmd_it;
	std::list<std::string> cmds;
	std::string file_name;
	bool FindEntry(char * name,cmd_it & it,bool neednext=false);
};

union Value{
	int _hex;
	bool _bool;
	int _int;
	std::string* _string;
	float _float;   
};

class Property {
public:
	Property(const char* _propname):propname(_propname) { }
	virtual void SetValue(char* input)=0;
	virtual void GetValuestring(char* str)=0;
	Value GetValue() { return __value;}
	std::string propname;
	Value __value;
	virtual ~Property(){ }
};

class Prop_int:public Property {
public:
	Prop_int(const char* _propname, int _value):Property(_propname) { 
		__value._int=_value;
	}
	void SetValue(char* input);
        void GetValuestring(char* str);
	~Prop_int(){ }
};
class Prop_float:public Property {
public:
	Prop_float(const char* _propname, float _value):Property(_propname){
		__value._float=_value;
	}
	void SetValue(char* input);
	void GetValuestring(char* str);
	~Prop_float(){ }
};

class Prop_bool:public Property {
public:
	Prop_bool(const char* _propname, bool _value):Property(_propname) { 
		__value._bool=_value;
	}
	void SetValue(char* input);
        void GetValuestring(char* str);
	~Prop_bool(){ }
};

class Prop_string:public Property{
public:
	Prop_string(const char* _propname, char* _value):Property(_propname) { 
		__value._string=new std::string(_value);
	}
	~Prop_string(){
		delete __value._string;
	}
	void SetValue(char* input);
        void GetValuestring(char* str);
};
class Prop_hex:public Property {
public:
	Prop_hex(const char* _propname, int _value):Property(_propname) { 
		__value._hex=_value;
	}
	void SetValue(char* input);
	~Prop_hex(){ }
        void GetValuestring(char* str);
};

class Section {
public:
	Section(const char* _sectionname) { sectionname=_sectionname; }
	virtual ~Section(){ ExecuteDestroy();}

	typedef void (*SectionFunction)(Section*);
	void AddInitFunction(SectionFunction func) {initfunctions.push_back(func);}
	void AddDestroyFunction(SectionFunction func) {destroyfunctions.push_front(func);}
	void ExecuteInit() { 
		typedef std::list<SectionFunction>::iterator func_it;
		for (func_it tel=initfunctions.begin(); tel!=initfunctions.end(); tel++){ 
			(*tel)(this);
		}
	}
	void ExecuteDestroy() { 
		typedef std::list<SectionFunction>::iterator func_it;
		for (func_it tel=destroyfunctions.begin(); tel!=destroyfunctions.end(); tel++){ 
			(*tel)(this);
		}
	}
	std::list<SectionFunction> initfunctions;
	std::list<SectionFunction> destroyfunctions;
	virtual void HandleInputline(char * _line){}
	virtual void PrintData(FILE* outfile) {}
	std::string sectionname;   
};


class Section_prop:public Section {
 public:
	Section_prop(const char* _sectionname):Section(_sectionname){}

	void Add_int(const char* _propname, int _value=0);
	void Add_string(const char* _propname, char* _value=NULL);
	void Add_bool(const char* _propname, bool _value=false);
	void Add_hex(const char* _propname, int _value=0);
	void Add_float(const char* _propname, float _value=0.0);   

	int Get_int(const char* _propname);
	const char* Get_string(const char* _propname);
	bool Get_bool(const char* _propname);
	int Get_hex(const char* _propname);
	float Get_float(const char* _propname);
	void HandleInputline(char *gegevens);
	void PrintData(FILE* outfile);
   
	std::list<Property*> properties;
	typedef std::list<Property*>::iterator it;
};

class Section_line: public Section{
public:
	Section_line(const char* _sectionname):Section(_sectionname){}
	void HandleInputline(char* gegevens);
	void PrintData(FILE* outfile);
	std::string data;
};

class Config{
public:
	Config(CommandLine * cmd){ cmdline=cmd;}
	~Config();
	CommandLine * cmdline;

	Section * AddSection(const char * _name,void (*_initfunction)(Section*));
	Section_line * AddSection_line(const char * _name,void (*_initfunction)(Section*));
	Section_prop * AddSection_prop(const char * _name,void (*_initfunction)(Section*));
	
	Section* GetSection(const char* _sectionname);

	void SetStartUp(void (*_function)(void));
	void Init();
	void ShutDown();
	void StartUp();
	void PrintConfig(const char* configfilename);
	bool ParseConfigFile(const char* configfilename);
	void ParseEnv(char ** envp);

	std::list<Section*> sectionlist;
	typedef std::list<Section*>::iterator it;
	typedef std::list<Section*>::reverse_iterator reverse_it;
private:
	void (* _start_function)(void);
};

#endif
