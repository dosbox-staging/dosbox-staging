// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config/setup.h"

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

#include "config/config.h"
#include "misc/ansi_code_markup.h"
#include "misc/console.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

// Commonly accessed global that holds configuration records
ConfigPtr control = {};

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
		LOG_ERR("CONFIG: Comparing stuff that doesn't make sense");
		break;
	}
	return false;
}

bool Value::operator==(const Hex& other) const
{
	return this->_hex == other;
}

bool Value::operator<(const Value& other) const
{
	return std::tie(_hex, _bool, _int, _string, _double) <
	       std::tie(other._hex, other._bool, other._int, other._string, other._double);
}

bool Value::SetValue(const std::string& _value, const Etype _type)
{
	assert(type == V_NONE || type == _type);
	type = _type;

	bool is_valid = true;
	switch (type) {
	case V_HEX: is_valid = SetHex(_value); break;
	case V_INT: is_valid = SetInt(_value); break;
	case V_BOOL: is_valid = SetBool(_value); break;
	case V_STRING: SetString(_value); break;
	case V_DOUBLE: is_valid = SetDouble(_value); break;

	case V_NONE:
	case V_CURRENT:
	default:
		LOG_ERR("CONFIG: Unhandled type when setting value: '%s'",
		        _value.c_str());
		is_valid = false;
		break;
	}
	return is_valid;
}

bool Value::SetHex(const std::string& _value)
{
	std::istringstream input(_value);
	input.flags(std::ios::hex);
	int result = INT_MIN;
	input >> result;
	if (result == INT_MIN) {
		return false;
	}
	_hex = result;
	return true;
}

bool Value::SetInt(const std::string& _value)
{
	std::istringstream input(_value);
	int result = INT_MIN;
	input >> result;
	if (result == INT_MIN) {
		return false;
	}
	_int = result;
	return true;
}

bool Value::SetDouble(const std::string& _value)
{
	std::istringstream input(_value);
	double result = std::numeric_limits<double>::max();
	input >> result;
	if (result == std::numeric_limits<double>::max()) {
		return false;
	}
	_double = result;
	return true;
}

// Sets the '_bool' member variable to either the parsed boolean value or false
// if it couldn't be parsed. Returns true if the provided string was parsed.
bool Value::SetBool(const std::string& _value)
{
	auto lowercase_value = _value;
	lowcase(lowercase_value);
	const auto parsed = parse_bool_setting(lowercase_value);

	_bool = parsed ? *parsed : false;

	return parsed.has_value();
}

void Value::SetString(const std::string& _value)
{
	_string = _value;
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
	case V_BOOL: oss << (_bool ? "on" : "off"); break;
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

bool Property::IsValidValue(const Value& _value)
{
	if (!IsRestrictedValue()) {
		return true;
	}

	for (const_iter it = valid_values.begin(); it != valid_values.end(); ++it) {
		if ((*it) == _value) {
			return true;
		}
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "PROGRAM_CONFIG_INVALID_SETTING",
	                      propname.c_str(),
	                      _value.ToString().c_str(),
	                      default_value.ToString().c_str());

	return false;
}

bool Property::IsValueDeprecated(const Value& val) const
{
	const auto is_deprecated = contains(deprecated_and_alternate_values, val);

	if (is_deprecated) {
		NOTIFY_DisplayWarning(
		        Notification::Source::Console,
		        "CONFIG",
		        "PROGRAM_CONFIG_DEPRECATED_SETTING_VALUE",
		        propname.c_str(),
		        val.ToString().c_str(),
		        GetAlternateForDeprecatedValue(val).ToString().c_str());
	}
	return is_deprecated;
}

bool Property::ValidateValue(const Value& _value)
{
	if (IsValueDeprecated(_value)) {
		value = GetAlternateForDeprecatedValue(_value);
		return true;
	} else if (IsValidValue(_value)) {
		value = _value;
		return true;
	} else {
		value = default_value;
		return false;
	}
}

static std::string create_setting_help_msg_name(const std::string& propname)
{
	std::string result = "CONFIG_" + propname;
	upcase(result);
	return result;
}

void Property::SetHelp(const std::string& help_text)
{
	MSG_Add(create_setting_help_msg_name(propname), help_text);
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

void Property::SetOptionHelp(const std::string& option, const std::string& help_text)
{
	MSG_Add(create_config_item_name(propname, option), help_text);
}

void Property::SetOptionHelp(const std::string& help_text)
{
	MSG_Add(create_config_item_name(propname, {}), help_text);
}

std::string Property::GetHelp() const
{
	std::string result = {};
	if (MSG_Exists(create_setting_help_msg_name(propname))) {

		auto help_text = MSG_Get(create_setting_help_msg_name(propname));

		// Fill in the default value if the help text contains '%s'.
		if (help_text.find("%s") != std::string::npos) {
			help_text = format_str(help_text,
			                       GetDefaultValue().ToString().c_str());
		}
		result.append(help_text);
	}

	const auto configitem_has_message = [this](const auto& val) {
		return MSG_Exists(create_config_item_name(propname, val)) ||
		       (iequals(val, propname) &&
		        MSG_Exists(create_config_item_name(propname, {})));
	};
	if (std::any_of(enabled_options.begin(),
	                enabled_options.end(),
	                configitem_has_message)) {
		for (const auto& val : enabled_options) {
			if (!result.empty()) {
				result.append("\n");
			}
			if (iequals(val, propname) &&
			    MSG_Exists(create_config_item_name(propname, {}))) {
				result.append(MSG_Get(
				        create_config_item_name(propname, {})));
			} else {
				result.append(MSG_Get(
				        create_config_item_name(propname, val)));
			}
		}
	}
	if (result.empty()) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "CONFIG",
		                      "PROGRAM_CONFIG_NO_HELP",
		                      propname.c_str());

		return "No help available for '" + propname + "'\n";
	}
	return result;
}

