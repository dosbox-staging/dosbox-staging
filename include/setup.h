/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_SETUP_H
#define DOSBOX_SETUP_H

#include "dosbox.h"

#include <cstdio>
#include <deque>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using parse_environ_result_t = std::list<std::tuple<std::string, std::string>>;

parse_environ_result_t parse_environ(const char * const * envp) noexcept;

class Hex {
private:
	int value;

public:
	Hex() : value(0) {}
	Hex(int in) : value(in) {}
	bool operator==(const Hex &other) const { return value == other.value; }
	operator int() const { return value; }
};

/* 
 * Multitype storage container that is aware of the currently stored type in it.
 * Value st = "hello";
 * Value in = 1;
 * st = 12 //Exception
 * in = 12 //works
 */
class Value {
private:
	Hex _hex = 0;
	bool _bool = false;
	int _int = 0;
	std::string *_string = nullptr;
	double _double = 0;

public:
	class WrongType {}; // Conversion error class

	enum Etype {
		V_NONE,
		V_HEX,
		V_BOOL,
		V_INT,
		V_STRING,
		V_DOUBLE,
		V_CURRENT
	} type = V_NONE;

	/* Constructors */

	Value() = default;

	Value(const Hex &in) : _hex(in), type(V_HEX) {}

	Value(int in) : _int(in), type(V_INT) {}

	Value(bool in) : _bool(in), type(V_BOOL) {}

	Value(double in) : _double(in), type(V_DOUBLE) {}

	Value(const std::string &in, Etype t) { SetValue(in, t); }

	Value(const std::string &in)
	        : _string(new std::string(in)),
	          type(V_STRING)
	{}

	Value(char const *const in)
	        : _string(new std::string(in)),
	          type(V_STRING)
	{}

	Value(const Value &in) { plaincopy(in); }

	/* Destructor */
	virtual ~Value() { destroy(); }

	/* Assignment operators */
	Value &operator=(Hex in) { return copy(Value(in)); }
	Value &operator=(int in) { return copy(Value(in)); }
	Value &operator=(bool in) { return copy(Value(in)); }
	Value &operator=(double in) { return copy(Value(in)); }
	Value &operator=(const std::string &in) { return copy(Value(in)); }
	Value &operator=(char const *const in) { return copy(Value(in)); }
	Value &operator=(const Value &in) { return copy(Value(in)); }

	bool operator==(const Value &other) const;

	operator bool() const;
	operator Hex() const;
	operator int() const;
	operator double() const;
	operator char const *() const;

	bool SetValue(const std::string &in, Etype _type = V_CURRENT);

	std::string ToString() const;

private:
	void destroy() throw();
	Value &copy(const Value &in);
	void plaincopy(const Value &in) throw();
	bool set_hex(const std::string &in);
	bool set_int(const std::string &in);
	bool set_bool(const std::string &in);
	void set_string(const std::string &in);
	bool set_double(const std::string &in);
};

class Property {
public:
	struct Changeable {
		enum Value { Always, WhenIdle, OnlyAtStart, Deprecated };
	};

	const std::string propname;

	Property(const std::string &name, Changeable::Value when);

	virtual ~Property() = default;

	void Set_values(const char * const * in);
	void Set_values(const std::vector<std::string> &in);
	void Set_help(std::string const& str);

	const char* GetHelp() const;

	virtual	bool SetValue(std::string const& str)=0;
	Value const& GetValue() const { return value;}
	Value const& Get_Default_Value() const { return default_value; }
	//CheckValue returns true, if value is in suggested_values;
	//Type specific properties are encouraged to override this and check for type
	//specific features.
	virtual bool CheckValue(Value const& in, bool warn);

	Changeable::Value GetChange() const { return change; }
	bool IsDeprecated() const { return (change == Changeable::Value::Deprecated); }

