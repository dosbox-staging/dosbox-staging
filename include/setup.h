/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

parse_environ_result_t parse_environ(const char* const* envp) noexcept;

class Hex {
private:
	int value;

public:
	Hex() : value(0) {}
	Hex(int in) : value(in) {}

	bool operator==(const Hex& other) const
	{
		return value == other.value;
	}

	operator int() const
	{
		return value;
	}
};

class Value {
private:
	Hex _hex            = 0;
	bool _bool          = false;
	int _int            = 0;
	std::string _string = {};
	double _double      = 0;

public:
	enum Etype {
		V_NONE,
		V_HEX,
		V_BOOL,
		V_INT,
		V_STRING,
		V_DOUBLE,
		V_CURRENT
	} type = V_NONE;

	// Constructors
	Value() = default;

	Value(const Hex& in) : _hex(in), type(V_HEX) {}
	Value(int in) : _int(in), type(V_INT) {}
	Value(bool in) : _bool(in), type(V_BOOL) {}
	Value(double in) : _double(in), type(V_DOUBLE) {}

	Value(const std::string& in, Etype t)
	{
		SetValue(in, t);
	}

	Value(const std::string& in) : _string(in), type(V_STRING) {}

	Value(const char* const in) : _string(in), type(V_STRING) {}

	bool operator==(const Value& other) const;

	operator bool() const;
	operator Hex() const;
	operator int() const;
	operator double() const;
	operator const char*() const;

	bool SetValue(const std::string& in, Etype _type = V_CURRENT);

	std::string ToString() const;

private:
	bool SetHex(const std::string& in);
	bool SetInt(const std::string& in);
	bool SetBool(const std::string& in);
	void SetString(const std::string& in);
	bool SetDouble(const std::string& in);
};

class Property {
public:
	struct Changeable {
		enum Value { Always, WhenIdle, OnlyAtStart, Deprecated };
	};

	const std::string propname;

	Property(const std::string& name, Changeable::Value when);

	virtual ~Property() = default;

	void Set_values(const char* const* in);
	void Set_values(const std::vector<std::string>& in);
	void Set_help(const std::string& str);

	const char* GetHelp() const;
	const char* GetHelpUtf8() const;

	virtual bool SetValue(const std::string& str) = 0;

	const Value& GetValue() const
	{
		return value;
	}
	const Value& GetDefaultValue() const
	{
		return default_value;
	}

	bool IsRestrictedValue() const
	{
		return !valid_values.empty();
	}

	virtual bool IsValidValue(const Value& in);

	Changeable::Value GetChange() const
	{
		return change;
	}

	bool IsDeprecated() const
	{
		return (change == Changeable::Value::Deprecated);
	}

	virtual const std::vector<Value>& GetValues() const;

	Value::Etype Get_type()
	{
		return default_value.type;
	}

protected:
	virtual bool ValidateValue(const Value& in);

	Value value;
	std::vector<Value> valid_values;
	Value default_value;
	const Changeable::Value change;

	typedef std::vector<Value>::const_iterator const_iter;
};

class Prop_int final : public Property {
public:
	Prop_int(const std::string& name, Changeable::Value when, int val)
	        : Property(name, when),
	          min_value(-1),
	          max_value(-1)
	{
		default_value = val;
		value         = val;
	}

	~Prop_int() override = default;

	int GetMin() const
	{
		return min_value;
	}
	int GetMax() const
	{
		return max_value;
	}

	void SetMinMax(const Value& min, const Value& max)
	{
		min_value = min;
		max_value = max;
	}

	bool SetValue(const std::string& in) override;
	bool IsValidValue(const Value& in) override;

protected:
	bool ValidateValue(const Value& in) override;

private:
	Value min_value;
	Value max_value;
};

class Prop_double final : public Property {
public:
	Prop_double(const std::string& _propname, Changeable::Value when, double _value)
	        : Property(_propname, when)
	{
		default_value = value = _value;
	}
	bool SetValue(const std::string& input);
	~Prop_double() {}
};

class Prop_bool final : public Property {
public:
	Prop_bool(const std::string& _propname, Changeable::Value when, bool _value)
	        : Property(_propname, when)
	{
		default_value = value = _value;
	}
	bool SetValue(const std::string& in);
	~Prop_bool() {}
};

class Prop_string : public Property {
public:
	Prop_string(const std::string& name, Changeable::Value when, const char* val)
	        : Property(name, when)
	{
		default_value = val;
		value         = val;
	}

	~Prop_string() override = default;

	bool SetValue(const std::string& in) override;
	bool IsValidValue(const Value& in) override;
};

class Prop_path final : public Prop_string {
public:
	Prop_path(const std::string& name, Changeable::Value when, const char* val)
	        : Prop_string(name, when, val),
	          realpath(val)
	{}

	~Prop_path() override = default;

	bool SetValue(const std::string& in) override;

	std::string realpath;
};

class Prop_hex final : public Property {
public:
	Prop_hex(const std::string& _propname, Changeable::Value when, Hex _value)
	        : Property(_propname, when)
	{
		default_value = value = _value;
	}
	bool SetValue(const std::string& in);
	~Prop_hex() {}
};

#define NO_SUCH_PROPERTY "PROP_NOT_EXIST"

typedef void (*SectionFunction)(Section*);

class Section {
private:
	// Wrapper class around startup and shutdown functions. the variable
	// changeable_at_runtime indicates it can be called on configuration
	// changes
	struct Function_wrapper {
		SectionFunction function;
		bool changeable_at_runtime;

