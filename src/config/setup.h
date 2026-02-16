// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SETUP_H
#define DOSBOX_SETUP_H

#include <cstdio>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "misc/std_filesystem.h"

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
	Hex(int new_value) : value(new_value) {}

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

	Value(const Hex& new_value) : _hex(new_value), type(V_HEX) {}
	Value(int new_value) : _int(new_value), type(V_INT) {}
	Value(bool new_value) : _bool(new_value), type(V_BOOL) {}
	Value(double new_value) : _double(new_value), type(V_DOUBLE) {}

	Value(const std::string& new_value, Etype _type)
	{
		SetValue(new_value, _type);
	}

	Value(const std::string& new_value) : _string(new_value), type(V_STRING)
	{}

	Value(const char* const new_value) : _string(new_value), type(V_STRING)
	{}

	bool operator==(const Value& other) const;
	bool operator==(const Hex& other) const;
	bool operator<(const Value& other) const;

	operator bool() const;
	operator Hex() const;
	operator int() const;
	operator double() const;
	operator std::string() const;

	bool SetValue(const std::string& value, Etype type = V_CURRENT);

	std::string ToString() const;

private:
	bool SetHex(const std::string& value);
	bool SetInt(const std::string& value);
	bool SetBool(const std::string& value);
	void SetString(const std::string& value);
	bool SetDouble(const std::string& value);
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

	void SetValues(const std::vector<std::string>& values);
	void SetEnabledOptions(const std::vector<std::string>& options);
	void SetDeprecatedWithAlternateValue(const char* deprecated_value,
	                                     const char* alternate_value);

	// The string may contain a single '%s' marker. If present, it will be
	// substitued with the settings's default value (e.g., `blocksize`,
	// `prebuffer`, etc.)
	void SetHelp(const std::string& help_text);

	void SetOptionHelp(const std::string& option, const std::string& help_text);
	void SetOptionHelp(const std::string& help_text);

	// Returns the translated help message, or the English help message if
	// no translation exists for the current language, adapted to the
	// currently active DOS code page with markup tags converted to ANSI
	// escape codes.
	//
	// If the setting's help text contains a '%s' marker, it will be
	// substituted with the setting's default value. (e.g., `blocksize`,
	// `prebuffer`, etc.)
	//
	std::string GetHelp() const;

	// Returns the help text in UTF-8 without any DOS code page or ANSI
	// markup tags conversions applied.
	//
	// If the setting's help text contains a '%s' marker, it will be
	// substituted with the setting's default value. (e.g., `blocksize`,
	// `prebuffer`, etc.)
	//
	std::string GetHelpRaw() const;

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

	virtual bool IsValidValue(const Value& value);
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

	Value::Etype GetType() const
	{
		return default_value.type;
	}

protected:
	virtual bool ValidateValue(const Value& value);

	Value value                                            = {};
	std::vector<Value> valid_values                        = {};
	std::vector<std::string> enabled_options               = {};
	std::map<Value, Value> deprecated_and_alternate_values = {};
	std::optional<std::string> queueable_value             = {};
	bool is_positive_bool_valid                            = false;
	bool is_negative_bool_valid                            = false;

	Value default_value            = {};
	const Changeable::Value change = {};
	typedef std::vector<Value>::const_iterator const_iter;

private:
	void MaybeSetBoolValid(const std::string_view value);
};

class PropInt final : public Property {
public:
	PropInt(const std::string& name, Changeable::Value when, int new_value)
	        : Property(name, when),
	          min_value(-1),
	          max_value(-1)
	{
		default_value = new_value;
		value         = new_value;
	}

	~PropInt() override = default;

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

	bool SetValue(const std::string& value) override;
	bool IsValidValue(const Value& value) override;

protected:
	bool ValidateValue(const Value& value) override;

private:
	Value min_value;
	Value max_value;
};

class PropDouble final : public Property {
public:
	PropDouble(const std::string& _propname, Changeable::Value when,
	           double new_value)
	        : Property(_propname, when)
	{
		default_value = value = new_value;
	}
	bool SetValue(const std::string& value) override;
	~PropDouble() override = default;
};

class PropBool final : public Property {
public:
	PropBool(const std::string& _propname, Changeable::Value when, bool new_value)
	        : Property(_propname, when)
	{
		default_value = value = new_value;
	}
	bool SetValue(const std::string& value) override;
	~PropBool() override = default;
};

class PropString : public Property {
public:
	PropString(const std::string& name, Changeable::Value when, const char* val)
	        : Property(name, when)
	{
		default_value = val;
		value         = val;
	}

	~PropString() override = default;

	bool SetValue(const std::string& value) override;
	bool IsValidValue(const Value& value) override;
};

class PropPath final : public PropString {
public:
	PropPath(const std::string& name, Changeable::Value when, const char* val)
	        : PropString(name, when, val)
	{
		SetValue(val);
	}

	~PropPath() override = default;

	bool SetValue(const std::string& value) override;

	std_fs::path realpath = {};
};

