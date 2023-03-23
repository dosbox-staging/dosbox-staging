/*
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
#include <regex>
#include <sstream>
#include <string_view>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "support.h"

#if defined(_MSC_VER) || (defined(__MINGW32__) && defined(__clang__))
_CRTIMP extern char** _environ;
#else
extern char** environ;
#endif

using namespace std;

// Commonly accessed global that holds configuration records
std::unique_ptr<Config> control = {};

// Set by parseconfigfile so Prop_path can use it to construct the realpath
static std::string current_config_dir;

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

Value::operator const char*() const
{
	assert(type == V_STRING);
	return _string.c_str();
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
	istringstream input(in);
	input.flags(ios::hex);
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
	istringstream input(in);
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
	istringstream input(in);
	double result = std::numeric_limits<double>::infinity();
	input >> result;
	if (result == std::numeric_limits<double>::infinity()) {
		return false;
	}
	_double = result;
	return true;
}

bool Value::SetBool(const std::string& in)
{
	istringstream input(in);
	string result;
	input >> result;
	lowcase(result);
	_bool = false;

	if (!result.size()) {
		return false;
	}

	if (result == "0" || result == "disabled" || result == "false" ||
	    result == "off" || result == "no") {
		_bool = false;
	} else if (result == "1" || result == "enabled" || result == "true" ||
	           result == "on" || result == "yes") {
		_bool = true;
	} else {
		return false;
	}

	return true;
}

void Value::SetString(const std::string& in)
{
	_string = in;
}

string Value::ToString() const
{
	ostringstream oss;
	switch (type) {
	case V_HEX:
		oss.flags(ios::hex);
		oss << _hex;
		break;
	case V_INT: oss << _int; break;
	case V_BOOL: oss << boolalpha << _bool; break;
	case V_STRING: oss << _string; break;
	case V_DOUBLE:
		oss.precision(2);
		oss << fixed << _double;
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

	LOG_WARNING("CONFIG: '%s' is an invalid value for '%s', using the default: '%s'",
	            in.ToString().c_str(),
	            propname.c_str(),
	            default_value.ToString().c_str());

	return false;
}

bool Property::ValidateValue(const Value& in)
{
	if (IsValidValue(in)) {
		value = in;
		return true;
	} else {
		value = default_value;
		return false;
	}
}

void Property::Set_help(const std::string& in)
{
	string result = string("CONFIG_") + propname;
	upcase(result);
	MSG_Add(result.c_str(), in.c_str());
}

const char* Property::GetHelp() const
{
	std::string result = "CONFIG_" + propname;
	upcase(result);
	return MSG_Get(result.c_str());
}

const char* Property::GetHelpUtf8() const
{
	std::string result = "CONFIG_" + propname;
	upcase(result);
	return MSG_GetRaw(result.c_str());
}

bool Prop_int::ValidateValue(const Value& in)
{
	if (IsRestrictedValue()) {
		if (IsValidValue(in)) {
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

	LOG_WARNING("CONFIG: %s lies outside the range %s-%s for config '%s', limiting it to %d",
	            in.ToString().c_str(),
	            min_value.ToString().c_str(),
	            max_value.ToString().c_str(),
	            propname.c_str(),
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

	LOG_WARNING("CONFIG: %s lies outside the range %s-%s for config '%s', using the default value %s",
	            in.ToString().c_str(),
	            min_value.ToString().c_str(),
	            max_value.ToString().c_str(),
	            propname.c_str(),
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

	LOG_WARNING("CONFIG: '%s' is an invalid value for '%s', using the default: '%s'",
	            in.ToString().c_str(),
	            propname.c_str(),
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

	std::string workcopy(input);
	Cross::ResolveHomedir(workcopy);

	// Prepend config directory in it exists. Check for absolute paths later
	if (current_config_dir.empty()) {
		realpath = workcopy;
	} else {
		realpath = current_config_dir + CROSS_FILESPLIT + workcopy;
	}

	// Absolute paths
	if (Cross::IsPathAbsolute(workcopy)) {
		realpath = workcopy;
	}

	return is_valid;
}

bool Prop_bool::SetValue(const std::string& input)
{
	auto is_valid = value.SetValue(input, Value::V_BOOL);
	if (!is_valid) {
		SetValue(default_value.ToString());

		LOG_WARNING("CONFIG: '%s' is an invalid value for '%s', using the default: '%s'",
		            input.c_str(),
		            propname.c_str(),
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

	string::size_type loc = string::npos;

	while ((p = section->Get_prop(i++))) {
		// trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != string::npos) {
			local.erase(0, loc);
		}
		loc       = local.find_first_of(separator);
		string in = "";

		// When i == number_of_properties add the total line. (makes
		// more then one string argument possible for parameters of cpu)
		if (loc != string::npos && i < number_of_properties) {
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
	string prevargument   = "";

	string::size_type loc = string::npos;
	while ((p = section->Get_prop(i++))) {
		// Trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != string::npos) {
			local.erase(0, loc);
		}

		loc = local.find_first_of(separator);

		string in = "";

		if (loc != string::npos) {
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
		prevargument = in;
	}

	return is_valid;
}

const std::vector<Value>& Property::GetValues() const
{
	return valid_values;
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

void Property::Set_values(const char* const* in)
{
	Value::Etype type = default_value.type;

	int i = 0;

	while (in[i]) {
		Value val(in[i], type);
		valid_values.push_back(val);
		i++;
	}
}

void Property::Set_values(const std::vector<std::string>& in)
{
	Value::Etype type = default_value.type;
	for (auto& str : in) {
		Value val(str, type);
		valid_values.push_back(val);
	}
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
	for (const_it tel = properties.begin(); tel != properties.end(); tel++) {
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

const char* Section_prop::Get_string(const std::string& _propname) const
{
	for (const_it tel = properties.begin(); tel != properties.end(); ++tel) {
		if ((*tel)->propname == _propname) {
			return ((*tel)->GetValue());
		}
	}
	return "";
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
	string::size_type loc = line.find('=');

	if (loc == string::npos) {
		return false;
	}

	string name = line.substr(0, loc);
	string val  = line.substr(loc + 1);

	// Strip quotes around the value
	trim(val);
	string::size_type length = val.length();
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
			LOG_WARNING("CONFIG: Deprecated option '%s'", name.c_str());
			LOG_WARNING("CONFIG: %s", p->GetHelp());
			return false;
		}

		return p->SetValue(val);
	}

	LOG_WARNING("CONFIG: Unknown option '%s'", name.c_str());
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

string Section_prop::GetPropValue(const std::string& _property) const
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

bool Config::PrintConfig(const std::string& filename) const
{
	char temp[50];
	char helpline[256];

	FILE* outfile = fopen(filename.c_str(), "w+t");
	if (!outfile) {
		return false;
	}

	// Print start of config file and add a return to improve readibility
	fprintf(outfile, MSG_GetRaw("CONFIGFILE_INTRO"), VERSION);
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

				std::string help = p->GetHelpUtf8();

				std::string::size_type pos = std::string::npos;

				while ((pos = help.find('\n', pos + 1)) !=
				       std::string::npos) {
					help.replace(pos, 1, prefix);
				}

				fprintf(outfile,
				        "# %*s: %s",
				        intmaxwidth,
				        p->propname.c_str(),
				        help.c_str());

				std::vector<Value> values = p->GetValues();

				if (!values.empty()) {
					fprintf(outfile,
					        "%s%s:",
					        prefix,
					        MSG_GetRaw("CONFIG_VALID_VALUES"));

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
				}
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

Section_prop* Config::AddEarlySectionProp(const char* name, SectionFunction func,
                                          bool changeable_at_runtime)
{
	Section_prop* s = new Section_prop(name);
	s->AddEarlyInitFunction(func, changeable_at_runtime);
	sectionlist.push_back(s);
	return s;
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
		sec->ExecuteEarlyInit();
	}

	for (const auto& sec : sectionlist) {
		sec->ExecuteInit();
	}
}

void Section::AddEarlyInitFunction(SectionFunction func, bool changeable_at_runtime)
{
	early_init_functions.emplace_back(func, changeable_at_runtime);
}

void Section::AddInitFunction(SectionFunction func, bool changeable_at_runtime)
{
	initfunctions.emplace_back(func, changeable_at_runtime);
}

void Section::AddDestroyFunction(SectionFunction func, bool changeable_at_runtime)
{
	destroyfunctions.emplace_front(func, changeable_at_runtime);
}

void Section::ExecuteEarlyInit(bool init_all)
{
	for (const auto& fn : early_init_functions) {
		if (init_all || fn.changeable_at_runtime) {
			fn.function(this);
		}
	}
}

void Section::ExecuteInit(bool initall)
{
	for (const auto& fn : initfunctions) {
		if (initall || fn.changeable_at_runtime) {
			fn.function(this);
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

Section* Config::GetSection(const std::string& section_name) const
{
	for (auto* el : sectionlist) {
		if (!strcasecmp(el->GetName(), section_name.c_str())) {
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

bool Config::ParseConfigFile(const std::string& type, const std::string& configfilename)
{
	std::error_code ec;
	const std_fs::path cfg_path = configfilename;
	const auto canonical_path   = std_fs::canonical(cfg_path, ec);

	if (ec) {
		return false;
	}

	if (contains(configFilesCanonical, canonical_path)) {
		LOG_INFO("CONFIG: Skipping duplicate config file '%s'",
		         configfilename.c_str());
		return true;
	}

	ifstream in(canonical_path);
	if (!in) {
		return false;
	}

	configfiles.push_back(configfilename);
	configFilesCanonical.push_back(canonical_path);

	// Get directory from configfilename, used with relative paths.
	current_config_dir = canonical_path.parent_path().string();

	string line;
	Section* currentsection = nullptr;

	while (getline(in, line)) {
		// Strip leading/trailing whitespace
		trim(line);
		if (line.empty()) {
			continue;
		}

		switch (line[0]) {
		case '%':
		case '\0':
		case '#':
		case ' ':
		case '\n': continue; break;
		case '[': {
			const auto bracket_pos = line.find(']');
			if (bracket_pos == string::npos) {
				continue;
			}
			line.erase(bracket_pos);
			const auto section_name = line.substr(1);
			if (const auto sec = GetSection(section_name); sec) {
				currentsection = sec;
			}
		} break;
		default:
			if (currentsection) {
				currentsection->HandleInputline(line);

				// If this is an autoexec section, the above
				// takes care of the joining while this handles
				// the overwrriten mode. We need to be prepared
				// for either scenario to play out because we
				// won't know the users final preferance until
				// the very last configuration file is
				// processed.
				if (std::string_view(currentsection->GetName()) ==
				    "autoexec") {
					OverwriteAutoexec(configfilename, line);
				}
			}
			break;
		}
	}

	// So internal changes don't use the path information
	current_config_dir.clear();

	LOG_INFO("CONFIG: Loaded '%s' config file '%s'",
	         type.c_str(),
	         configfilename.c_str());

	return true;
}

parse_environ_result_t parse_environ(const char* const* envp) noexcept
{
	assert(envp);

	// Filter envirnment variables in following format:
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
		if (section_delimiter == string::npos) {
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

	LOG_WARNING("SETUP: Unknown verbosity mode '%s', defaulting to 'high'",
	            user_choice.c_str());
	return Verbosity::High;
}

bool CommandLine::FindExist(const char* name, bool remove)
{
	cmd_it it;
	if (!(FindEntry(name, it, false))) {
		return false;
	}
	if (remove) {
		cmds.erase(it);
	}
	return true;
}

// Checks if any of the command line arguments are found in the pre_args *and*
// exist prior to any of the post_args. If none of the command line arguments
// are found in the pre_args then false is returned.
bool CommandLine::ExistsPriorTo(const std::list<std::string_view>& pre_args,
                                const std::list<std::string_view>& post_args) const
{
	auto any_of_iequals = [](const auto& haystack, const auto& needle) {
		return std::any_of(haystack.begin(),
		                   haystack.end(),
		                   [&needle](const auto& hay) {
			                   return iequals(hay, needle);
		                   });
	};

	for (const auto& cli_arg : cmds) {
		// If any of the pre-args are insensitive-equal-to the CLI
		// argument
		if (any_of_iequals(pre_args, cli_arg)) {
			return true;
		}
		if (any_of_iequals(post_args, cli_arg)) {
			return false;
		}
	}
	return false;
}

bool CommandLine::FindInt(const char* name, int& value, bool remove)
{
	cmd_it it, it_next;

	if (!(FindEntry(name, it, true))) {
		return false;
	}

	it_next = it;
	++it_next;
	value = atoi((*it_next).c_str());

	if (remove) {
		cmds.erase(it, ++it_next);
	}
	return true;
}

bool CommandLine::FindString(const char* name, std::string& value, bool remove)
{
	cmd_it it, it_next;

	if (!(FindEntry(name, it, true))) {
		return false;
	}

	it_next = it;
	++it_next;
	value = *it_next;

	if (remove) {
		cmds.erase(it, ++it_next);
	}
	return true;
}

bool CommandLine::FindCommand(unsigned int which, std::string& value)
{
	if (which < 1) {
		return false;
	}
	if (which > cmds.size()) {
		return false;
	}
	cmd_it it = cmds.begin();
	for (; which > 1; which--) {
		it++;
	}
	value = (*it);
	return true;
}

// Was a directory provided on the command line?
bool CommandLine::HasDirectory() const
{
	return std::any_of(cmds.begin(), cmds.end(), is_directory);
}

// Was an executable filename provided on the command line?
bool CommandLine::HasExecutableName() const
{
	for (const auto& arg : cmds) {
		if (is_executable_filename(arg)) {
			return true;
		}
	}
	return false;
}

bool CommandLine::FindEntry(const char* name, cmd_it& it, bool neednext)
{
	for (it = cmds.begin(); it != cmds.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)) {
			cmd_it itnext = it;
			++itnext;
			if (neednext && (itnext == cmds.end())) {
				return false;
			}
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringBegin(const char* const begin, std::string& value,
                                  bool remove)
{
	size_t len = strlen(begin);
	for (cmd_it it = cmds.begin(); it != cmds.end(); ++it) {
		if (strncmp(begin, (*it).c_str(), len) == 0) {
			value = ((*it).c_str() + len);
			if (remove) {
				cmds.erase(it);
			}
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringRemain(const char* name, std::string& value)
{
	cmd_it it;
	value.clear();
	if (!FindEntry(name, it)) {
		return false;
	}
	++it;
	for (; it != cmds.end(); ++it) {
		value += " ";
		value += (*it);
	}
	return true;
}

// Only used for parsing command.com /C
// Allowing /C dir and /Cdir
// Restoring quotes back into the commands so command /C mount d "/tmp/a b"
// works as intended
bool CommandLine::FindStringRemainBegin(const char* name, std::string& value)
{
	cmd_it it;
	value.clear();

	if (!FindEntry(name, it)) {
		size_t len = strlen(name);

		for (it = cmds.begin(); it != cmds.end(); ++it) {
			if (strncasecmp(name, (*it).c_str(), len) == 0) {
				std::string temp = ((*it).c_str() + len);
				// Restore quotes for correct parsing in later
				// stages
				if (temp.find(' ') != std::string::npos) {
					value = std::string("\"") + temp +
					        std::string("\"");
				} else {
					value = temp;
				}
				break;
			}
		}
		if (it == cmds.end()) {
			return false;
		}
	}

	++it;

	for (; it != cmds.end(); ++it) {
		value += " ";
		std::string temp = (*it);
		if (temp.find(' ') != std::string::npos) {
			value += std::string("\"") + temp + std::string("\"");
		} else {
			value += temp;
		}
	}
	return true;
}

bool CommandLine::GetStringRemain(std::string& value)
{
	if (!cmds.size()) {
		return false;
	}

	cmd_it it = cmds.begin();
	value     = (*it++);

	for (; it != cmds.end(); ++it) {
		value += " ";
		value += (*it);
	}
	return true;
}

unsigned int CommandLine::GetCount(void)
{
	return (unsigned int)cmds.size();
}

void CommandLine::FillVector(std::vector<std::string>& vector)
{
	for (cmd_it it = cmds.begin(); it != cmds.end(); ++it) {
		vector.push_back((*it));
	}
#ifdef WIN32
	// Add back the \" if the parameter contained a space
	for (Bitu i = 0; i < vector.size(); i++) {
		if (vector[i].find(' ') != std::string::npos) {
			vector[i] = "\"" + vector[i] + "\"";
		}
	}
#endif
}

int CommandLine::GetParameterFromList(const char* const params[],
                                      std::vector<std::string>& output)
{
	// Return values: 0 = P_NOMATCH, 1 = P_NOPARAMS
	// TODO return nomoreparams
	int retval = 1;
	output.clear();

	enum { P_START, P_FIRSTNOMATCH, P_FIRSTMATCH } parsestate = P_START;

	cmd_it it = cmds.begin();

	while (it != cmds.end()) {
		bool found = false;

		for (int i = 0; *params[i] != 0; ++i) {
			if (!strcasecmp((*it).c_str(), params[i])) {
				// Found a parameter
				found = true;
				switch (parsestate) {
				case P_START:
					retval     = i + 2;
					parsestate = P_FIRSTMATCH;
					break;
				case P_FIRSTMATCH:
				case P_FIRSTNOMATCH: return retval;
				}
			}
		}

		if (!found) {
			switch (parsestate) {
			case P_START:
				// No match
				retval     = 0;
				parsestate = P_FIRSTNOMATCH;
				output.push_back(*it);
				break;
			case P_FIRSTMATCH:
			case P_FIRSTNOMATCH: output.push_back(*it); break;
			}
		}

		cmd_it itold = it;
		++it;

		cmds.erase(itold);
	}

	return retval;
}

CommandLine::CommandLine(int argc, const char* const argv[])
{
	if (argc > 0) {
		file_name = argv[0];
	}

	int i = 1;

	while (i < argc) {
		cmds.push_back(argv[i]);
		i++;
	}
}

uint16_t CommandLine::Get_arglength()
{
	if (cmds.empty()) {
		return 0;
	}

	size_t total_length = 0;
	for (const auto& cmd : cmds) {
		total_length += cmd.size() + 1;
	}

	if (total_length > UINT16_MAX) {
		LOG_WARNING("SETUP: Command line length too long, truncating");
		total_length = UINT16_MAX;
	}

	return static_cast<uint16_t>(total_length);
}

CommandLine::CommandLine(const char* name, const char* cmdline)
{
	if (name) {
		file_name = name;
	}

	// Parse the commands and put them in the list
	bool inword, inquote;
	char c;
	inword  = false;
	inquote = false;
	std::string str;

	const char* c_cmdline = cmdline;

	while ((c = *c_cmdline) != 0) {
		if (inquote) {
			if (c != '"') {
				str += c;
			} else {
				inquote = false;
				cmds.push_back(str);
				str.erase();
			}
		} else if (inword) {
			if (c != ' ') {
				str += c;
			} else {
				inword = false;
				cmds.push_back(str);
				str.erase();
			}
		} else if (c == '\"') {
			inquote = true;
		} else if (c != ' ') {
			str += c;
			inword = true;
		}
		c_cmdline++;
	}

	if (inword || inquote) {
		cmds.push_back(str);
	}
}

void CommandLine::Shift(unsigned int amount)
{
	while (amount--) {
		file_name = cmds.size() ? (*(cmds.begin())) : "";
		if (cmds.size()) {
			cmds.erase(cmds.begin());
		}
	}
}

const std::string& SETUP_GetLanguage()
{
	static bool lang_is_cached = false;
	static std::string lang    = {};

	if (lang_is_cached) {
		return lang;
	}

	// Has the user provided a language on the command line?
	(void)control->cmdline->FindString("-lang", lang, true);

	// Is a language provided in the conf file?
	if (lang.empty()) {
		const auto section = control->GetSection("dosbox");
		assert(section);
		lang = static_cast<const Section_prop*>(section)->Get_string("language");
	}

	// Check the LANG environment variable
	if (lang.empty()) {
		const char* envlang = getenv("LANG");
		if (envlang) {
			lang = envlang;
			clear_language_if_default(lang);
		}
	}

	// Query the OS using OS-specific calls
	if (lang.empty()) {
		lang = get_language_from_os();
		clear_language_if_default(lang);
	}

	// Drop the dialect part of the language
	// (e.g. "en_GB.UTF8" -> "en")
	if (lang.size() > 2) {
		lang = lang.substr(0, 2);
	}

	// Return it as lowercase
	lowcase(lang);

	lang_is_cached = true;
	return lang;
}

// Parse the user's configuration files starting with the primary, then custom
// -conf's, and finally the local dosbox.conf
void MSG_Init(Section_prop*);
void SETUP_ParseConfigFiles(const std::string& config_path)
{
	std::string config_file;

	// First: parse the user's primary config file
	const bool wants_primary_conf = !control->cmdline->FindExist("-noprimaryconf",
	                                                             true);
	if (wants_primary_conf) {
		Cross::GetPlatformConfigName(config_file);
		const std::string config_combined = config_path + config_file;
		control->ParseConfigFile("primary", config_combined);
	}

	// Second: parse the local 'dosbox.conf', if present
	const bool wants_local_conf = !control->cmdline->FindExist("-nolocalconf",
	                                                           true);
	if (wants_local_conf) {
		control->ParseConfigFile("local", "dosbox.conf");
	}

	// Finally: layer on custom -conf <files>
	while (control->cmdline->FindString("-conf", config_file, true)) {
		if (!control->ParseConfigFile("custom", config_file)) {
			// Try to load it from the user directory
			if (!control->ParseConfigFile("custom",
			                              config_path + config_file)) {
				LOG_WARNING("CONFIG: Can't open custom config file '%s'",
				            config_file.c_str());
			}
		}
	}

	// Once we've parsed all the potential conf files, we've down our best
	// to discover the user's desired language. At this point, we can now
	// initialize the messaging system which honors the language and loads
	// those messages.
	if (const auto sec = control->GetSection("dosbox"); sec) {
		MSG_Init(static_cast<Section_prop*>(sec));
	}

	// Create a new primary if permitted and no other conf was loaded
	if (wants_primary_conf && !control->configfiles.size()) {
		std::string new_config_path = config_path;

		Cross::CreatePlatformConfigDir(new_config_path);
		Cross::GetPlatformConfigName(config_file);

		const std::string config_combined = new_config_path + config_file;

		if (control->PrintConfig(config_combined)) {
			LOG_MSG("CONFIG: Wrote new primary config file '%s'",
			        config_combined.c_str());
			control->ParseConfigFile("new primary", config_combined);
		} else {
			LOG_WARNING("CONFIG: Unable to write a new primary config file '%s'",
			            config_combined.c_str());
		}
	}
}

static char return_msg[200];
const char* SetProp(std::vector<std::string>& pvars)
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
		Section* sec = control->GetSectionFromProperty(pvars[0].c_str());

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
		Section* sec = control->GetSection(pvars[0].c_str());

		if (!sec) {
			// Not a section: little duplicate from above
			Section* sec = control->GetSectionFromProperty(
			        pvars[0].c_str());

			if (sec) {
				pvars.insert(pvars.begin(),
				             std::string(sec->GetName()));
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
			Section* sec2 = control->GetSectionFromProperty(
			        pvars[1].c_str());

			if (!sec2) {
				// Not a property
				Section* sec3 = control->GetSectionFromProperty(
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
	Section* sec2 = control->GetSectionFromProperty(pvars[1].c_str());
	if (!sec2) {
		safe_sprintf(return_msg,
		             MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
		             pvars[1].c_str(),
		             pvars[0].c_str());
		return return_msg;
	}

	return return_msg;
}