std::string Property::GetHelpRaw() const
{
	std::string result = {};
	if (MSG_Exists(create_setting_help_msg_name(propname))) {

		auto help_text = MSG_GetTranslatedRaw(
		        create_setting_help_msg_name(propname));

		// Fill in the default value if the help text contains '%s'.
		if (help_text.find("%s") != std::string::npos) {
			help_text = format_str(help_text,
			                       GetDefaultValue().ToString().c_str());
		}
		result.append(help_text);
	}

	const auto configitem_has_message = [this](const auto& val) {
		return MSG_Exists(create_config_item_name(propname, val)) ||
		       (iequals(val, propname) &&
		        MSG_Exists(create_config_item_name(propname, {})));
	};
	if (std::any_of(enabled_options.begin(),
	                enabled_options.end(),
	                configitem_has_message)) {
		for (const auto& val : enabled_options) {
			if (!result.empty()) {
				result.append("\n");
			}
			if (iequals(val, propname) &&
			    MSG_Exists(create_config_item_name(propname, {}))) {
				result.append(MSG_GetTranslatedRaw(
				        create_config_item_name(propname, {})));
			} else {
				result.append(MSG_GetTranslatedRaw(
				        create_config_item_name(propname, val)));
			}
		}
	}
	if (result.empty()) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "CONFIG",
		                      "PROGRAM_CONFIG_NO_HELP",
		                      propname.c_str());
	}
	return result;
}

bool PropInt::ValidateValue(const Value& _value)
{
	if (IsRestrictedValue()) {
		if (IsValueDeprecated(_value)) {
			value = GetAlternateForDeprecatedValue(_value);
			return true;
		} else if (IsValidValue(_value)) {
			value = _value;
			return true;
		} else {
			value = default_value;
			return false;
		}
	}

	// Handle ranges if specified
	const int mi = min_value;
	const int ma = max_value;

	int va = static_cast<int>(Value(_value));

	// No ranges
	if (mi == -1 && ma == -1) {
		value = _value;
		return true;
	}

	// Inside range
	if (va >= mi && va <= ma) {
		value = _value;
		return true;
	}

	// Outside range, set it to the closest boundary
	if (va > ma) {
		va = ma;
	} else {
		va = mi;
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "PROGRAM_CONFIG_SETTING_OUTSIDE_VALID_RANGE",
	                      propname.c_str(),
	                      _value.ToString().c_str(),
	                      min_value.ToString().c_str(),
	                      max_value.ToString().c_str(),
	                      std::to_string(va).c_str());

	value = va;
	return true;
}

