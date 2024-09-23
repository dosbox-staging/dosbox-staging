/*
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2023  The DOSBox Team
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

#include "setup.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "support.h"
#include "version.h"

#if defined(_MSC_VER) || (defined(__MINGW32__) && defined(__clang__))
_CRTIMP extern char** _environ;
#else
extern char** environ;
#endif

// Commonly accessed global that holds configuration records
ConfigPtr control = {};

// Set by parseconfigfile so Prop_path can use it to construct the realpath
static std_fs::path current_config_dir;

Value::operator bool() const
{
	assert(type == V_BOOL);
	return _bool;
}

Value::operator Hex() const
{
	assert(type == V_HEX);
	return _hex;
}

Value::operator int() const
{
	assert(type == V_INT);
	return _int;
}

Value::operator double() const
{
	assert(type == V_DOUBLE);
	return _double;
}

Value::operator std::string() const
{
	assert(type == V_STRING);
	return _string;
}

bool Value::operator==(const Value& other) const
{
	if (this == &other) {
		return true;
	}
	if (type != other.type) {
		return false;
	}
	switch (type) {
	case V_BOOL: return _bool == other._bool;
	case V_INT: return _int == other._int;
	case V_HEX: return _hex == other._hex;
	case V_DOUBLE: return _double == other._double;
	case V_STRING: return _string == other._string;
	default:
		LOG_ERR("SETUP: Comparing stuff that doesn't make sense");
		break;
	}
	return false;
}

bool Value::operator==(const Hex& other) const {
    return this->_hex == other;
}

bool Value::operator<(const Value& other) const
{
	return std::tie(_hex, _bool, _int, _string, _double) <
	       std::tie(other._hex, other._bool, other._int, other._string, other._double);
}

bool Value::SetValue(const std::string& in, const Etype _type)
{
	assert(type == V_NONE || type == _type);
	type = _type;

	bool is_valid = true;
	switch (type) {
	case V_HEX: is_valid = SetHex(in); break;
	case V_INT: is_valid = SetInt(in); break;
	case V_BOOL: is_valid = SetBool(in); break;
	case V_STRING: SetString(in); break;
	case V_DOUBLE: is_valid = SetDouble(in); break;

	case V_NONE:
	case V_CURRENT:
	default:
		LOG_ERR("SETUP: Unhandled type when setting value: '%s'",
		        in.c_str());
		is_valid = false;
		break;
	}
	return is_valid;
}

bool Value::SetHex(const std::string& in)
{
	std::istringstream input(in);
	input.flags(std::ios::hex);
	int result = INT_MIN;
	input >> result;
	if (result == INT_MIN) {
		return false;
	}
	_hex = result;
	return true;
}

bool Value::SetInt(const std::string& in)
{
	std::istringstream input(in);
	int result = INT_MIN;
	input >> result;
	if (result == INT_MIN) {
		return false;
	}
	_int = result;
	return true;
}

bool Value::SetDouble(const std::string& in)
{
	std::istringstream input(in);
	double result = std::numeric_limits<double>::infinity();
	input >> result;
	if (result == std::numeric_limits<double>::infinity()) {
		return false;
	}
	_double = result;
	return true;
}

// Sets the '_bool' member variable to either the parsed boolean value or false
// if it couldn't be parsed. Returns true if the provided string was parsed.
bool Value::SetBool(const std::string& in)
{
	auto in_lowercase = in;
	lowcase(in_lowercase);
	const auto parsed = parse_bool_setting(in_lowercase);
	_bool = parsed ? *parsed : false;
	return parsed.has_value();
}

void Value::SetString(const std::string& in)
{
	_string = in;
}

std::string Value::ToString() const
{
	std::ostringstream oss;
	switch (type) {
	case V_HEX:
		oss.flags(std::ios::hex);
		oss << _hex;
		break;
	case V_INT: oss << _int; break;
	case V_BOOL: oss << std::boolalpha << _bool; break;
	case V_STRING: oss << _string; break;
	case V_DOUBLE:
		oss.precision(2);
		oss << std::fixed << _double;
		break;
	case V_NONE:
	case V_CURRENT:
	default: E_Exit("ToString messed up ?"); break;
	}
	return oss.str();
}

Property::Property(const std::string& name, Changeable::Value when)
        : propname(name),
          value(),
          valid_values{},
          default_value(),
          change(when)
{
	assertm(std::regex_match(name, std::regex{"[a-zA-Z0-9_]+"}),
	        "Only letters, digits, and underscores are allowed in property names");
}

bool Property::IsValidValue(const Value& in)
{
	if (!IsRestrictedValue()) {
		return true;
	}

	for (const_iter it = valid_values.begin(); it != valid_values.end(); ++it) {
		if ((*it) == in) {
			return true;
		}
	}

	LOG_WARNING("CONFIG: Invalid '%s' setting: '%s', using '%s'",
	            propname.c_str(),
	            in.ToString().c_str(),
	            default_value.ToString().c_str());

	return false;
}

bool Property::IsValueDeprecated(const Value& val) const
{
	const auto is_deprecated = contains(deprecated_and_alternate_values, val);
	if (is_deprecated) {
		LOG_WARNING("CONFIG: Setting '%s = %s' is deprecated, "
		            "falling back to the alternate: '%s = %s'",
		            propname.c_str(),
		            val.ToString().c_str(),
		            propname.c_str(),
		            GetAlternateForDeprecatedValue(val).ToString().c_str());
	}
	return is_deprecated;
}

bool Property::ValidateValue(const Value& in)
{
	if (IsValueDeprecated(in)) {
		value = GetAlternateForDeprecatedValue(in);
		return true;
	} else if (IsValidValue(in)) {
		value = in;
		return true;
	} else {
		value = default_value;
		return false;
	}
}

static std::string create_config_name(const std::string& propname)
{
	std::string result = "CONFIG_" + propname;
	upcase(result);
	return result;
}

void Property::Set_help(const std::string& in)
{
	MSG_Add(create_config_name(propname).c_str(), in.c_str());
}

static std::string create_config_item_name(const std::string& propname,
                                           const std::string& item)
{
	std::string result = "CONFIGITEM_" + propname;
	if (!item.empty()) {
		result += '_' + item;
	}
	upcase(result);
	return result;
}

void Property::SetOptionHelp(const std::string& option, const std::string& in)
{
	MSG_Add(create_config_item_name(propname, option).c_str(), in.c_str());
}

void Property::SetOptionHelp(const std::string& in)
{
	MSG_Add(create_config_item_name(propname, {}).c_str(), in.c_str());
}

std::string Property::GetHelp() const
{
	std::string result = {};
	if (MSG_Exists(create_config_name(propname).c_str())) {
		std::string help_text = MSG_Get(create_config_name(propname).c_str());
		// Fill in the default value if the help text contains '%s'.
		if (help_text.find("%s") != std::string::npos) {
			help_text = format_str(help_text,
			                       GetDefaultValue().ToString().c_str());
		}
		result.append(help_text);
	}

	const auto configitem_has_message = [this](const auto& val) {
		return MSG_Exists(create_config_item_name(propname, val).c_str()) ||
		       (iequals(val, propname) &&
		        MSG_Exists(create_config_item_name(propname, {}).c_str()));
	};
	if (std::any_of(enabled_options.begin(),
	                enabled_options.end(),
	                configitem_has_message)) {
		for (const auto& val : enabled_options) {
			if (!result.empty()) {
				result.append("\n");
			}
			if (iequals(val, propname) &&
			    MSG_Exists(create_config_item_name(propname, {}).c_str())) {
				result.append(MSG_Get(
				        create_config_item_name(propname, {}).c_str()));
			} else {
				result.append(MSG_Get(
				        create_config_item_name(propname, val).c_str()));
			}
		}
	}
	if (result.empty()) {
		LOG_WARNING("CONFIG: No help available for '%s'.", propname.c_str());
		return "No help available for '" + propname + "'\n";
	}
	return result;
}

std::string Property::GetHelpUtf8() const
{
	std::string result = {};
	if (MSG_Exists(create_config_name(propname).c_str())) {
		std::string help_text = MSG_GetRaw(create_config_name(propname).c_str());
		// Fill in the default value if the help text contains '%s'.
		if (help_text.find("%s") != std::string::npos) {
			help_text = format_str(help_text,
			                       GetDefaultValue().ToString().c_str());
		}
		result.append(help_text);
	}

	const auto configitem_has_message = [this](const auto& val) {
		return MSG_Exists(create_config_item_name(propname, val).c_str()) ||
		       (iequals(val, propname) &&
		        MSG_Exists(create_config_item_name(propname, {}).c_str()));
	};
	if (std::any_of(enabled_options.begin(),
	                enabled_options.end(),
	                configitem_has_message)) {
		for (const auto& val : enabled_options) {
			if (!result.empty()) {
				result.append("\n");
			}
			if (iequals(val, propname) &&
			    MSG_Exists(create_config_item_name(propname, {}).c_str())) {
				result.append(MSG_GetRaw(
				        create_config_item_name(propname, {}).c_str()));
			} else {
				result.append(MSG_GetRaw(
				        create_config_item_name(propname, val).c_str()));
			}
		}
	}
	if (result.empty()) {
		LOG_WARNING("CONFIG: No help available for '%s'.", propname.c_str());
	}
	return result;
}

bool Prop_int::ValidateValue(const Value& in)
{
	if (IsRestrictedValue()) {
		if (IsValueDeprecated(in)) {
			value = GetAlternateForDeprecatedValue(in);
			return true;
		} else if (IsValidValue(in)) {
			value = in;
			return true;
		} else {
			value = default_value;
			return false;
		}
	}

	// Handle ranges if specified
	const int mi = min_value;
	const int ma = max_value;
	int va       = static_cast<int>(Value(in));

	// No ranges
	if (mi == -1 && ma == -1) {
		value = in;
		return true;
	}

	// Inside range
	if (va >= mi && va <= ma) {
		value = in;
		return true;
	}

	// Outside range, set it to the closest boundary
	if (va > ma) {
		va = ma;
	} else {
		va = mi;
	}

	LOG_WARNING("CONFIG: Invalid '%s' setting: '%s'. "
	            "Value outside of the valid range %s-%s, using '%d'",
	            propname.c_str(),
	            in.ToString().c_str(),
	            min_value.ToString().c_str(),
	            max_value.ToString().c_str(),
	            va);

	value = va;
	return true;
}

bool Prop_int::IsValidValue(const Value& in)
{
	if (IsRestrictedValue()) {
		return Property::IsValidValue(in);
	}

	// LOG_MSG("still used ?");
	// No >= and <= in Value type and == is ambigious
	const int mi = min_value;
	const int ma = max_value;
	int va       = static_cast<int>(Value(in));
	if (mi == -1 && ma == -1) {
		return true;
	}
	if (va >= mi && va <= ma) {
		return true;
	}

	LOG_WARNING(
	        "CONFIG: Invalid '%s' setting: '%s'. "
	        "Value outside of the valid range %s-%s, using '%s'",
	        propname.c_str(),
	        in.ToString().c_str(),
	        min_value.ToString().c_str(),
	        max_value.ToString().c_str(),
	        default_value.ToString().c_str());

	return false;
}

bool Prop_double::SetValue(const std::string& input)
{
	Value val;
	if (!val.SetValue(input, Value::V_DOUBLE)) {
		return false;
	}
	return ValidateValue(val);
}

bool Prop_int::SetValue(const std::string& input)
{
	Value val;
	if (!val.SetValue(input, Value::V_INT)) {
		return false;
	}
	bool is_valid = ValidateValue(val);
	return is_valid;
}

bool Prop_string::SetValue(const std::string& input)
{
	std::string temp(input);

	// Valid values are always case insensitive.
	// If the list of valid values is not specified, the string value could
	// be a path or something similar, which are case sensitive.
	if (IsRestrictedValue()) {
		lowcase(temp);
	}
	Value val(temp, Value::V_STRING);
	return ValidateValue(val);
}

bool Prop_string::IsValidValue(const Value& in)
{
	if (!IsRestrictedValue()) {
		return true;
	}

	// If the property's valid values includes either positive or negative
	// bool strings ("on", "false", etc..), then check if this incoming
	// value is either.
	if (is_positive_bool_valid && has_true(in.ToString())) {
		return true;
	}
	if (is_negative_bool_valid && has_false(in.ToString())) {
		return true;
	}

	for (const auto& val : valid_values) {
		if (val == in) { // Match!
			return true;
		}
		if (val.ToString() == "%u") {
			unsigned int v;
			if (sscanf(in.ToString().c_str(), "%u", &v) == 1) {
				return true;
			}
		}
	}

	LOG_WARNING("CONFIG: Invalid '%s' setting: '%s', using '%s'",
	            propname.c_str(),
	            in.ToString().c_str(),
	            default_value.ToString().c_str());

	return false;
}

bool Prop_path::SetValue(const std::string& input)
{
	// Special version to merge realpath with it

	Value val(input, Value::V_STRING);
	bool is_valid = ValidateValue(val);

	if (input.empty()) {
		realpath.clear();
		return false;
	}

	const auto temp_path = resolve_home(input);

	if (temp_path.is_absolute()) {
		realpath = temp_path;
		return is_valid;
	}

	if (current_config_dir.empty()) {
		realpath = GetConfigDir() / temp_path;
	} else {
		realpath = current_config_dir / temp_path;
	}

	return is_valid;
}

bool Prop_bool::SetValue(const std::string& input)
{
	auto is_valid = value.SetValue(input, Value::V_BOOL);
	if (!is_valid) {
		SetValue(default_value.ToString());

		LOG_WARNING("CONFIG: Invalid '%s' setting: '%s', using '%s'",
		            propname.c_str(),
		            input.c_str(),
		            default_value.ToString().c_str());
	}
	return is_valid;
}

bool Prop_hex::SetValue(const std::string& input)
{
	Value val;
	val.SetValue(input, Value::V_HEX);
	return ValidateValue(val);
}

void PropMultiVal::MakeDefaultValue()
{
	Property* p = section->Get_prop(0);
	if (!p) {
		return;
	}

	int i = 1;

	std::string result = p->GetDefaultValue().ToString();

	while ((p = section->Get_prop(i++))) {
		std::string props = p->GetDefaultValue().ToString();
		if (props.empty()) {
			continue;
		}
		result += separator;
		result += props;
	}

	Value val(result, Value::V_STRING);
	ValidateValue(val);
}

bool PropMultiValRemain::SetValue(const std::string& input)
{
	Value val(input, Value::V_STRING);
	bool is_valid = ValidateValue(val);

	std::string local(input);
	int i = 0, number_of_properties = 0;
	Property* p = section->Get_prop(0);
	// No properties in this section. do nothing
	if (!p) {
		return false;
	}

	while ((section->Get_prop(number_of_properties))) {
		number_of_properties++;
	}

	std::string::size_type loc = std::string::npos;

	while ((p = section->Get_prop(i++))) {
		// trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != std::string::npos) {
			local.erase(0, loc);
		}
		loc            = local.find_first_of(separator);
		std::string in = "";

		// When i == number_of_properties add the total line. (makes
		// more then one string argument possible for parameters of cpu)
		if (loc != std::string::npos && i < number_of_properties) {
			// Separator found
			in = local.substr(0, loc);
			local.erase(0, loc + 1);
		} else if (local.size()) {
			// Last argument or last property
			in = local;
			local.clear();
		}
		// Test Value. If it fails set default
		Value valtest(in, p->Get_type());
		if (!p->IsValidValue(valtest)) {
			MakeDefaultValue();
			return false;
		}
		p->SetValue(in);
	}
	return is_valid;
}

bool PropMultiVal::SetValue(const std::string& input)
{
	Value val(input, Value::V_STRING);
	bool is_valid = ValidateValue(val);

	std::string local(input);
	int i = 0;

	Property* p = section->Get_prop(0);
	if (!p) {
		// No properties in this section; do nothing
		return false;
	}

	Value::Etype prevtype = Value::V_NONE;
	std::string prevargument = "";

	std::string::size_type loc = std::string::npos;
	while ((p = section->Get_prop(i++))) {
		// Trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != std::string::npos) {
			local.erase(0, loc);
		}

		loc = local.find_first_of(separator);

		std::string in = "";

		if (loc != std::string::npos) {
			// Separator found
			in = local.substr(0, loc);
			local.erase(0, loc + 1);
		} else if (local.size()) {
			// Last argument
			in = local;
			local.clear();
		}

		if (p->Get_type() == Value::V_STRING) {
			// Strings are only checked against the valid values
			// list. Test Value. If it fails set default
			Value valtest(in, p->Get_type());
			if (!p->IsValidValue(valtest)) {
				MakeDefaultValue();
				return false;
			}
			p->SetValue(in);

		} else {
			// Non-strings can have more things, conversion alone is
			// not enough (as invalid values as converted to 0)
			bool r = p->SetValue(in);
			if (!r) {
				if (in.empty() && p->Get_type() == prevtype) {
					// Nothing there, but same type of
					// variable, so repeat it (sensitivity)
					in = prevargument;
					p->SetValue(in);
				} else {
					// Something was there to be parsed or
					// not the same type. Invalidate entire
					// property.
					MakeDefaultValue();
				}
			}
		}

		prevtype     = p->Get_type();
		prevargument = std::move(in);
	}

	return is_valid;
}

const std::vector<Value>& Property::GetValues() const
{
	return valid_values;
}

std::vector<Value> Property::GetDeprecatedValues() const
{
	std::vector<Value> values = {};
	std::transform(deprecated_and_alternate_values.begin(),
	               deprecated_and_alternate_values.end(),
	               std::back_inserter(values),
	               [](const auto& kv) { return kv.first; });
	return values;
}

const Value& Property::GetAlternateForDeprecatedValue(const Value& val) const
{
	const auto it = deprecated_and_alternate_values.find(val);
	return (it != deprecated_and_alternate_values.end()) ? it->second
	                                                     : default_value;
}

const std::vector<Value>& PropMultiVal::GetValues() const
{
	Property* p = section->Get_prop(0);

	// No properties in this section. do nothing
	if (!p) {
		return valid_values;
	}
	int i = 0;
	while ((p = section->Get_prop(i++))) {
		std::vector<Value> v = p->GetValues();
		if (!v.empty()) {
			return p->GetValues();
		}
	}
	return valid_values;
}

// When setting a property's list of valid values (For example, composite =
// [auto, on, off]), this function inspects the given valid value to see if it's
// boolean string (ie: "on" or "off"). If so, this records if a boolean is valid
// and its direction (either positive or negative) so we can accept all of those
// corresponding boolean strings from the user (ie: composite = disabled")
//
void Property::MaybeSetBoolValid(const std::string_view valid_value)
{
	if (has_true(valid_value)) {
		is_positive_bool_valid = true;
	} else if (has_false(valid_value)) {
		is_negative_bool_valid = true;
	}
}

void Property::SetDeprecatedWithAlternateValue(const char* deprecated_value,
                                               const char* alternate_value)
{
	assert(deprecated_value);
	assert(alternate_value);
	deprecated_and_alternate_values[deprecated_value] = alternate_value;
}

void Property::Set_values(const std::vector<std::string>& in)
{
	Value::Etype type = default_value.type;
	for (auto& str : in) {
		MaybeSetBoolValid(str);
		Value val(str, type);
		valid_values.push_back(val);
	}
	SetEnabledOptions(in);
}

void Property::SetEnabledOptions(const std::vector<std::string>& in)
{
	enabled_options = in;
}

Prop_int* Section_prop::Add_int(const std::string& _propname,
                                Property::Changeable::Value when, int _value)
{
	Prop_int* test = new Prop_int(_propname, when, _value);
	properties.push_back(test);
	return test;
}

Prop_string* Section_prop::Add_string(const std::string& _propname,
                                      Property::Changeable::Value when,
                                      const char* _value)
{
	Prop_string* test = new Prop_string(_propname, when, _value);
	properties.push_back(test);
	return test;
}

Prop_path* Section_prop::Add_path(const std::string& _propname,
                                  Property::Changeable::Value when, const char* _value)
{
	Prop_path* test = new Prop_path(_propname, when, _value);
	properties.push_back(test);
	return test;
}

Prop_bool* Section_prop::Add_bool(const std::string& _propname,
                                  Property::Changeable::Value when, bool _value)
{
	Prop_bool* test = new Prop_bool(_propname, when, _value);
	properties.push_back(test);
	return test;
}

Prop_hex* Section_prop::Add_hex(const std::string& _propname,
                                Property::Changeable::Value when, Hex _value)
{
	Prop_hex* test = new Prop_hex(_propname, when, _value);
	properties.push_back(test);
	return test;
}

PropMultiVal* Section_prop::AddMultiVal(const std::string& _propname,
                                        Property::Changeable::Value when,
                                        const std::string& sep)
{
	PropMultiVal* test = new PropMultiVal(_propname, when, sep);
	properties.push_back(test);
	return test;
}

PropMultiValRemain* Section_prop::AddMultiValRemain(const std::string& _propname,
                                                    Property::Changeable::Value when,
                                                    const std::string& sep)
{
	PropMultiValRemain* test = new PropMultiValRemain(_propname, when, sep);
	properties.push_back(test);
	return test;
}

int Section_prop::Get_int(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool Section_prop::Get_bool(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return false;
}

double Section_prop::Get_double(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return 0.0;
}

Prop_path* Section_prop::Get_path(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			Prop_path* val = dynamic_cast<Prop_path*>((*tel));
			if (val) {
				return val;
			} else {
				return nullptr;
			}
		}
	}
	return nullptr;
}

PropMultiVal* Section_prop::GetMultiVal(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			PropMultiVal* val = dynamic_cast<PropMultiVal*>((*tel));
			if (val) {
				return val;
			} else {
				return nullptr;
			}
		}
	}
	return nullptr;
}

PropMultiValRemain* Section_prop::GetMultiValRemain(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			PropMultiValRemain* val = dynamic_cast<PropMultiValRemain*>(
			        (*tel));
			if (val) {
				return val;
			} else {
				return nullptr;
			}
		}
	}
	return nullptr;
}

Property* Section_prop::Get_prop(int index)
{
	for (it tel = properties.begin(); tel != properties.end(); ++tel) {
		if (!index--) {
			return (*tel);
		}
	}
	return nullptr;
}

Property* Section_prop::Get_prop(const std::string_view propname)
{
	for (Property* property : properties) {
		if (property->propname == propname) {
			return property;
		}
	}
	return nullptr;
}

std::string Section_prop::Get_string(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return "";
}

Prop_bool* Section_prop::GetBoolProp(const std::string& propname) const
{
	for (const auto property : properties) {
		if (property->propname == propname) {
			return dynamic_cast<Prop_bool*>(property);
		}
	}
	return nullptr;
}


Prop_string* Section_prop::GetStringProp(const std::string& propname) const
{
	for (const auto property : properties) {
		if (property->propname == propname) {
			return dynamic_cast<Prop_string*>(property);
		}
	}
	return nullptr;
}

Hex Section_prop::Get_hex(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool Section_prop::HandleInputline(const std::string& line)
{
	std::string::size_type loc = line.find('=');

	if (loc == std::string::npos) {
		return false;
	}

	std::string name = line.substr(0, loc);
	std::string val  = line.substr(loc + 1);

	// Strip quotes around the value
	trim(val);
	std::string::size_type length = val.length();
	if (length > 1 && ((val[0] == '\"' && val[length - 1] == '\"') ||
	                   (val[0] == '\'' && val[length - 1] == '\''))) {
		val = val.substr(1, length - 2);
	}

	// Trim the result in case there were spaces somewhere
	trim(name);
	trim(val);

	for (auto& p : properties) {
		if (strcasecmp(p->propname.c_str(), name.c_str()) != 0) {
			continue;
		}

		if (p->IsDeprecated()) {
			LOG_WARNING("CONFIG: Deprecated option '%s'\n\n%s\n",
			            name.c_str(),
			            p->GetHelpUtf8().c_str());

			if (!p->IsDeprecatedButAllowed()) {
				return false;
			}
		}

		return p->SetValue(val);
	}

	LOG_WARNING("CONFIG: Invalid option '%s'", name.c_str());
	return false;
}

void Section_prop::PrintData(FILE* outfile) const
{
	// Now print out the individual section entries

	// Determine maximum length of the props in this section
	int len = 0;
	for (const auto& tel : properties) {
		const auto prop_length = check_cast<int>(tel->propname.length());
		len = std::max<int>(len, prop_length);
	}

	for (const auto& tel : properties) {
		if (tel->IsDeprecated()) {
			continue;
		}

		fprintf(outfile,
		        "%-*s = %s\n",
		        std::min<int>(40, len),
		        tel->propname.c_str(),
		        tel->GetValue().ToString().c_str());
	}
}

std::string Section_prop::GetPropValue(const std::string& _property) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if (!strcasecmp((*tel)->propname.c_str(), _property.c_str())) {
			return (*tel)->GetValue().ToString();
		}
	}
	return NO_SUCH_PROPERTY;
}

bool Section_line::HandleInputline(const std::string& line)
{
	if (!data.empty()) {
		// Add return to previous line in buffer
		data += "\n";
	}
	data += line;
	return true;
}

void Section_line::PrintData(FILE* outfile) const
{
	fprintf(outfile, "%s", data.c_str());
}

std::string Section_line::GetPropValue(const std::string&) const
{
	return NO_SUCH_PROPERTY;
}

bool Config::WriteConfig(const std_fs::path& path) const
{
	char temp[50];
	char helpline[256];

	FILE* outfile = fopen(path.string().c_str(), "w+t");
	if (!outfile) {
		return false;
	}

	// Print start of config file and add a return to improve readibility
	fprintf(outfile, MSG_GetRaw("CONFIGFILE_INTRO"), DOSBOX_VERSION);
	fprintf(outfile, "\n");

	for (auto tel = sectionlist.cbegin(); tel != sectionlist.cend(); ++tel) {
		// Print section header
		safe_strcpy(temp, (*tel)->GetName());
		lowcase(temp);
		fprintf(outfile, "[%s]\n", temp);

		Section_prop* sec = dynamic_cast<Section_prop*>(*tel);
		if (sec) {
			Property* p;
			int i           = 0;
			size_t maxwidth = 0;

			while ((p = sec->Get_prop(i++))) {
				maxwidth = std::max(maxwidth, p->propname.length());
			}

			i = 0;

			char prefix[80];
			int intmaxwidth = std::min<int>(60, check_cast<int>(maxwidth));
			safe_sprintf(prefix, "\n# %*s  ", intmaxwidth, "");

			while ((p = sec->Get_prop(i++))) {
				if (p->IsDeprecated()) {
					continue;
				}

				auto help = p->GetHelpUtf8();

				std::string::size_type pos = std::string::npos;

				while ((pos = help.find('\n', pos + 1)) !=
				       std::string::npos) {
					help.replace(pos, 1, prefix);
				}

				// Percentage signs are encoded as '%%' in the
				// config descriptions because they are sent
				// through printf-like functions (e.g.,
				// WriteOut()). So we need to de-escape them before
				// writing them into the config.
				auto s = format_str(help);

				fprintf(outfile,
				        "# %*s: %s",
				        intmaxwidth,
				        p->propname.c_str(),
				        s.c_str());

				auto print_values = [&](const char* values_msg_key,
				                        const std::vector<Value>& values) {
					if (values.empty()) {
						return;
					}
					fprintf(outfile,
					        "%s%s:",
					        prefix,
					        MSG_GetRaw(values_msg_key));

					std::vector<Value>::const_iterator it =
					        values.begin();

					while (it != values.end()) {
						if ((*it).ToString() != "%u") {
							// Hack hack hack. else
							// we need to modify
							// GetValues, but that
							// one is const...
							if (it != values.begin()) {
								fputs(",", outfile);
							}

							fprintf(outfile,
							        " %s",
							        (*it).ToString().c_str());
						}
						++it;
					}
					fprintf(outfile, ".");
				};
				print_values("CONFIG_VALID_VALUES", p->GetValues());
				print_values("CONFIG_DEPRECATED_VALUES", p->GetDeprecatedValues());
				fprintf(outfile, "\n");
			}
		} else {
			upcase(temp);
			strcat(temp, "_CONFIGFILE_HELP");

			const char* helpstr   = MSG_GetRaw(temp);
			const char* linestart = helpstr;
			char* helpwrite       = helpline;

			while (*helpstr && static_cast<size_t>(helpstr - linestart) <
			                           sizeof(helpline)) {
				*helpwrite++ = *helpstr;

				if (*helpstr == '\n') {
					*helpwrite = 0;

					fprintf(outfile, "# %s", helpline);

					helpwrite = helpline;
					linestart = ++helpstr;
				} else {
					++helpstr;
				}
			}
		}

		fprintf(outfile, "\n");
		(*tel)->PrintData(outfile);

		// Always add empty line between sections
		fprintf(outfile, "\n");
	}

	fclose(outfile);
	return true;
}

Section_prop* Config::AddInactiveSectionProp(const char* section_name)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");

	constexpr bool inactive = false;
	auto s = std::make_unique<Section_prop>(section_name, inactive);
	auto* section = s.get();
	sectionlist.push_back(s.release());
	return section;
}

Section_prop* Config::AddSection_prop(const char* section_name, SectionFunction func,
                                      bool changeable_at_runtime)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");

	Section_prop* s = new Section_prop(section_name);
	s->AddInitFunction(func, changeable_at_runtime);
	sectionlist.push_back(s);
	return s;
}

Section_prop::~Section_prop()
{
	// ExecuteDestroy should be here else the destroy functions use
	// destroyed properties
	ExecuteDestroy(true);

	// Delete properties themself (properties stores the pointer of a prop
	for (it prop = properties.begin(); prop != properties.end(); ++prop) {
		delete (*prop);
	}
}

Section_line* Config::AddSection_line(const char* section_name, SectionFunction func)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");

	Section_line* blah = new Section_line(section_name);
	blah->AddInitFunction(func);
	sectionlist.push_back(blah);

	return blah;
}

// Move assignment operator
Config& Config::operator=(Config&& source) noexcept
{
	if (this == &source) {
		return *this;
	}

	// Move each member
	cmdline              = std::move(source.cmdline);
	sectionlist          = std::move(source.sectionlist);
	_start_function      = std::move(source._start_function);
	secure_mode          = std::move(source.secure_mode);
	startup_params       = std::move(source.startup_params);
	configfiles          = std::move(source.configfiles);
	configFilesCanonical = std::move(source.configFilesCanonical);

	overwritten_autoexec_section = std::move(source.overwritten_autoexec_section);
	overwritten_autoexec_conf = std::move(source.overwritten_autoexec_conf);

	// Hollow-out the source
	source.cmdline                      = {};
	source.overwritten_autoexec_section = {};
	source.overwritten_autoexec_conf    = {};
	source._start_function              = {};
	source.secure_mode                  = {};
	source.startup_params               = {};
	source.configfiles                  = {};
	source.configFilesCanonical         = {};

	return *this;
}

// Move constructor, leverages move by assignment
Config::Config(Config&& source) noexcept
{
	*this = std::move(source);
}

void Config::Init() const
{
	for (const auto& sec : sectionlist) {
		sec->ExecuteInit();
	}
}

void Section::AddInitFunction(SectionFunction func, bool changeable_at_runtime)
{
	if (func) {
		init_functions.emplace_back(func, changeable_at_runtime);
	}
}

void Section::AddDestroyFunction(SectionFunction func, bool changeable_at_runtime)
{
	destroyfunctions.emplace_front(func, changeable_at_runtime);
}

void Section::ExecuteInit(const bool init_all)
{
	for (size_t i = 0; i < init_functions.size(); ++i) {
		// Can we skip calling this function?
		if (!(init_all || init_functions[i].changeable_at_runtime)) {
			continue;
		}

		// Track the size of our container because it might grow.
		const auto size_on_entry = init_functions.size();

		assert(init_functions[i].function);
		init_functions[i].function(this);

		const auto size_on_exit = init_functions.size();

		if (size_on_exit > size_on_entry) {
			//
			// If the above function call appended items then we
			// need to avoid calling them immediately in this
			// current pass by advancing our index across them. The
			// setup class will call the added function itself.
			//
			const auto num_appended = size_on_exit - size_on_entry;
			i += num_appended;
			assert(i < init_functions.size());
		}
	}
}

void Section::ExecuteDestroy(bool destroyall)
{
	typedef std::deque<Function_wrapper>::iterator func_it;

	for (func_it tel = destroyfunctions.begin(); tel != destroyfunctions.end();) {
		if (destroyall || (*tel).changeable_at_runtime) {
			(*tel).function(this);

			// Remove destroyfunctions once used
			tel = destroyfunctions.erase(tel);
		} else {
			++tel;
		}
	}
}

Config::~Config()
{
	for (auto cnt = sectionlist.rbegin(); cnt != sectionlist.rend(); ++cnt) {
		delete (*cnt);
	}
}

Section* Config::GetSection(const std::string_view section_name) const
{
	for (auto* el : sectionlist) {
		if (iequals(el->GetName(), section_name)) {
			return el;
		}
	}
	return nullptr;
}

Section* Config::GetSectionFromProperty(const char* prop) const
{
	for (auto* el : sectionlist) {
		if (el->GetPropValue(prop) != NO_SUCH_PROPERTY) {
			return el;
		}
	}
	return nullptr;
}

void Config::OverwriteAutoexec(const std::string& conf, const std::string& line)
{
	// If we're in a new config file, then record that filename and reset
	// the section
	if (overwritten_autoexec_conf != conf) {
		overwritten_autoexec_conf = conf;
		overwritten_autoexec_section.data.clear();
	}
	overwritten_autoexec_section.HandleInputline(line);
}

const std::string& Config::GetOverwrittenAutoexecConf() const
{
	return overwritten_autoexec_conf;
}

const Section_line& Config::GetOverwrittenAutoexecSection() const
{
	return overwritten_autoexec_section;
}

bool Config::ParseConfigFile(const std::string& type,
                             const std::string& config_file_name)
{
	std::error_code ec;
	const std_fs::path cfg_path = config_file_name;
	const auto canonical_path   = std_fs::canonical(cfg_path, ec);

	if (ec) {
		return false;
	}

	if (contains(configFilesCanonical, canonical_path)) {
		LOG_INFO("CONFIG: Skipping duplicate config file '%s'",
		         config_file_name.c_str());
		return true;
	}

	std::ifstream in(canonical_path);
	if (!in) {
		return false;
	}

	configfiles.push_back(config_file_name);
	configFilesCanonical.push_back(canonical_path);

	// Get directory from config_file_name, used with relative paths.
	current_config_dir = canonical_path.parent_path();

	// If this is an autoexec section, the above takes care of the joining
	// while this handles the overwrriten mode. We need to be prepared for
	// either scenario to play out because we won't know the users final
	// preference until the very last configuration file is processed.

	std::string line = {};

	Section* current_section = nullptr;
	bool is_autoexec_section = false;
	bool is_autoexec_started = false;

	auto is_empty_line = [](const std::string& line) {
		return line.empty() || line[0] == '\0' || line[0] == '\n' ||
		       line[0] == '\r';
	};

	auto is_comment = [](const std::string& line) {
		return !line.empty() && (line[0] == '%' || line[0] == '#');
	};

	auto is_section_start = [](const std::string& line) {
		return !line.empty() && line[0] == '[';
	};

	auto handle_autoexec_line = [&]() {
		// Ignore all the empty lines until the meaningful [autoexec]
		// content starts
		if (!is_autoexec_started) {
			if (is_empty_line(line) || is_comment(line)) {
				return;
			}
			is_autoexec_started = true;
		}

		if (!is_comment(line)) {
			current_section->HandleInputline(line);
			OverwriteAutoexec(config_file_name, line);
		}
	};

	while (getline(in, line)) {
		trim(line);

		if (is_section_start(line)) {
			is_autoexec_section = false;
			is_autoexec_started = false;
		}

		// Special handling of [autoexec] section
		if (is_autoexec_section) {
			handle_autoexec_line();
			continue;
		}

		// Strip leading/trailing whitespace, skip unnecessary lines
		if (is_empty_line(line) || is_comment(line)) {
			continue;
		}

		if (is_section_start(line)) {
			// New section
			const auto bracket_pos = line.find(']');
			if (bracket_pos == std::string::npos) {
				continue;
			}
			line.erase(bracket_pos);
			const auto section_name = line.substr(1);
			if (const auto sec = GetSection(section_name); sec) {
				current_section = sec;
				is_autoexec_section = (section_name == "autoexec");
			}
		} else if (current_section) {
			current_section->HandleInputline(line);
		}
	}

	// So internal changes don't use the path information
	current_config_dir.clear();

	LOG_INFO("CONFIG: Loaded %s config file '%s'",
	         type.c_str(),
	         config_file_name.c_str());

	return true;
}

parse_environ_result_t parse_environ(const char* const* envp) noexcept
{
	assert(envp);

	// Filter environment variables in following format:
	// DOSBOX_SECTIONNAME_PROPNAME=VALUE (prefix, section, and property
	// names are case-insensitive).
	std::list<std::tuple<std::string, std::string>> props_to_set;

	for (const char* const* str = envp; *str; str++) {
		const char* env_var = *str;
		if (strncasecmp(env_var, "DOSBOX_", 7) != 0) {
			continue;
		}

		const std::string rest       = (env_var + 7);
		const auto section_delimiter = rest.find('_');
		if (section_delimiter == std::string::npos) {
			continue;
		}

		const auto section_name = rest.substr(0, section_delimiter);
		if (section_name.empty()) {
			continue;
		}

		const auto prop_name_and_value = rest.substr(section_delimiter + 1);
		if (prop_name_and_value.empty() || !isalpha(prop_name_and_value[0])) {
			continue;
		}

		props_to_set.emplace_back(
		        std::make_tuple(section_name, prop_name_and_value));
	}

	return props_to_set;
}

std::optional<bool> parse_bool_setting(const std::string_view setting)
{
	static const std::unordered_map<std::string_view, bool> mapping{
	        {"enabled", true},
	        {"true", true},
	        {"on", true},
	        {"yes", true},

	        {"disabled", false},
	        {"false", false},
	        {"off", false},
	        {"no", false},
	        {"none", false},
	};

	const auto it = mapping.find(setting);
	return it != mapping.end() ? std::optional<bool>(it->second) : std::nullopt;
}

bool has_true(const std::string_view setting)
{
	const auto has_bool = parse_bool_setting(setting);
	return (has_bool && *has_bool == true);
}

bool has_false(const std::string_view setting)
{
	const auto has_bool = parse_bool_setting(setting);
	return (has_bool && *has_bool == false);
}

void set_section_property_value(const std::string_view section_name,
                                const std::string_view property_name,
                                const std::string_view property_value)
{
	auto* sect_updater = static_cast<Section_prop*>(
	        control->GetSection(section_name));
	assertm(sect_updater, "Invalid section name");

	auto* property = sect_updater->Get_prop(property_name);
	assertm(property, "Invalid property name");

	property->SetValue(std::string(property_value));
}

void Config::ParseEnv()
{
#if defined(_MSC_VER) || (defined(__MINGW32__) && defined(__clang__))
	const char* const* envp = _environ;
#else
	const char* const* envp = environ;
#endif
	if (envp == nullptr) {
		return;
	}

	for (const auto& set_prop_desc : parse_environ(envp)) {
		const auto section_name = std::get<0>(set_prop_desc);

		Section* sec = GetSection(section_name);

		if (!sec) {
			continue;
		}

		const auto prop_name_and_value = std::get<1>(set_prop_desc);
		sec->HandleInputline(prop_name_and_value);
	}
}

void Config::SetStartUp(void (*_function)(void))
{
	_start_function = _function;
}

void Config::StartUp()
{
	(*_start_function)();
}

Verbosity Config::GetStartupVerbosity() const
{
	const Section* s = GetSection("dosbox");
	assert(s);
	const std::string user_choice = s->GetPropValue("startup_verbosity");

	if (user_choice == "high") {
		return Verbosity::High;
	}
	if (user_choice == "low") {
		return Verbosity::Low;
	}
	if (user_choice == "quiet") {
		return Verbosity::Quiet;
	}
	if (user_choice == "auto") {
		return (cmdline->HasDirectory() || cmdline->HasExecutableName())
		             ? Verbosity::InstantLaunch
		             : Verbosity::High;
	}

	LOG_WARNING("SETUP: Invalid 'startup_verbosity' setting: '%s', using 'high'",
	            user_choice.c_str());
	return Verbosity::High;
}

const std::string& Config::GetArgumentLanguage()
{
	return arguments.lang;
}

// forward declaration
void MSG_Init(Section_prop*);

// Parse the user's configuration files starting with the primary, then custom
// -conf's, and finally the local dosbox.conf
void Config::ParseConfigFiles(const std_fs::path& config_dir)
{
	// First: parse the user's primary 'dosbox-staging.conf' config file
	const bool load_primary_config = !arguments.noprimaryconf;

	if (load_primary_config) {
		const auto config_path = config_dir / GetPrimaryConfigName();
		ParseConfigFile("primary", config_path.string());
	}

	// Second: parse the local 'dosbox.conf', if present
	const bool load_local_config = !arguments.nolocalconf;

	if (load_local_config) {
		ParseConfigFile("local", "dosbox.conf");
	}

	// Finally: layer on additional config files specified with the '-conf'
	// switch
	for (const auto& conf_file : arguments.conf) {
		if (!ParseConfigFile("custom", conf_file)) {
			// Try to load it from the user directory
			const auto cfg = config_dir / conf_file;
			if (!ParseConfigFile("custom", cfg.string())) {
				LOG_WARNING("CONFIG: Can't open custom config file '%s'",
				            conf_file.c_str());
			}
		}
	}

	// Once we've parsed all the potential config files, we've down our best
	// to discover the user's desired language. At this point, we can now
	// initialise the messaging system which honours the language and loads
	// those messages.
	if (const auto sec = GetSection("dosbox"); sec) {
		MSG_Init(static_cast<Section_prop*>(sec));
	}
}

static char return_msg[200];
const char* Config::SetProp(std::vector<std::string>& pvars)
{
	*return_msg = 0;

	// Attempt to split off the first word
	std::string::size_type spcpos = pvars[0].find_first_of(' ');
	std::string::size_type equpos = pvars[0].find_first_of('=');

	if ((equpos != std::string::npos) &&
	    ((spcpos == std::string::npos) || (equpos < spcpos))) {
		// If we have a '=' possibly before a ' ' split on the =
		pvars.insert(pvars.begin() + 1, pvars[0].substr(equpos + 1));
		pvars[0].erase(equpos);

		// As we had a = the first thing must be a property now
		Section* sec = GetSectionFromProperty(pvars[0].c_str());

		if (sec) {
			pvars.insert(pvars.begin(), std::string(sec->GetName()));
		} else {
			safe_sprintf(return_msg,
			             MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
			             pvars[0].c_str());
			return return_msg;
		}
		// Order in the vector should be ok now
	} else {
		if ((spcpos != std::string::npos) &&
		    ((equpos == std::string::npos) || (spcpos < equpos))) {
			// ' ' before a possible '=', split on the ' '
			pvars.insert(pvars.begin() + 1, pvars[0].substr(spcpos + 1));
			pvars[0].erase(spcpos);
		}

		// Check if the first parameter is a section or property
		Section* sec = GetSection(pvars[0]);

		if (!sec) {
			// Not a section: little duplicate from above
			Section* secprop = GetSectionFromProperty(
			        pvars[0].c_str());

			if (secprop) {
				pvars.insert(pvars.begin(),
				             std::string(secprop->GetName()));
			} else {
				safe_sprintf(return_msg,
				             MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
				             pvars[0].c_str());
				return return_msg;
			}
		} else {
			// First of pvars is most likely a section, but could
			// still be gus have a look at the second parameter
			if (pvars.size() < 2) {
				safe_strcpy(return_msg,
				            MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return return_msg;
			}

			std::string::size_type spcpos2 = pvars[1].find_first_of(' ');
			std::string::size_type equpos2 = pvars[1].find_first_of('=');

			if ((equpos2 != std::string::npos) &&
			    ((spcpos2 == std::string::npos) || (equpos2 < spcpos2))) {
				// Split on the =
				pvars.insert(pvars.begin() + 2,
				             pvars[1].substr(equpos2 + 1));
				pvars[1].erase(equpos2);

			} else if ((spcpos2 != std::string::npos) &&
			           ((equpos2 == std::string::npos) ||
			            (spcpos2 < equpos2))) {
				// Split on the ' '
				pvars.insert(pvars.begin() + 2,
				             pvars[1].substr(spcpos2 + 1));
				pvars[1].erase(spcpos2);
			}

			// Is this a property?
			Section* sec2 = GetSectionFromProperty(
			        pvars[1].c_str());

			if (!sec2) {
				// Not a property
				Section* sec3 = GetSectionFromProperty(
				        pvars[0].c_str());
				if (sec3) {
					// Section and property name are identical
					pvars.insert(pvars.begin(), pvars[0]);
				}
				// else has been checked above already
			}
		}
	}

	if (pvars.size() < 3) {
		safe_strcpy(return_msg, MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
		return return_msg;
	}

	// Check if the property actually exists in the section
	Section* sec2 = GetSectionFromProperty(pvars[1].c_str());
	if (!sec2) {
		safe_sprintf(return_msg,
		             MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
		             pvars[1].c_str(),
		             pvars[0].c_str());
		return return_msg;
	}

	return return_msg;
}

void Config::ParseArguments()
{
	arguments.printconf = cmdline->FindRemoveBoolArgument("printconf");
	arguments.noprimaryconf = cmdline->FindRemoveBoolArgument("noprimaryconf");
	arguments.nolocalconf = cmdline->FindRemoveBoolArgument("nolocalconf");
	arguments.fullscreen  = cmdline->FindRemoveBoolArgument("fullscreen");
	arguments.list_countries = cmdline->FindRemoveBoolArgument("list-countries");
	arguments.list_layouts = cmdline->FindRemoveBoolArgument("list-layouts");
	arguments.list_glshaders = cmdline->FindRemoveBoolArgument("list-glshaders");
	arguments.noconsole   = cmdline->FindRemoveBoolArgument("noconsole");
	arguments.startmapper = cmdline->FindRemoveBoolArgument("startmapper");
	arguments.exit        = cmdline->FindRemoveBoolArgument("exit");
	arguments.securemode = cmdline->FindRemoveBoolArgument("securemode");
	arguments.noautoexec = cmdline->FindRemoveBoolArgument("noautoexec");

	arguments.eraseconf = cmdline->FindRemoveBoolArgument("eraseconf") ||
	                      cmdline->FindRemoveBoolArgument("resetconf");
	arguments.erasemapper = cmdline->FindRemoveBoolArgument("erasemapper") ||
	                        cmdline->FindRemoveBoolArgument("resetmapper");

	arguments.version = cmdline->FindRemoveBoolArgument("version", 'v');
	arguments.help    = (cmdline->FindRemoveBoolArgument("help", 'h') ||
	                     cmdline->FindRemoveBoolArgument("help", '?'));

	arguments.working_dir = cmdline->FindRemoveStringArgument("working-dir");
	arguments.lang = cmdline->FindRemoveStringArgument("lang");
	arguments.machine = cmdline->FindRemoveStringArgument("machine");

	arguments.socket = cmdline->FindRemoveIntArgument("socket");

	arguments.conf = cmdline->FindRemoveVectorArgument("conf");
	arguments.set  = cmdline->FindRemoveVectorArgument("set");

	arguments.editconf = cmdline->FindRemoveOptionalArgument("editconf");
}

// Only checks if config file exists and is not empty
bool config_file_is_valid(const std_fs::path& path)
{
	FILE* file = fopen(path.string().c_str(), "r");
	if (!file) {
		return false;
	}

	constexpr size_t BufferSize = 4096;
	char buffer[BufferSize];
	size_t bytes_read;
	do {
		bytes_read = fread(buffer, 1, BufferSize, file);
		for (size_t i = 0; i < bytes_read; ++i) {
			if (!isspace(buffer[i])) {
				fclose(file);
				return true;
			}
		}
	} while (bytes_read == BufferSize);

	fclose(file);
	return false;
}