class PropHex final : public Property {
public:
	PropHex(const std::string& _propname, Changeable::Value when, Hex new_value)
	        : Property(_propname, when)
	{
		default_value = value = new_value;
	}
	bool SetValue(const std::string& value) override;
	~PropHex() override = default;
};

#define NO_SUCH_PROPERTY "PROP_NOT_EXIST"

class SectionProp;

using SectionUpdateHandler =
        std::function<void(SectionProp&, const std::string& prop_name)>;

class Section {
private:
	std::vector<SectionUpdateHandler> update_handlers = {};

	std::string sectionname = {};

	bool active = true;

public:
	Section() = default;
	Section(const std::string& name, const bool active = true)
	        : sectionname(name),
	          active(active)
	{}

	// Construct and assign by std::move
	Section(Section&& other)            = default;
	Section& operator=(Section&& other) = default;

	virtual ~Section() = default;

	void AddUpdateHandler(SectionUpdateHandler update_handler);
	void ExecuteUpdate(const Property& property);

	bool IsActive() const
	{
		return active;
	}

	const char* GetName() const
	{
		return sectionname.c_str();
	}

	virtual std::string GetPropertyValue(const std::string& property) const = 0;

	virtual bool HandleInputLine(const std::string& line) = 0;

	virtual void PrintData(FILE* outfile) const = 0;
};

class PropMultiVal;
class PropMultiValRemain;

class SectionProp final : public Section {
private:
	std::deque<Property*> properties = {};
	typedef std::deque<Property*>::iterator it;
	typedef std::deque<Property*>::const_iterator const_it;

public:
	SectionProp(const std::string& name, bool active = true)
	        : Section(name, active)
	{}

	~SectionProp() override;

	PropInt* AddInt(const std::string& _propname,
	                Property::Changeable::Value when, int new_value = 0);

	PropString* AddString(const std::string& _propname,
	                      Property::Changeable::Value when,
	                      const char* new_value = nullptr);

	PropPath* AddPath(const std::string& _propname,
	                  Property::Changeable::Value when,
	                  const char* new_value = nullptr);

	PropBool* AddBool(const std::string& _propname,
	                  Property::Changeable::Value when, bool new_value = false);

	PropHex* AddHex(const std::string& _propname,
	                Property::Changeable::Value when, Hex new_value = 0);

	PropMultiVal* AddMultiVal(const std::string& _propname,
	                          Property::Changeable::Value when,
	                          const std::string& sep);

	PropMultiValRemain* AddMultiValRemain(const std::string& _propname,
	                                      Property::Changeable::Value when,
	                                      const std::string& sep);

	Property* GetProperty(int index);
	Property* GetProperty(const std::string_view propname);

	const_it begin() const
	{
		return properties.begin();
	}

	const_it end() const
	{
		return properties.end();
	}

	int GetInt(const std::string& _propname) const;

	std::string GetString(const std::string& _propname) const;
	std::string GetStringLowCase(const std::string& _propname) const;

	PropBool* GetBoolProp(const std::string& propname) const;
	PropString* GetStringProp(const std::string& propname) const;

	bool GetBool(const std::string& _propname) const;

	Hex GetHex(const std::string& _propname) const;

	double GetDouble(const std::string& _propname) const;

	PropPath* GetPath(const std::string& _propname) const;

	PropMultiVal* GetMultiVal(const std::string& _propname) const;

	PropMultiValRemain* GetMultiValRemain(const std::string& _propname) const;

	bool HandleInputLine(const std::string& line) override;

	void PrintData(FILE* outfile) const override;

	std::string GetPropertyValue(const std::string& property) const override;
};

class PropMultiVal : public Property {
protected:
	std::unique_ptr<SectionProp> section;
	std::string separator;

	void MakeDefaultValue();

public:
	PropMultiVal(const std::string& name, Changeable::Value when,
	             const std::string& sep)
	        : Property(name, when),
	          section(new SectionProp("")),
	          separator(sep)
	{
		default_value = "";
		value         = "";
	}

	SectionProp* GetSection()
	{
		return section.get();
	}
	const SectionProp* GetSection() const
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

class AutoExecSection final : public Section {
public:
	AutoExecSection() = default;
	AutoExecSection(const std::string& name) : Section(name), data() {}

	// Construct and assign by std::move
	AutoExecSection(AutoExecSection&& other)            = default;
	AutoExecSection& operator=(AutoExecSection&& other) = default;

	std::string GetPropertyValue(const std::string& property) const override;
	bool HandleInputLine(const std::string& line) override;
	void PrintData(FILE* outfile) const override;

	std::string data = {};
};

bool config_file_is_valid(const std_fs::path& path);

// Helper functions for retrieving configuration sections

SectionProp* get_section(const char* section_name);
AutoExecSection* get_autoexec_section(const char* section_name);

SectionProp* get_joystick_section();
SectionProp* get_sdl_section();
SectionProp* get_mixer_section();

#endif // DOSBOX_SETUP_H