	virtual const std::vector<Value>& GetValues() const;
	Value::Etype Get_type(){return default_value.type;}

protected:
	//Set interval value to in or default if in is invalid. force always sets the value.
	//Can be overridden to set a different value if invalid.
	virtual bool SetVal(Value const& in, bool forced,bool warn=true) {
		if(forced || CheckValue(in,warn)) {
			value = in; return true;
		} else {
			value = default_value; return false;
		}
	}
	Value value;
	std::vector<Value> suggested_values;
	typedef std::vector<Value>::const_iterator const_iter;
	Value default_value;
	const Changeable::Value change;
};

class Prop_int final : public Property {
public:
	Prop_int(const std::string &name, Changeable::Value when, int val)
	        : Property(name, when),
	          min_value(-1),
	          max_value(-1)
	{
		default_value = val;
		value = val;
	}

	~Prop_int() override = default;

	int GetMin() const { return min_value; }
	int GetMax() const { return max_value; }

	void SetMinMax(const Value &min, const Value &max)
	{
		min_value = min;
		max_value = max;
	}

	bool SetValue(const std::string &in) override;

	bool CheckValue(const Value &in, bool warn) override;

protected:
	// Override SetVal, so it takes min,max in account when there are no
	// suggested values
	bool SetVal(const Value &in, bool forced, bool warn = true) override;

private:
	Value min_value;
	Value max_value;
};

class Prop_double final : public Property {
public:
	Prop_double(std::string const & _propname, Changeable::Value when, double _value)
		:Property(_propname,when){
		default_value = value = _value;
	}
	bool SetValue(std::string const& input);
	~Prop_double(){ }
};

class Prop_bool final : public Property {
public:
	Prop_bool(std::string const& _propname, Changeable::Value when, bool _value)
		:Property(_propname,when) {
		default_value = value = _value;
	}
	bool SetValue(std::string const& in);
	~Prop_bool(){ }
};

class Prop_string : public Property {
public:
	Prop_string(const std::string &name, Changeable::Value when, const char *val)
	        : Property(name, when)
	{
		default_value = val;
		value = val;
	}

	~Prop_string() override = default;

	bool SetValue(const std::string &in) override;

	bool CheckValue(const Value &in, bool warn) override;
};

class Prop_path final : public Prop_string {
public:
	Prop_path(const std::string &name, Changeable::Value when, const char *val)
	        : Prop_string(name, when, val),
	          realpath(val)
	{}

	~Prop_path() override = default;

	bool SetValue(const std::string &in) override;

	std::string realpath;
};

class Prop_hex final : public Property {
public:
	Prop_hex(std::string const& _propname, Changeable::Value when, Hex _value)
		:Property(_propname,when) {
		default_value = value = _value;
	}
	bool SetValue(std::string const& in);
	~Prop_hex(){ }
};

#define NO_SUCH_PROPERTY "PROP_NOT_EXIST"

typedef void (*SectionFunction)(Section *);

class Section {
private:
	/* Wrapper class around startup and shutdown functions. the variable
	 * changeable_at_runtime indicates it can be called on configuration
	 * changes */
	struct Function_wrapper {
		SectionFunction function;
		bool changeable_at_runtime;

		Function_wrapper(SectionFunction const fn, bool ch)
		        : function(fn),
		          changeable_at_runtime(ch)
		{}
	};

	std::deque<Function_wrapper> early_init_functions = {};
	std::deque<Function_wrapper> initfunctions = {};
	std::deque<Function_wrapper> destroyfunctions = {};
	std::string sectionname;
public:
	Section(const std::string &name) : sectionname(name) {}

	virtual ~Section() = default; // Children must call executedestroy!

	void AddEarlyInitFunction(SectionFunction func,
	                          bool changeable_at_runtime = false);
	void AddInitFunction(SectionFunction func, bool changeable_at_runtime = false);
	void AddDestroyFunction(SectionFunction func,
	                        bool changeable_at_runtime = false);