bool PropInt::IsValidValue(const Value& _value)
{
	if (IsRestrictedValue()) {
		return Property::IsValidValue(_value);
	}

	// LOG_MSG("still used ?");
	// No >= and <= in Value type and == is ambigious
	const int mi = min_value;
	const int ma = max_value;

	int va = static_cast<int>(Value(_value));

	if (mi == -1 && ma == -1) {
		return true;
	}
	if (va >= mi && va <= ma) {
		return true;
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "PROGRAM_CONFIG_SETTING_OUTSIDE_VALID_RANGE",
	                      propname.c_str(),
	                      _value.ToString().c_str(),
	                      min_value.ToString().c_str(),
	                      max_value.ToString().c_str(),
	                      default_value.ToString().c_str());

	return false;
}

bool PropDouble::SetValue(const std::string& input)
{
	Value val;
	if (!val.SetValue(input, Value::V_DOUBLE)) {
		return false;
	}
	return ValidateValue(val);
}

bool PropInt::SetValue(const std::string& input)
{
	Value val;
	if (!val.SetValue(input, Value::V_INT)) {
		return false;
	}
	bool is_valid = ValidateValue(val);
	return is_valid;
}

bool PropString::SetValue(const std::string& input)
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

bool PropString::IsValidValue(const Value& _value)
{
	if (!IsRestrictedValue()) {
		return true;
	}

	// If the property's valid values includes either positive or negative
	// bool strings ("on", "false", etc..), then check if this incoming
	// value is either.
	if (is_positive_bool_valid && has_true(_value.ToString())) {
		return true;
	}
	if (is_negative_bool_valid && has_false(_value.ToString())) {
		return true;
	}

	for (const auto& val : valid_values) {
		if (val == _value) { // Match!
			return true;
		}
		if (val.ToString() == "%u") {
			unsigned int v;
			if (sscanf(_value.ToString().c_str(), "%u", &v) == 1) {
				return true;
			}
		}
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "PROGRAM_CONFIG_INVALID_SETTING",
	                      propname.c_str(),
	                      _value.ToString().c_str(),
	                      default_value.ToString().c_str());

	return false;
}

bool PropPath::SetValue(const std::string& input)
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

bool PropBool::SetValue(const std::string& input)
{
	auto is_valid = value.SetValue(input, Value::V_BOOL);
	if (!is_valid) {
		SetValue(default_value.ToString());

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "CONFIG",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      propname.c_str(),
		                      input.c_str(),
		                      default_value.ToString().c_str());
	}
	return is_valid;
}

bool PropHex::SetValue(const std::string& input)
{
	Value val;
	val.SetValue(input, Value::V_HEX);
	return ValidateValue(val);
}

