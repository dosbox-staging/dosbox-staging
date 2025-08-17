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

#if defined(_MSC_VER) || (defined(__MINGW32__) && defined(__clang__))
_CRTIMP extern char** _environ;
#else
extern char** environ;
#endif

// Commonly accessed global that holds configuration records
ConfigPtr control = {};

// Set by parseconfigfile so PropPath can use it to construct the realpath
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
		prevargument = curr_value;
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

bool Config::WriteConfig(const std_fs::path& path) const
{
	char temp[50];
	char helpline[256];

	FILE* outfile = fopen(path.string().c_str(), "w+t");
	if (!outfile) {
		return false;
	}

	// Print start of config file and add a return to improve readibility
	fprintf(outfile, MSG_GetTranslatedRaw("CONFIGFILE_INTRO").c_str(), DOSBOX_VERSION);
	fprintf(outfile, "\n");

	for (auto tel = sectionlist.cbegin(); tel != sectionlist.cend(); ++tel) {
		// Print section header
		safe_strcpy(temp, (*tel)->GetName());
		lowcase(temp);
		fprintf(outfile, "[%s]\n", temp);

		SectionProp* sec = dynamic_cast<SectionProp*>(*tel);
		if (sec) {
			Property* p;
			int i = 0;

			size_t maxwidth = 0;

			while ((p = sec->GetProperty(i++))) {
				maxwidth = std::max(maxwidth, p->propname.length());
			}

			i = 0;

			char prefix[80];
			int intmaxwidth = std::min<int>(60, check_cast<int>(maxwidth));
			safe_sprintf(prefix, "\n# %*s  ", intmaxwidth, "");

			while ((p = sec->GetProperty(i++))) {
				if (p->IsDeprecated()) {
					continue;
				}

				auto help = p->GetHelpRaw();

				std::string::size_type pos = std::string::npos;

				while ((pos = help.find('\n', pos + 1)) !=
				       std::string::npos) {
					help.replace(pos, 1, prefix);
				}

				// Percentage signs are encoded as '%%' in the
				// config descriptions because they are sent
				// through printf-like functions (e.g.,
				// WriteOut()). So we need to de-escape them
				// before writing them into the config.
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
					        MSG_GetTranslatedRaw(values_msg_key)
					                .c_str());

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

				print_values("PROGRAM_CONFIG_VALID_VALUES",
				             p->GetValues());

				print_values("PROGRAM_CONFIG_DEPRECATED_VALUES",
				             p->GetDeprecatedValues());

				fprintf(outfile, "\n");
				fprintf(outfile, "#\n");
			}
		} else {
			upcase(temp);
			strcat(temp, "_CONFIGFILE_HELP");

			const auto _helpstr = MSG_GetTranslatedRaw(temp);

			const char* helpstr   = _helpstr.c_str();
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

SectionProp* Config::AddSection(const char* section_name, SectionFunction func,
                                bool changeable_at_runtime)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");

	SectionProp* s = new SectionProp(section_name);
	s->AddInitFunction(func, changeable_at_runtime);
	sectionlist.push_back(s);
	return s;
}

SectionProp::~SectionProp()
{
	// ExecuteDestroy should be here else the destroy functions use
	// destroyed properties
	ExecuteDestroy(true);

	// Delete properties themself (properties stores the pointer of a prop
	for (it prop = properties.begin(); prop != properties.end(); ++prop) {
		delete (*prop);
	}
}

SectionLine* Config::AddSectionLine(const char* section_name, SectionFunction func)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");

	SectionLine* blah = new SectionLine(section_name);
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
	cmdline         = std::move(source.cmdline);
	sectionlist     = std::move(source.sectionlist);
	_start_function = std::move(source._start_function);
	secure_mode     = std::move(source.secure_mode);
	startup_params  = std::move(source.startup_params);
	configfiles     = std::move(source.configfiles);

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
		if (el->GetPropertyValue(prop) != NO_SUCH_PROPERTY) {
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

const SectionLine& Config::GetOverwrittenAutoexecSection() const
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

	std::ifstream input_stream(canonical_path);
	if (!input_stream) {
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

	while (getline(input_stream, line)) {
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

// Applies queued configuration settings to CLI arguments. It replaces any
// existing settings with their latest values. For example, if "machine=value"
// was set multiple times, only the most recent value is preserved in the final
// CLI args.
void Config::ApplyQueuedValuesToCli(std::vector<std::string>& args) const
{
	for (const auto section : sectionlist) {
		const auto properties = dynamic_cast<SectionProp*>(section);
		if (!properties) {
			continue;
		}

		for (const auto property : *properties) {
			const auto queued_value = property->GetQueuedValue();
			if (!queued_value) {
				continue;
			}

			constexpr auto set_prefix = "--set";

			const auto key_prefix = format_str("%s=",
			                                   property->propname.c_str());

			// Remove existing matches
			auto it = args.begin();
			while (it != args.end() && std::next(it) != args.end()) {
				const auto current = it;
				const auto next    = std::next(it);

				if (*current == set_prefix &&
				    next->starts_with(key_prefix)) {
					it = args.erase(current, std::next(next));
				} else {
					++it;
				}
			}

			// Add the new arguments with the queued value
			args.emplace_back(set_prefix);
			args.emplace_back(key_prefix + *queued_value);
		}
	}
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

		const std::string rest = (env_var + 7);

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

StartupVerbosity Config::GetStartupVerbosity() const
{
	const Section* s = GetSection("dosbox");
	assert(s);
	const std::string user_choice = s->GetPropertyValue("startup_verbosity");

	if (user_choice == "high") {
		return StartupVerbosity::High;
	}
	if (user_choice == "low") {
		return StartupVerbosity::Low;
	}
	if (user_choice == "quiet") {
		return StartupVerbosity::Quiet;
	}
	if (user_choice == "auto") {
		return (cmdline->HasDirectory() || cmdline->HasExecutableName())
		             ? StartupVerbosity::Low
		             : StartupVerbosity::High;
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "Invalid [color=light-green]'startup_verbosity'[reset] setting: "
	                      "[color=white]'%s'[reset], using [color=white]'high'[reset]",
	                      user_choice.c_str());

	return StartupVerbosity::High;
}

const std::string& Config::GetArgumentLanguage()
{
	return arguments.lang;
}

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
	MSG_LoadMessages();
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
		pvars.emplace(pvars.begin() + 1, pvars[0].substr(equpos + 1));
		pvars[0].erase(equpos);

		// As we had a = the first thing must be a property now
		Section* sec = GetSectionFromProperty(pvars[0].c_str());

		if (sec) {
			pvars.emplace(pvars.begin(), std::string(sec->GetName()));
		} else {
			safe_sprintf(return_msg,
			             MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND")
			                     .c_str(),
			             pvars[0].c_str());
			return return_msg;
		}
		// Order in the vector should be ok now
	} else {
		if ((spcpos != std::string::npos) &&
		    ((equpos == std::string::npos) || (spcpos < equpos))) {
			// ' ' before a possible '=', split on the ' '
			pvars.emplace(pvars.begin() + 1,
			              pvars[0].substr(spcpos + 1));
			pvars[0].erase(spcpos);
		}

		// Check if the first parameter is a section or property
		Section* sec = GetSection(pvars[0]);

		if (!sec) {
			// Not a section: little duplicate from above
			Section* secprop = GetSectionFromProperty(pvars[0].c_str());

			if (secprop) {
				pvars.emplace(pvars.begin(),
				              std::string(secprop->GetName()));
			} else {
				safe_sprintf(return_msg,
				             MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND")
				                     .c_str(),
				             pvars[0].c_str());
				return return_msg;
			}
		} else {
			// First of pvars is most likely a section, but could
			// still be gus have a look at the second parameter
			if (pvars.size() < 2) {
				safe_strcpy(return_msg,
				            MSG_Get("PROGRAM_CONFIG_SET_SYNTAX").c_str());
				return return_msg;
			}

			std::string::size_type spcpos2 = pvars[1].find_first_of(' ');
			std::string::size_type equpos2 = pvars[1].find_first_of('=');

			if ((equpos2 != std::string::npos) &&
			    ((spcpos2 == std::string::npos) || (equpos2 < spcpos2))) {
				// Split on the =
				pvars.emplace(pvars.begin() + 2,
				              pvars[1].substr(equpos2 + 1));

				pvars[1].erase(equpos2);

			} else if ((spcpos2 != std::string::npos) &&
			           ((equpos2 == std::string::npos) ||
			            (spcpos2 < equpos2))) {
				// Split on the ' '
				pvars.emplace(pvars.begin() + 2,
				              pvars[1].substr(spcpos2 + 1));

				pvars[1].erase(spcpos2);
			}

			// Is this a property?
			Section* sec2 = GetSectionFromProperty(pvars[1].c_str());

			if (!sec2) {
				// Not a property
				Section* sec3 = GetSectionFromProperty(
				        pvars[0].c_str());
				if (sec3) {
					// Section and property name are identical
					pvars.emplace(pvars.begin(), pvars[0]);
				}
				// else has been checked above already
			}
		}
	}

	if (pvars.size() < 3) {
		safe_strcpy(return_msg,
		            MSG_Get("PROGRAM_CONFIG_SET_SYNTAX").c_str());
		return return_msg;
	}

	// Check if the property actually exists in the section
	Section* sec2 = GetSectionFromProperty(pvars[1].c_str());
	if (!sec2) {
		safe_sprintf(return_msg,
		             MSG_Get("PROGRAM_CONFIG_NO_PROPERTY").c_str(),
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
	arguments.list_code_pages = cmdline->FindRemoveBoolArgument("list-code-pages");
	arguments.list_glshaders = cmdline->FindRemoveBoolArgument("list-glshaders");
	arguments.noconsole   = cmdline->FindRemoveBoolArgument("noconsole");
	arguments.startmapper = cmdline->FindRemoveBoolArgument("startmapper");
	arguments.exit        = cmdline->FindRemoveBoolArgument("exit");
	arguments.securemode  = cmdline->FindRemoveBoolArgument("securemode");
	arguments.noautoexec  = cmdline->FindRemoveBoolArgument("noautoexec");

	arguments.eraseconf = cmdline->FindRemoveBoolArgument("eraseconf") ||
	                      cmdline->FindRemoveBoolArgument("resetconf");
	arguments.erasemapper = cmdline->FindRemoveBoolArgument("erasemapper") ||
	                        cmdline->FindRemoveBoolArgument("resetmapper");

	arguments.version = cmdline->FindRemoveBoolArgument("version", 'v');
	arguments.help    = (cmdline->FindRemoveBoolArgument("help", 'h') ||
                          cmdline->FindRemoveBoolArgument("help", '?'));

	arguments.working_dir = cmdline->FindRemoveStringArgument("working-dir");
	arguments.lang    = cmdline->FindRemoveStringArgument("lang");
	arguments.machine = cmdline->FindRemoveStringArgument("machine");

	arguments.socket   = cmdline->FindRemoveIntArgument("socket");
	arguments.wait_pid = cmdline->FindRemoveIntArgument("waitpid");

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
