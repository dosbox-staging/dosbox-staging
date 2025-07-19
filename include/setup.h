// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SETUP_H
#define DOSBOX_SETUP_H

#include "dosbox.h"

#include <cstdio>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "std_filesystem.h"

using parse_environ_result_t = std::list<std::tuple<std::string, std::string>>;

parse_environ_result_t parse_environ(const char* const* envp) noexcept;

// Helpers to test if a string setting is boolean
std::optional<bool> parse_bool_setting(const std::string_view setting);
bool has_true(const std::string_view setting);
bool has_false(const std::string_view setting);

void set_section_property_value(const std::string_view section_name,
                                const std::string_view property_name,
                                const std::string_view property_value);

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
	bool operator==(const Hex& other) const;
	bool operator<(const Value& other) const;

	operator bool() const;
	operator Hex() const;
	operator int() const;
	operator double() const;
	operator std::string() const;

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
		enum Value {
			Always,
			WhenIdle,
			OnlyAtStart,
			Deprecated,
			DeprecatedButAllowed
		};
	};

	const std::string propname;

	Property(const std::string& name, Changeable::Value when);

	virtual ~Property() = default;

	void Set_values(const std::vector<std::string>& in);
	void SetEnabledOptions(const std::vector<std::string>& in);
	void SetDeprecatedWithAlternateValue(const char* deprecated_value,
	                                     const char* alternate_value);

	// The string may contain a single '%s' marker. If present, it will be
	// substitued with the settings's default value (see `GetHelp()` and
	// `GetHelpUtf8()`).
	void Set_help(const std::string& help_text);

	void SetOptionHelp(const std::string& option, const std::string& help_text);
	void SetOptionHelp(const std::string& help_text);

	// If the setting's help text contains a '%s' marker, the `GetHelp`
	// functions will substitute it with the setting's default value.
	std::string GetHelp() const;
	std::string GetHelpForHost() const;

	virtual bool SetValue(const std::string& str) = 0;

	const Value& GetValue() const
	{
		return value;
	}
	const Value& GetDefaultValue() const
	{
		return default_value;
	}

	void SetQueueableValue(std::string&& value);

	const std::optional<std::string>& GetQueuedValue() const;

	bool IsRestrictedValue() const
	{
		return !valid_values.empty();
	}

	virtual bool IsValidValue(const Value& in);
	virtual bool IsValueDeprecated(const Value& value) const;

	Changeable::Value GetChange() const
	{
		return change;
	}

	bool IsDeprecated() const
	{
		return (change == Changeable::Value::Deprecated ||
		        change == Changeable::Value::DeprecatedButAllowed);
	}

	bool IsDeprecatedButAllowed() const
	{
		return change == Changeable::Value::DeprecatedButAllowed;
	}

	virtual const std::vector<Value>& GetValues() const;
	std::vector<Value> GetDeprecatedValues() const;
	const Value& GetAlternateForDeprecatedValue(const Value& value) const;

	Value::Etype Get_type() const
	{
		return default_value.type;
	}

protected:
	virtual bool ValidateValue(const Value& in);

	Value value                                            = {};
	std::vector<Value> valid_values                        = {};
	std::vector<std::string> enabled_options               = {};
	std::map<Value, Value> deprecated_and_alternate_values = {};
	std::optional<std::string> queueable_value             = {};
	bool is_positive_bool_valid                            = false;
	bool is_negative_bool_valid                            = false;

	Value default_value                                    = {};
	const Changeable::Value change                         = {};
	typedef std::vector<Value>::const_iterator const_iter;

private:
	void MaybeSetBoolValid(const std::string_view value);
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
	bool SetValue(const std::string& input) override;
	~Prop_double() override = default;
};

class Prop_bool final : public Property {
public:
	Prop_bool(const std::string& _propname, Changeable::Value when, bool _value)
	        : Property(_propname, when)
	{
		default_value = value = _value;
	}
	bool SetValue(const std::string& in) override;
	~Prop_bool() override = default;
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
	        : Prop_string(name, when, val)
	{
		SetValue(val);
	}

	~Prop_path() override = default;

	bool SetValue(const std::string& in) override;

	std_fs::path realpath = {};
};

class Prop_hex final : public Property {
public:
	Prop_hex(const std::string& _propname, Changeable::Value when, Hex _value)
	        : Property(_propname, when)
	{
		default_value = value = _value;
	}
	bool SetValue(const std::string& in) override;
	~Prop_hex() override = default;
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

	std::deque<Function_wrapper> init_functions   = {};
	std::deque<Function_wrapper> destroyfunctions = {};
	std::string sectionname                       = {};
	bool active                                   = true;

public:
	Section() = default;
	Section(const std::string& name, const bool active = true) : sectionname(name), active(active) {}

	// Construct and assign by std::move
	Section(Section&& other)            = default;
	Section& operator=(Section&& other) = default;

	// Children must call executedestroy!
	virtual ~Section() = default;

	void AddInitFunction(SectionFunction func, bool changeable_at_runtime = false);

	void AddDestroyFunction(SectionFunction func,
	                        bool changeable_at_runtime = false);

	void ExecuteInit(bool initall = true);
	void ExecuteDestroy(bool destroyall = true);

	bool IsActive() const
	{
		return active;
	}

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
	Section_prop(const std::string& name, bool active = true) : Section(name, active) {}

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
	Property* Get_prop(const std::string_view propname);

	const_it begin() const
	{
		return properties.begin();
	}

	const_it end() const
	{
		return properties.end();
	}

	int Get_int(const std::string& _propname) const;

	std::string Get_string(const std::string& _propname) const;

	Prop_bool* GetBoolProp(const std::string& propname) const;
	Prop_string* GetStringProp(const std::string& propname) const;

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

	bool SetValue(const std::string& input) override;
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

// Base for all hardware and software "devices"
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

bool config_file_is_valid(const std_fs::path& path);

// Helper functions for retrieving configuration sections

Section_prop* get_section(const char* section_name);

Section_prop* get_joystick_section();
Section_prop* get_sdl_section();
Section_prop* get_mixer_section();

#endif