void PropMultiVal::MakeDefaultValue()
{
	Property* p = section->GetProperty(0);
	if (!p) {
		return;
	}

	int i = 1;

	std::string result = p->GetDefaultValue().ToString();

	while ((p = section->GetProperty(i++))) {
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

bool PropMultiValRemain::SetValue(const std::string& _value)
{
	Value val(_value, Value::V_STRING);
	bool is_valid = ValidateValue(val);

	std::string local(_value);
	int i                    = 0;
	int number_of_properties = 0;

	Property* p = section->GetProperty(0);

	// No properties in this section. do nothing
	if (!p) {
		return false;
	}

	while ((section->GetProperty(number_of_properties))) {
		number_of_properties++;
	}

	std::string::size_type loc = std::string::npos;

	while ((p = section->GetProperty(i++))) {
		// trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != std::string::npos) {
			local.erase(0, loc);
		}

		loc = local.find_first_of(separator);

		std::string curr_value = "";

		// When i == number_of_properties add the total line. (makes
		// more then one string argument possible for parameters of cpu)
		if (loc != std::string::npos && i < number_of_properties) {
			// Separator found
			curr_value = local.substr(0, loc);
			local.erase(0, loc + 1);

		} else if (local.size()) {
			// Last argument or last property
			curr_value = local;
			local.clear();
		}

		// Test Value. If it fails set default
		const Value valtest(curr_value, p->GetType());

		if (!p->IsValidValue(valtest)) {
			MakeDefaultValue();
			return false;
		}
		p->SetValue(curr_value);
	}
	return is_valid;
}

bool PropMultiVal::SetValue(const std::string& _value)
{
	Value val(_value, Value::V_STRING);
	bool is_valid = ValidateValue(val);

	std::string local(_value);
	int i = 0;

	Property* p = section->GetProperty(0);
	if (!p) {
		// No properties in this section; do nothing
		return false;
	}

	Value::Etype prevtype    = Value::V_NONE;
	std::string prevargument = "";

	std::string::size_type loc = std::string::npos;
	while ((p = section->GetProperty(i++))) {
		// Trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != std::string::npos) {
			local.erase(0, loc);
		}

		loc = local.find_first_of(separator);

		std::string curr_value = "";

		if (loc != std::string::npos) {
			// Separator found
			curr_value = local.substr(0, loc);
			local.erase(0, loc + 1);
		} else if (local.size()) {
			// Last argument
			curr_value = local;
			local.clear();
		}

		if (p->GetType() == Value::V_STRING) {
			// Strings are only checked against the valid values
			// list. Test Value. If it fails set default
			Value valtest(curr_value, p->GetType());
			if (!p->IsValidValue(valtest)) {
				MakeDefaultValue();
				return false;
			}
			p->SetValue(curr_value);

		} else {
			// Non-strings can have more things, conversion alone is
			// not enough (as invalid values as converted to 0)
			bool r = p->SetValue(curr_value);
			if (!r) {
				if (curr_value.empty() && p->GetType() == prevtype) {
					// Nothing there, but same type of
					// variable, so repeat it (sensitivity)
					curr_value = prevargument;
					p->SetValue(curr_value);
				} else {
					// Something was there to be parsed or
					// not the same type. Invalidate entire
					// property.
					MakeDefaultValue();
				}
			}
		}

		prevtype     = p->GetType();
		prevargument = std::move(curr_value);
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

void Property::SetQueueableValue(std::string&& value)
{
	assert(!value.empty());
	queueable_value = std::move(value);
}

const std::optional<std::string>& Property::GetQueuedValue() const
{
	return queueable_value;
}

const Value& Property::GetAlternateForDeprecatedValue(const Value& val) const
{
	const auto it = deprecated_and_alternate_values.find(val);
	return (it != deprecated_and_alternate_values.end()) ? it->second
	                                                     : default_value;
}

const std::vector<Value>& PropMultiVal::GetValues() const
{
	Property* p = section->GetProperty(0);

	// No properties in this section. do nothing
	if (!p) {
		return valid_values;
	}
	int i = 0;
	while ((p = section->GetProperty(i++))) {
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

void Property::SetValues(const std::vector<std::string>& values)
{
	Value::Etype type = default_value.type;
	for (auto& str : values) {
		MaybeSetBoolValid(str);
		Value val(str, type);
		valid_values.push_back(val);
	}
	SetEnabledOptions(values);
}

void Property::SetEnabledOptions(const std::vector<std::string>& options)
{
	enabled_options = options;
}

PropInt* SectionProp::AddInt(const std::string& _propname,
                             Property::Changeable::Value when, int _value)
{
	PropInt* test = new PropInt(_propname, when, _value);
	properties.push_back(test);
	return test;
}

PropString* SectionProp::AddString(const std::string& _propname,
                                   Property::Changeable::Value when,
                                   const char* _value)
{
	PropString* test = new PropString(_propname, when, _value);
	properties.push_back(test);
	return test;
}

PropPath* SectionProp::AddPath(const std::string& _propname,
                               Property::Changeable::Value when, const char* _value)
{
	PropPath* test = new PropPath(_propname, when, _value);
	properties.push_back(test);
	return test;
}

PropBool* SectionProp::AddBool(const std::string& _propname,
                               Property::Changeable::Value when, bool _value)
{
	PropBool* test = new PropBool(_propname, when, _value);
	properties.push_back(test);
	return test;
}

PropHex* SectionProp::AddHex(const std::string& _propname,
                             Property::Changeable::Value when, Hex _value)
{
	PropHex* test = new PropHex(_propname, when, _value);
	properties.push_back(test);
	return test;
}

PropMultiVal* SectionProp::AddMultiVal(const std::string& _propname,
                                       Property::Changeable::Value when,
                                       const std::string& sep)
{
	PropMultiVal* test = new PropMultiVal(_propname, when, sep);
	properties.push_back(test);
	return test;
}

PropMultiValRemain* SectionProp::AddMultiValRemain(const std::string& _propname,
                                                   Property::Changeable::Value when,
                                                   const std::string& sep)
{
	PropMultiValRemain* test = new PropMultiValRemain(_propname, when, sep);
	properties.push_back(test);
	return test;
}

int SectionProp::GetInt(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool SectionProp::GetBool(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return false;
}

double SectionProp::GetDouble(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return 0.0;
}

PropPath* SectionProp::GetPath(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			PropPath* val = dynamic_cast<PropPath*>((*tel));
			if (val) {
				return val;
			} else {
				return nullptr;
			}
		}
	}
	return nullptr;
}

PropMultiVal* SectionProp::GetMultiVal(const std::string& _propname) const
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

PropMultiValRemain* SectionProp::GetMultiValRemain(const std::string& _propname) const
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

Property* SectionProp::GetProperty(int index)
{
	for (it tel = properties.begin(); tel != properties.end(); ++tel) {
		if (!index--) {
			return (*tel);
		}
	}
	return nullptr;
}

Property* SectionProp::GetProperty(const std::string_view propname)
{
	for (Property* property : properties) {
		if (iequals(property->propname, propname)) {
			return property;
		}
	}
	return nullptr;
}

std::string SectionProp::GetString(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if (iequals((*tel)->propname, _propname)) {
			return ((*tel)->GetValue());
		}
	}
	return "";
}

PropBool* SectionProp::GetBoolProp(const std::string& propname) const
{
	for (const auto property : properties) {
		if (iequals(property->propname, propname)) {
			return dynamic_cast<PropBool*>(property);
		}
	}
	return nullptr;
}

PropString* SectionProp::GetStringProp(const std::string& propname) const
{
	for (const auto property : properties) {
		if (iequals(property->propname, propname)) {
			return dynamic_cast<PropString*>(property);
		}
	}
	return nullptr;
}

Hex SectionProp::GetHex(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if (iequals((*tel)->propname, _propname)) {
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool SectionProp::HandleInputline(const std::string& line)
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
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "CONFIG",
			                      "PROGRAM_CONFIG_DEPRECATED_SETTING",
			                      name.c_str());

			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "CONFIG",
			                      create_setting_help_msg_name(name));

			if (!p->IsDeprecatedButAllowed()) {
				return false;
			}
		}

		return p->SetValue(val);
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND",
	                      name.c_str());
	return false;
}

