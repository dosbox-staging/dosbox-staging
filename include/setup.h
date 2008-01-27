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

/* $Id: setup.h,v 1.31 2008-01-27 18:31:01 qbix79 Exp $ */

#ifndef DOSBOX_SETUP_H
#define DOSBOX_SETUP_H

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif

#ifndef DOSBOX_CROSS_H
#include "cross.h"
#endif
#ifndef DOSBOX_PROGRAMS_H
#include "programs.h"
#endif

#ifndef CH_LIST
#define CH_LIST
#include <list>
#endif

#ifndef CH_VECTOR
#define CH_VECTOR
#include <vector>
#endif

#ifndef CH_STRING
#define CH_STRING
#include <string>
#endif

#pragma warning(disable: 4290)

class Hex {
private:
	int _hex;
public:
	Hex(int in):_hex(in) { };
	Hex():_hex(0) { };
	bool operator==(Hex const& other) {return _hex == other._hex;}
};

class Value {
public: //for the time being. Added a n to _hex to keep compilable during the work
	Hex _hexn;
	int _hex;
	bool _bool;
	int _int;
	std::string* _string;
	double _double;
public:
	class WrongType { }; // Conversion error class
	enum { V_NONE, V_HEX, V_BOOL, V_INT, V_STRING, V_DOUBLE} type;
	
	/* Constructors */
	Value()                      :type(V_NONE)                                 { };
	Value(Hex in)                :type(V_HEX),    _hexn(in)                    { };
	Value(int in)                :type(V_INT),    _int(in)                     { };
	Value(bool in)               :type(V_BOOL),   _bool(in)                    { };
	Value(double in)             :type(V_DOUBLE), _double(in)                  { };
	Value(std::string const& in) :type(V_STRING), _string(new std::string(in)) { };
	Value(char const * const in) :type(V_STRING), _string(new std::string(in)) { };
	Value(Value const& in) {plaincopy(in);}
	~Value() { destroy();};
	
	/* Assigment operators */
	Value& operator= (Hex in) throw(WrongType)                { return copy(Value(in));}
	Value& operator= (int in) throw(WrongType)                { return copy(Value(in));}
	Value& operator= (bool in) throw(WrongType)               { return copy(Value(in));}
	Value& operator= (double in) throw(WrongType)             { return copy(Value(in));}
	Value& operator= (std::string const& in) throw(WrongType) { return copy(Value(in));}
	Value& operator= (char const * const in) throw(WrongType) { return copy(Value(in));}
	Value& operator= (Value const& in) throw(WrongType)       { return copy(Value(in));}

	bool operator== (Value const & other);
	operator bool () const throw(WrongType);
	operator Hex () const throw(WrongType);
	operator int () const throw(WrongType);
	operator double () const throw(WrongType);
	operator char const* () const throw(WrongType);

private:
	void destroy();
	Value& copy(Value const& in) throw(WrongType);
	void plaincopy(Value const& in);

};

class Property {
public:
	struct Changable { enum Value {Always, WhenIdle,OnlyAtStart};};
	Property(char const * const _propname):propname(_propname) { }
	virtual void SetValue(char* input)=0;
	virtual void GetValuestring(char* str) const=0;
	Value const& GetValue() const { return value;}
	virtual ~Property(){ }
	std::string propname;
	//CheckValue returns true (and sets value to in) if value is in suggested_values;
	//Type specific properties are encouraged to override this and check for type
	//specific features.
	virtual bool CheckValue(Value const& in, bool warn);
	//Set interval value to in or default if in is invalid. force always sets the value.
	void SetVal(Value const& in, bool forced) {if(forced || CheckValue(in,false)) value = in; else value = default_value;}
protected:
	Value value;
	std::vector<Value> suggested_values;
	typedef std::vector<Value>::iterator iter;
	Value default_value;
	/*const*/ Changable::Value change;
};

class Prop_int:public Property {
public:
	Prop_int(char const * const  _propname, int _value):Property(_propname) { 
		default_value = value = _value;
		min = max = -1;
	}
	void SetMinMax(Value const& min,Value const& max) {this->min = min; this->max=max;}
	void SetValue(char* input);
	void GetValuestring(char* str) const;
	~Prop_int(){ }
	virtual bool CheckValue(Value const& in, bool warn);
private:
	Value min,max;
};
class Prop_double:public Property {
public:
	Prop_double(char const * const _propname, double _value):Property(_propname){
		default_value = value = _value;
	}
	void SetValue(char* input);
	void GetValuestring(char* str) const;
	~Prop_double(){ }
};

class Prop_bool:public Property {
public:
	Prop_bool(char const * const _propname, bool _value):Property(_propname) { 
		default_value = value = _value;
	}
	void SetValue(char* input);
	void GetValuestring(char* str) const;
	~Prop_bool(){ }
};

class Prop_string:public Property{
public:
	Prop_string(char const * const _propname, char const * const _value):Property(_propname) { 
		default_value = value = _value;
	}
	void SetValue(char* input);
	void GetValuestring(char* str) const;
	~Prop_string(){ }

};
class Prop_hex:public Property {
public:
	Prop_hex(char const * const  _propname, int _value):Property(_propname) { 
		default_value = value._hex = _value;
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

	void AddInitFunction(SectionFunction func,bool canchange=false);
	void AddDestroyFunction(SectionFunction func,bool canchange=false);
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
	Prop_int& Add_int(char const *  const _propname, int _value=0);
	Prop_string& Add_string(char const * const _propname, char const * const _value=NULL);
	Prop_bool&  Add_bool(char const * const _propname, bool _value=false);
	Prop_hex& Add_hex(char const * const _propname, int _value=0);
	void Add_double(char const * const _propname, double _value=0.0);   

	Property* Get_prop(int index);
	int Get_int(char const * const _propname) const;
	const char* Get_string(char const * const _propname) const;
	bool Get_bool(char const * const _propname) const;
	int Get_hex(char const * const _propname) const;
	double Get_double(char const * const _propname) const;
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
	
	Section* GetSection(int index);
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
