/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: setup.h,v 1.26 2007-01-08 19:45:37 qbix79 Exp $ */

#ifndef DOSBOX_SETUP_H
#define DOSBOX_SETUP_H

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif

#ifndef DOSBOX_CROSS_H
#include "cross.h"
#endif
#include <string>
#include <list>

class CommandLine {
public:
	CommandLine(int argc,char const * const argv[]);
	CommandLine(char const * const name,char const * const cmdline);
	const char * GetFileName(){ return file_name.c_str();}	

	bool FindExist(char const * const name,bool remove=false);
	bool FindHex(char const * const name,int & value,bool remove=false);
	bool FindInt(char const * const name,int & value,bool remove=false);
	bool FindString(char const * const name,std::string & value,bool remove=false);
	bool FindCommand(unsigned int which,std::string & value);
	bool FindStringBegin(char const * const begin,std::string & value, bool remove=false);
	bool FindStringRemain(char const * const name,std::string & value);
	bool GetStringRemain(std::string & value);
	unsigned int GetCount(void);
	void Shift(unsigned int amount=1);
private:
	typedef std::list<std::string>::iterator cmd_it;
	std::list<std::string> cmds;
	std::string file_name;
	bool FindEntry(char const * const name,cmd_it & it,bool neednext=false);
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
	Property(char const * const _propname):propname(_propname) { }
	virtual void SetValue(char* input)=0;
	virtual void GetValuestring(char* str) const=0;
	Value GetValue() const { return value;}
	virtual ~Property(){ }
	std::string propname;
	Value value;
};

class Prop_int:public Property {
public:
	Prop_int(char const * const  _propname, int _value):Property(_propname) { 
		value._int=_value;
	}
	void SetValue(char* input);
        void GetValuestring(char* str) const;
	~Prop_int(){ }
};
class Prop_float:public Property {
public:
	Prop_float(char const * const _propname, float _value):Property(_propname){
		value._float=_value;
	}
	void SetValue(char* input);
	void GetValuestring(char* str) const;
	~Prop_float(){ }
};

class Prop_bool:public Property {
public:
	Prop_bool(char const * const _propname, bool _value):Property(_propname) { 
		value._bool=_value;
	}
	void SetValue(char* input);
        void GetValuestring(char* str) const;
	~Prop_bool(){ }
};

class Prop_string:public Property{
public:
	Prop_string(char const * const _propname, char const * const _value):Property(_propname) { 
		value._string=new std::string(_value);
	}
	~Prop_string(){
		delete value._string;
	}
	void SetValue(char* input);
        void GetValuestring(char* str) const;
};
class Prop_hex:public Property {
public:
	Prop_hex(char const * const  _propname, int _value):Property(_propname) { 
		value._hex=_value;
	}
	void SetValue(char* input);
	~Prop_hex(){ }
	void GetValuestring(char* str) const;
};

class Section {
private:
	typedef void (*SectionFunction)(Section*);
	/* Wrapper class around startup and shutdown functions. the variable
	 * canchange indicates it can be called on configuration changes */
	struct Function_wrapper {
		SectionFunction function;
		bool canchange;
		Function_wrapper(SectionFunction const _fun,bool _ch){
			function=_fun;
			canchange=_ch;
		}
	};
	std::list<Function_wrapper> initfunctions;
	std::list<Function_wrapper> destroyfunctions;
	std::string sectionname;
public:
	Section(char const * const _sectionname):sectionname(_sectionname) {  }

	void AddInitFunction(SectionFunction func,bool canchange=false) {initfunctions.push_back(Function_wrapper(func,canchange));}
	void AddDestroyFunction(SectionFunction func,bool canchange=false) {destroyfunctions.push_front(Function_wrapper(func,canchange));}
	void ExecuteInit(bool initall=true);
	void ExecuteDestroy(bool destroyall=true);
	const char* GetName() const {return sectionname.c_str();}

	virtual char const * GetPropValue(char const * const _property) const =0;
	virtual void HandleInputline(char * _line)=0;
	virtual void PrintData(FILE* outfile) const =0;
	virtual ~Section() { /*Children must call executedestroy ! */}
};


class Section_prop:public Section {
private:
	std::list<Property*> properties;
	typedef std::list<Property*>::iterator it;
	typedef std::list<Property*>::const_iterator const_it;
public:
	Section_prop(char const * const  _sectionname):Section(_sectionname){}
	void Add_int(char const *  const _propname, int _value=0);
	void Add_string(char const * const _propname, char const * const _value=NULL);
	void Add_bool(char const * const _propname, bool _value=false);
	void Add_hex(char const * const _propname, int _value=0);
	void Add_float(char const * const _propname, float _value=0.0);   

	int Get_int(char const * const _propname) const;
	const char* Get_string(char const * const _propname) const;
	bool Get_bool(char const * const _propname) const;
	int Get_hex(char const * const _propname) const;
	float Get_float(char const * const _propname) const;
	void HandleInputline(char *gegevens);
	void PrintData(FILE* outfile) const;
	virtual char const * GetPropValue(char const * const _property) const;
	//ExecuteDestroy should be here else the destroy functions use destroyed properties
	virtual ~Section_prop();
};

class Section_line: public Section{
public:
	Section_line(char const * const _sectionname):Section(_sectionname){}
	~Section_line(){ExecuteDestroy(true);}
	void HandleInputline(char* gegevens);
	void PrintData(FILE* outfile) const;
	virtual const char* GetPropValue(char const * const _property) const;
	std::string data;
};

class Config{
public:
	CommandLine * cmdline;
private:
	std::list<Section*> sectionlist;
	typedef std::list<Section*>::iterator it;
	typedef std::list<Section*>::reverse_iterator reverse_it;
	typedef std::list<Section*>::const_iterator const_it;
	typedef std::list<Section*>::const_reverse_iterator const_reverse_it;
	void (* _start_function)(void);
public:
	Config(CommandLine * cmd):cmdline(cmd){}
	~Config();

	Section_line * AddSection_line(char const * const _name,void (*_initfunction)(Section*));
	Section_prop * AddSection_prop(char const * const _name,void (*_initfunction)(Section*),bool canchange=false);
	
	Section* GetSection(char const* const _sectionname) const;
	Section* GetSectionFromProperty(char const * const prop) const;

	void SetStartUp(void (*_function)(void));
	void Init();
	void ShutDown();
	void StartUp();
	void PrintConfig(char const * const configfilename) const;
	bool ParseConfigFile(char const * const configfilename);
	void ParseEnv(char ** envp);
};

class Module_base {
	/* Base for all hardware and software "devices" */
protected:
	Section* m_configuration;
public:
	Module_base(Section* configuration){m_configuration=configuration;};
//	Module_base(Section* configuration, SaveState* state) {};
	virtual ~Module_base(){/*LOG_MSG("executed")*/;};//Destructors are required
	/* Returns true if succesful.*/
	virtual bool Change_Config(Section* /*newconfig*/) {return false;} ;
};
#endif