void SectionProp::PrintData(FILE* outfile) const
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

std::string SectionProp::GetPropertyValue(const std::string& _property) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if (!strcasecmp((*tel)->propname.c_str(), _property.c_str())) {
			return (*tel)->GetValue().ToString();
		}
	}
	return NO_SUCH_PROPERTY;
}

bool SectionLine::HandleInputline(const std::string& line)
{
	if (!data.empty()) {
		// Add return to previous line in buffer
		data += "\n";
	}
	data += line;
	return true;
}

void SectionLine::PrintData(FILE* outfile) const
{
	fprintf(outfile, "%s", data.c_str());
}

std::string SectionLine::GetPropertyValue(const std::string&) const
{
	return NO_SUCH_PROPERTY;
}

SectionProp::~SectionProp()
{
	// Delete properties themself (properties stores the pointer of a prop
	for (it prop = properties.begin(); prop != properties.end(); ++prop) {
		delete (*prop);
	}
}

void Section::AddUpdateHandler(SectionUpdateHandler update_handler)
{
	assert(update_handler);
	update_handlers.emplace_back(update_handler);
}

void Section::ExecuteUpdate(const Property& property)
{
	for (const auto& handler : update_handlers) {
		handler(dynamic_cast<SectionProp&>(*this), property.propname);
	}
}

std::optional<bool> parse_bool_setting(const std::string_view setting)
{
	static const std::unordered_map<std::string_view, bool> mapping{
	        { "enabled",  true},
	        {    "true",  true},
	        {      "on",  true},
	        {     "yes",  true},

	        {"disabled", false},
	        {   "false", false},
	        {     "off", false},
	        {      "no", false},
	        {    "none", false},
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
	auto* sect_updater = static_cast<SectionProp*>(
	        control->GetSection(section_name));
	assertm(sect_updater, "Invalid section name");

	auto* property = sect_updater->GetProperty(property_name);
	assertm(property, "Invalid property name");

	property->SetValue(std::string(property_value));
}

// Get up-to-date in-memory model of a config section
SectionProp* get_section(const char* section_name)
{
	assert(control);

	const auto sec = static_cast<SectionProp*>(control->GetSection(section_name));
	assert(sec);

	return sec;
}

SectionProp* get_joystick_section()
{
	return get_section("joystick");
}

SectionProp* get_sdl_section()
{
	return get_section("sdl");
}

SectionProp* get_mixer_section()
{
	return get_section("mixer");
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