		Function_wrapper(const SectionFunction fn, bool ch)
		        : function(fn),
		          changeable_at_runtime(ch)
		{}
	};

	std::deque<Function_wrapper> early_init_functions = {};
	std::deque<Function_wrapper> initfunctions        = {};
	std::deque<Function_wrapper> destroyfunctions     = {};
	std::string sectionname                           = {};

public:
	Section() = default;
	Section(const std::string& name) : sectionname(name) {}

	// Construct and assign by std::move
	Section(Section&& other)            = default;
	Section& operator=(Section&& other) = default;

	// Children must call executedestroy!
	virtual ~Section() = default;

	void AddEarlyInitFunction(SectionFunction func,
	                          bool changeable_at_runtime = false);

	void AddInitFunction(SectionFunction func, bool changeable_at_runtime = false);

	void AddDestroyFunction(SectionFunction func,
	                        bool changeable_at_runtime = false);

	void ExecuteEarlyInit(bool initall = true);
	void ExecuteInit(bool initall = true);
	void ExecuteDestroy(bool destroyall = true);

	const char* GetName() const
	{
		return sectionname.c_str();
	}

	virtual std::string GetPropValue(const std::string& property) const = 0;

	virtual bool HandleInputline(const std::string& line) = 0;

	virtual void PrintData(FILE* outfile) const = 0;
};

class PropMultiVal;
class PropMultiValRemain;

class Section_prop final : public Section {
private:
	std::deque<Property*> properties = {};
	typedef std::deque<Property*>::iterator it;
	typedef std::deque<Property*>::const_iterator const_it;

public:
	Section_prop(const std::string& name) : Section(name) {}

	~Section_prop() override;

	Prop_int* Add_int(const std::string& _propname,
	                  Property::Changeable::Value when, int _value = 0);

	Prop_string* Add_string(const std::string& _propname,
	                        Property::Changeable::Value when,
	                        const char* _value = nullptr);

	Prop_path* Add_path(const std::string& _propname,
	                    Property::Changeable::Value when,
	                    const char* _value = nullptr);

	Prop_bool* Add_bool(const std::string& _propname,
	                    Property::Changeable::Value when, bool _value = false);

	Prop_hex* Add_hex(const std::string& _propname,
	                  Property::Changeable::Value when, Hex _value = 0);

	//	void Add_double(const char * _propname, double _value=0.0);
	//
	PropMultiVal* AddMultiVal(const std::string& _propname,
	                          Property::Changeable::Value when,
	                          const std::string& sep);

	PropMultiValRemain* AddMultiValRemain(const std::string& _propname,
	                                      Property::Changeable::Value when,
	                                      const std::string& sep);

	Property* Get_prop(int index);

	int Get_int(const std::string& _propname) const;

	const char* Get_string(const std::string& _propname) const;

	bool Get_bool(const std::string& _propname) const;

	Hex Get_hex(const std::string& _propname) const;

	double Get_double(const std::string& _propname) const;

	Prop_path* Get_path(const std::string& _propname) const;

	PropMultiVal* GetMultiVal(const std::string& _propname) const;

	PropMultiValRemain* GetMultiValRemain(const std::string& _propname) const;

	bool HandleInputline(const std::string& line) override;

	void PrintData(FILE* outfile) const override;

	std::string GetPropValue(const std::string& property) const override;
};

class PropMultiVal : public Property {
protected:
	std::unique_ptr<Section_prop> section;
	std::string separator;

	void MakeDefaultValue();

public:
	PropMultiVal(const std::string& name, Changeable::Value when,
	             const std::string& sep)
	        : Property(name, when),
	          section(new Section_prop("")),
	          separator(sep)
	{
		default_value = "";
		value         = "";
	}

	Section_prop* GetSection()
	{
		return section.get();
	}
	const Section_prop* GetSection() const
	{
		return section.get();
	}

	// value contains total string.
	// SetValue sets each of the sub properties.
	bool SetValue(const std::string& input) override;

	const std::vector<Value>& GetValues() const override;
};

class PropMultiValRemain final : public PropMultiVal {
public:
	PropMultiValRemain(const std::string& _propname, Changeable::Value when,
	                   const std::string& sep)
	        : PropMultiVal(_propname, when, sep)
	{}

	virtual bool SetValue(const std::string& input);
};

class Section_line final : public Section {
public:
	Section_line() = default;
	Section_line(const std::string& name) : Section(name), data() {}

	// Construct and assign by std::move
	Section_line(Section_line&& other)            = default;
	Section_line& operator=(Section_line&& other) = default;

	~Section_line() override
	{
		ExecuteDestroy(true);
	}

	std::string GetPropValue(const std::string& property) const override;
	bool HandleInputline(const std::string& line) override;
	void PrintData(FILE* outfile) const override;

	std::string data = {};
};

/* Base for all hardware and software "devices" */
class Module_base {
protected:
	Section* m_configuration;

public:
	Module_base(Section* conf_section) : m_configuration(conf_section) {}

	Module_base(const Module_base&) = delete;            // prevent copying
	Module_base& operator=(const Module_base&) = delete; // prevent assignment

	virtual ~Module_base() = default;

	virtual bool Change_Config(Section* /*newconfig*/)
	{
		return false;
	}
};

void SETUP_ParseConfigFiles(const std::string& config_path);

const std::string& SETUP_GetLanguage();

const char* SetProp(std::vector<std::string>& pvars);

#endif