	void ExecuteEarlyInit(bool initall = true);
	void ExecuteInit(bool initall=true);
	void ExecuteDestroy(bool destroyall=true);
	const char* GetName() const {return sectionname.c_str();}

	virtual std::string GetPropValue(const std::string &property) const = 0;
	virtual bool HandleInputline(const std::string &line) = 0;
	virtual void PrintData(FILE *outfile) const = 0;
};

class Prop_multival;
class Prop_multival_remain;

class Section_prop final : public Section {
private:
	std::deque<Property *> properties = {};
	typedef std::deque<Property*>::iterator it;
	typedef std::deque<Property*>::const_iterator const_it;

public:
	Section_prop(const std::string &name) : Section(name) {}

	~Section_prop() override;

	Prop_int* Add_int(std::string const& _propname, Property::Changeable::Value when, int _value=0);
	Prop_string* Add_string(std::string const& _propname, Property::Changeable::Value when, char const * const _value=NULL);
	Prop_path* Add_path(std::string const& _propname, Property::Changeable::Value when, char const * const _value=NULL);
	Prop_bool*  Add_bool(std::string const& _propname, Property::Changeable::Value when, bool _value=false);
	Prop_hex* Add_hex(std::string const& _propname, Property::Changeable::Value when, Hex _value=0);
//	void Add_double(char const * const _propname, double _value=0.0);
	Prop_multival *Add_multi(std::string const& _propname, Property::Changeable::Value when,std::string const& sep);
	Prop_multival_remain *Add_multiremain(std::string const& _propname, Property::Changeable::Value when,std::string const& sep);

	Property* Get_prop(int index);
	int Get_int(std::string const& _propname) const;
	const char* Get_string(std::string const& _propname) const;
	bool Get_bool(std::string const& _propname) const;
	Hex Get_hex(std::string const& _propname) const;
	double Get_double(std::string const& _propname) const;
	Prop_path* Get_path(std::string const& _propname) const;
	Prop_multival* Get_multival(std::string const& _propname) const;
	Prop_multival_remain* Get_multivalremain(std::string const& _propname) const;
	bool HandleInputline(const std::string &line) override;
	void PrintData(FILE* outfile) const override;
	std::string GetPropValue(const std::string &property) const override;
};

class Prop_multival : public Property {
protected:
	std::unique_ptr<Section_prop> section;
	std::string separator;
	void make_default_value();

public:
	Prop_multival(const std::string &name,
	              Changeable::Value when,
	              const std::string &sep)
	        : Property(name, when),
	          section(new Section_prop("")),
	          separator(sep)
	{
		default_value = "";
		value = "";
	}

	Section_prop *GetSection() { return section.get(); }
	const Section_prop *GetSection() const { return section.get(); }

	// value contains total string.
	// SetValue sets each of the sub properties.
	bool SetValue(const std::string &input) override;

	const std::vector<Value> &GetValues() const override;
};

class Prop_multival_remain final : public Prop_multival{
public:
	Prop_multival_remain(std::string const& _propname, Changeable::Value when,std::string const& sep):Prop_multival(_propname,when,sep){ }

	virtual bool SetValue(std::string const& input);
};

class Section_line final : public Section {
public:
	Section_line(std::string const &name) : Section(name), data() {}

	~Section_line() override { ExecuteDestroy(true); }

	std::string GetPropValue(const std::string &property) const override;
	bool HandleInputline(const std::string &line) override;
	void PrintData(FILE *outfile) const override;

	std::string data;
};

/* Base for all hardware and software "devices" */
class Module_base {
protected:
	Section *m_configuration;

public:
	Module_base(Section *conf_section) : m_configuration(conf_section) {}

	Module_base(const Module_base &) = delete; // prevent copying
	Module_base &operator=(const Module_base &) = delete; // prevent assignment

	virtual ~Module_base() = default;

	virtual bool Change_Config(Section * /*newconfig*/) { return false; }
};

void SETUP_ParseConfigFiles(const std::string &config_path);

#endif
