/*
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

#include "setup.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>

#include "control.h"
#include "string_utils.h"

#ifdef _MSC_VER
_CRTIMP extern char **_environ;
#else
extern char **environ;
#endif

using namespace std;
static std::string current_config_dir; // Set by parseconfigfile so Prop_path can use it to construct the realpath
void Value::destroy() throw(){
	if (type == V_STRING) delete _string;
}

Value& Value::copy(Value const& in) {
	if (this != &in) { //Selfassigment!
		if(type != V_NONE && type != in.type) throw WrongType();
		destroy();
		plaincopy(in);
	}
	return *this;
}

void Value::plaincopy(Value const& in) throw(){
	type = in.type;
	_int = in._int;
	_double = in._double;
	_bool = in._bool;
	_hex = in._hex;
	if(type == V_STRING) _string = new string(*in._string);
}

Value::operator bool () const {
	if(type != V_BOOL) throw WrongType();
	return _bool;
}

Value::operator Hex () const {
	if(type != V_HEX) throw WrongType();
	return _hex;
}

Value::operator int () const {
	if(type != V_INT) throw WrongType();
	return _int;
}

Value::operator double () const {
	if(type != V_DOUBLE) throw WrongType();
	return _double;
}

Value::operator char const* () const {
	if(type != V_STRING) throw WrongType();
	return _string->c_str();
}

bool Value::operator==(Value const& other) const {
	if(this == &other) return true;
	if(type != other.type) return false;
	switch(type){
		case V_BOOL: 
			if(_bool == other._bool) return true;
			break;
		case V_INT:
			if(_int == other._int) return true;
			break;
		case V_HEX:
			if(_hex == other._hex) return true;
			break;
		case V_DOUBLE:
			if(_double == other._double) return true;
			break;
		case V_STRING:
			if((*_string) == (*other._string)) return true;
			break;
		default:
			E_Exit("comparing stuff that doesn't make sense");
			break;
	}
	return false;
}
bool Value::SetValue(string const& in,Etype _type) {
	/* Throw exception if the current type isn't the wanted type 
	 * Unless the wanted type is current.
	 */
	if(_type == V_CURRENT && type == V_NONE) throw WrongType();
	if(_type != V_CURRENT) { 
		if(type != V_NONE && type != _type) throw WrongType();
		type = _type;
	}
	bool retval = true;
	switch(type){
		case V_HEX:
			retval = set_hex(in);
			break;
		case V_INT:
			retval = set_int(in);
			break;
		case V_BOOL:
			retval = set_bool(in);
			break;
		case V_STRING:
			set_string(in);
			break;
		case V_DOUBLE:
			retval = set_double(in);
			break;

		case V_NONE:
		case V_CURRENT:
		default:
			/* Shouldn't happen!/Unhandled */
			throw WrongType();
			break;
	}
	return retval;
}

bool Value::set_hex(std::string const& in) {
	istringstream input(in);
	input.flags(ios::hex);
	Bits result = INT_MIN;
	input >> result;
	if(result == INT_MIN) return false;
	_hex = result;
	return true;
}

bool Value::set_int(string const &in) {
	istringstream input(in);
	Bits result = INT_MIN;
	input >> result;
	if(result == INT_MIN) return false;
	_int = result;
	return true;
}
bool Value::set_double(string const &in) {
	istringstream input(in);
	double result = std::numeric_limits<double>::infinity();
	input >> result;
	if(result == std::numeric_limits<double>::infinity()) return false;
	_double = result;
	return true;
}

bool Value::set_bool(string const &in) {
	istringstream input(in);
	string result;
	input >> result;
	lowcase(result);
	_bool = true; // TODO
	if(!result.size()) return false;

	if(result=="0" || result=="disabled" || result=="false" || result=="off") {
		_bool = false;
	} else if(result=="1" || result=="enabled" || result=="true" || result=="on") {
		_bool = true;
	} else return false;

	return true;
}

void Value::set_string(string const & in) {
	if(!_string) _string = new string();
	_string->assign(in);
}

string Value::ToString() const {
	ostringstream oss;
	switch(type) {
		case V_HEX:
			oss.flags(ios::hex);
			oss << _hex;
			break;
		case V_INT:
			oss << _int;
			break;
		case V_BOOL:
			oss << boolalpha << _bool;
			break;
		case V_STRING:
			oss << *_string;
			break;
		case V_DOUBLE:
			oss.precision(2);
			oss << fixed << _double;
			break;
		case V_NONE:
		case V_CURRENT:
		default:
			E_Exit("ToString messed up ?");
			break;
	}
	return oss.str();
}

Property::Property(const std::string &name, Changeable::Value when)
        : propname(name),
          value(),
          suggested_values{},
          default_value(),
          change(when)
{
	assertm(std::regex_match(name, std::regex{"[a-zA-Z0-9_]+"}),
	        "Only letters, digits, and underscores are allowed in property name");
}

bool Property::CheckValue(Value const& in, bool warn){
	if (suggested_values.empty()) return true;
	for(const_iter it = suggested_values.begin();it != suggested_values.end();++it) {
		if ( (*it) == in) { //Match!
			return true;
		}
	}
	if (warn) LOG_MSG("\"%s\" is not a valid value for variable: %s.\nIt might now be reset to the default value: %s",in.ToString().c_str(),propname.c_str(),default_value.ToString().c_str());
	return false;
}

void Property::Set_help(string const& in) {
	string result = string("CONFIG_") + propname;
	upcase(result);
	MSG_Add(result.c_str(),in.c_str());
}

const char * Property::GetHelp() const
{
	std::string result = "CONFIG_" + propname;
	upcase(result);
	return MSG_Get(result.c_str());
}

bool Prop_int::SetVal(Value const& in, bool forced, bool warn) {
	if (forced) {
		value = in;
		return true;
	} else if (!suggested_values.empty()){
		if ( CheckValue(in,warn) ) {
			value = in;
			return true;
		} else {
			value = default_value;
			return false;
		}
	} else {
		// Handle ranges if specified
		const int mi = min_value;
		const int ma = max_value;
		int va = static_cast<int>(Value(in));

		//No ranges
		if (mi == -1 && ma == -1) { value = in; return true;}

		//Inside range
		if (va >= mi && va <= ma) { value = in; return true;}

		//Outside range, set it to the closest boundary
		if (va > ma ) va = ma; else va = mi;

		if (warn) {
			LOG_MSG("%s is outside the allowed range %s-%s for variable: %s.\n"
			        "It has been set to the closest boundary: %d.",
			        in.ToString().c_str(),
			        min_value.ToString().c_str(),
			        max_value.ToString().c_str(),
			        propname.c_str(),
			        va);
		}

		value = va; 
		return true;
	}
}
bool Prop_int::CheckValue(Value const& in, bool warn) {
//	if(!suggested_values.empty() && Property::CheckValue(in,warn)) return true;
	if(!suggested_values.empty()) return Property::CheckValue(in,warn);
	// LOG_MSG("still used ?");
	//No >= and <= in Value type and == is ambigious
	const int mi = min_value;
	const int ma = max_value;
	int va = static_cast<int>(Value(in));
	if (mi == -1 && ma == -1) return true;
	if (va >= mi && va <= ma) return true;

	if (warn) {
		LOG_MSG("%s lies outside the range %s-%s for variable: %s.\n"
		        "It might now be reset to the default value: %s",
		        in.ToString().c_str(),
		        min_value.ToString().c_str(),
		        max_value.ToString().c_str(),
		        propname.c_str(),
		        default_value.ToString().c_str());
	}
	return false;
}

bool Prop_double::SetValue(std::string const& input) {
	Value val;
	if(!val.SetValue(input,Value::V_DOUBLE)) return false;
	return SetVal(val,false,true);
}

//void Property::SetValue(char* input){
//	value.SetValue(input, Value::V_CURRENT);
//}
bool Prop_int::SetValue(std::string const& input) {
	Value val;
	if (!val.SetValue(input,Value::V_INT)) return false;
	bool retval = SetVal(val,false,true);
	return retval;
}

bool Prop_string::SetValue(std::string const& input) {
	//Special version for lowcase stuff
	std::string temp(input);
	//suggested values always case insensitive.
	//If there are none then it can be paths and such which are case sensitive
	if (!suggested_values.empty()) lowcase(temp);
	Value val(temp,Value::V_STRING);
	return SetVal(val,false,true);
}
bool Prop_string::CheckValue(Value const& in, bool warn) {
	if (suggested_values.empty()) return true;
	for(const_iter it = suggested_values.begin();it != suggested_values.end();++it) {
		if ( (*it) == in) { //Match!
			return true;
		}
		if ((*it).ToString() == "%u") {
			unsigned int value;
			if(sscanf(in.ToString().c_str(),"%u",&value) == 1) {
				return true;
			}
		}
	}
	if (warn) LOG_MSG("\"%s\" is not a valid value for variable: %s.\nIt might now be reset to the default value: %s",in.ToString().c_str(),propname.c_str(),default_value.ToString().c_str());
	return false;
}

bool Prop_path::SetValue(std::string const& input) {
	//Special version to merge realpath with it

	Value val(input,Value::V_STRING);
	bool retval = SetVal(val,false,true);

	if (input.empty()) {
		realpath.clear();
		return false;
	}
	std::string workcopy(input);
	Cross::ResolveHomedir(workcopy); //Parse ~ and friends
	//Prepend config directory in it exists. Check for absolute paths later
	if ( current_config_dir.empty()) realpath = workcopy;
	else realpath = current_config_dir + CROSS_FILESPLIT + workcopy;
	//Absolute paths
	if (Cross::IsPathAbsolute(workcopy)) realpath = workcopy;
	return retval;
}

bool Prop_bool::SetValue(std::string const& input) {
	return value.SetValue(input,Value::V_BOOL);
}

bool Prop_hex::SetValue(std::string const& input) {
	Value val;
	val.SetValue(input,Value::V_HEX);
	return SetVal(val,false,true);
}

void Prop_multival::make_default_value() {
	Bitu i = 1;
	Property *p = section->Get_prop(0);
	if (!p) return;

	std::string result = p->Get_Default_Value().ToString();
	while( (p = section->Get_prop(i++)) ) {
		std::string props = p->Get_Default_Value().ToString();
		if (props.empty()) continue;
		result += separator; result += props;
	}
	Value val(result,Value::V_STRING);
	SetVal(val,false,true);
}

//TODO checkvalue stuff
bool Prop_multival_remain::SetValue(std::string const& input) {
	Value val(input,Value::V_STRING);
	bool retval = SetVal(val,false,true);

	std::string local(input);
	int i = 0,number_of_properties = 0;
	Property *p = section->Get_prop(0);
	//No properties in this section. do nothing
	if (!p) return false;

	while( (section->Get_prop(number_of_properties)) )
		number_of_properties++;

	string::size_type loc = string::npos;
	while( (p = section->Get_prop(i++)) ) {
		//trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != string::npos) local.erase(0,loc);
		loc = local.find_first_of(separator);
		string in = "";//default value
		/* when i == number_of_properties add the total line. (makes more then
		 * one string argument possible for parameters of cpu) */
		if (loc != string::npos && i < number_of_properties) { //separator found
			in = local.substr(0,loc);
			local.erase(0,loc+1);
		} else if (local.size()) { //last argument or last property
			in = local;
			local.clear();
		}
		//Test Value. If it fails set default
		Value valtest (in,p->Get_type());
		if (!p->CheckValue(valtest,true)) {
			make_default_value();
			return false;
		}
		p->SetValue(in);
	}
	return retval;
}

//TODO checkvalue stuff
bool Prop_multival::SetValue(std::string const& input) {
	Value val(input,Value::V_STRING);
	bool retval = SetVal(val,false,true);

	std::string local(input);
	int i = 0;
	Property *p = section->Get_prop(0);
	//No properties in this section. do nothing
	if (!p) return false;
	Value::Etype prevtype = Value::V_NONE;
	string prevargument = "";

	string::size_type loc = string::npos;
	while( (p = section->Get_prop(i++)) ) {
		//trim leading separators
		loc = local.find_first_not_of(separator);
		if (loc != string::npos) local.erase(0,loc);
		loc = local.find_first_of(separator);
		string in = "";//default value
		if (loc != string::npos) { //separator found
			in = local.substr(0,loc);
			local.erase(0,loc+1);
		} else if (local.size()) { //last argument
			in = local;
			local.clear();
		}
		
		if (p->Get_type() == Value::V_STRING) {
			//Strings are only checked against the suggested values list.
			//Test Value. If it fails set default
			Value valtest (in,p->Get_type());
			if (!p->CheckValue(valtest,true)) {
				make_default_value();
				return false;
			}
			p->SetValue(in);
		} else {
			//Non-strings can have more things, conversion alone is not enough (as invalid values as converted to 0)
			bool r = p->SetValue(in);
			if (!r) {
				if (in.empty() && p->Get_type() == prevtype ) {
					//Nothing there, but same type of variable, so repeat it (sensitivity)
					in = prevargument; 
					p->SetValue(in);
				} else {
					//Something was there to be parsed or not the same type. Invalidate entire property.
					make_default_value();
				}
			}
		}
		prevtype = p->Get_type();
		prevargument = in;

	}
	return retval;
}

const std::vector<Value>& Property::GetValues() const {
	return suggested_values;
}
const std::vector<Value>& Prop_multival::GetValues() const {
	Property *p = section->Get_prop(0);
	//No properties in this section. do nothing
	if (!p) return suggested_values;
	int i =0;
	while( (p = section->Get_prop(i++)) ) {
		std::vector<Value> v = p->GetValues();
		if(!v.empty()) return p->GetValues();
	}
	return suggested_values;
}

/*
void Section_prop::Add_double(char const * const _propname, double _value) {
	Property* test=new Prop_double(_propname,_value);
	properties.push_back(test);
}*/

void Property::Set_values(const char * const *in) {
	Value::Etype type = default_value.type;
	int i = 0;
	while (in[i]) {
		Value val(in[i],type);
		suggested_values.push_back(val);
		i++;
	}
}

void Property::Set_values(const std::vector<std::string> & in) {
	Value::Etype type = default_value.type;
	for (auto &str : in) {
		Value val(str, type);
		suggested_values.push_back(val);
	}
}

Prop_int* Section_prop::Add_int(string const& _propname, Property::Changeable::Value when, int _value) {
	Prop_int* test=new Prop_int(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_string* Section_prop::Add_string(string const& _propname, Property::Changeable::Value when, char const * const _value) {
	Prop_string* test=new Prop_string(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_path* Section_prop::Add_path(string const& _propname, Property::Changeable::Value when, char const * const _value) {
	Prop_path* test=new Prop_path(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_bool* Section_prop::Add_bool(string const& _propname, Property::Changeable::Value when, bool _value) {
	Prop_bool* test=new Prop_bool(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_hex* Section_prop::Add_hex(string const& _propname, Property::Changeable::Value when, Hex _value) {
	Prop_hex* test=new Prop_hex(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_multival* Section_prop::Add_multi(std::string const& _propname, Property::Changeable::Value when,std::string const& sep) {
	Prop_multival* test = new Prop_multival(_propname,when,sep);
	properties.push_back(test);
	return test;
}

Prop_multival_remain* Section_prop::Add_multiremain(std::string const& _propname, Property::Changeable::Value when,std::string const& sep) {
	Prop_multival_remain* test = new Prop_multival_remain(_propname,when,sep);
	properties.push_back(test);
	return test;
}

int Section_prop::Get_int(string const&_propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if ((*tel)->propname==_propname){
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool Section_prop::Get_bool(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			return ((*tel)->GetValue());
		}
	}
	return false;
}

double Section_prop::Get_double(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			return ((*tel)->GetValue());
		}
	}
	return 0.0;
}

Prop_path* Section_prop::Get_path(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			Prop_path* val = dynamic_cast<Prop_path*>((*tel));
			if (val) return val; else return NULL;
		}
	}
	return NULL;
}

Prop_multival* Section_prop::Get_multival(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			Prop_multival* val = dynamic_cast<Prop_multival*>((*tel));
			if(val) return val; else return NULL;
		}
	}
	return NULL;
}

Prop_multival_remain* Section_prop::Get_multivalremain(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			Prop_multival_remain* val = dynamic_cast<Prop_multival_remain*>((*tel));
			if (val) return val; else return NULL;
		}
	}
	return NULL;
}
Property* Section_prop::Get_prop(int index){
	for(it tel = properties.begin();tel != properties.end();++tel){
		if (!index--) return (*tel);
	}
	return NULL;
}

const char* Section_prop::Get_string(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			return ((*tel)->GetValue());
		}
	}
	return "";
}
Hex Section_prop::Get_hex(string const& _propname) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if ((*tel)->propname == _propname){
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool Section_prop::HandleInputline(string const& gegevens){
	string str1 = gegevens;
	string::size_type loc = str1.find('=');
	if (loc == string::npos) return false;
	string name = str1.substr(0,loc);
	string val = str1.substr(loc + 1);

	/* Remove quotes around value */
	trim(val);
	string::size_type length = val.length();
	if (length > 1 &&
	     ((val[0] == '\"'  && val[length - 1] == '\"' ) ||
	      (val[0] == '\'' && val[length - 1] == '\''))
	   ) val = val.substr(1,length - 2);

	/* trim the results incase there were spaces somewhere */
	trim(name);
	trim(val);
	for (auto &p : properties) {

		if (strcasecmp(p->propname.c_str(), name.c_str()) != 0)
			continue;

		if (p->IsDeprecated()) {
			LOG_MSG("CONFIG: Deprecated option '%s'", name.c_str());
			LOG_MSG("CONFIG: %s", p->GetHelp());
			return false;
		}

		return p->SetValue(val);
	}
	LOG_MSG("CONFIG: Unknown option %s", name.c_str());
	return false;
}

void Section_prop::PrintData(FILE* outfile) const {
	/* Now print out the individual section entries */

	// Determine maximum length of the props in this section
	int len = 0;
	for (const auto &tel : properties)
		len = std::max<int>(len, tel->propname.length());

	for (const auto &tel : properties) {

		if (tel->IsDeprecated())
			continue;

		fprintf(outfile, "%-*s = %s\n",
		        std::min<int>(40, len),
		        tel->propname.c_str(),
		        tel->GetValue().ToString().c_str());
	}
}

string Section_prop::GetPropValue(string const& _property) const {
	for(const_it tel = properties.begin();tel != properties.end();++tel){
		if (!strcasecmp((*tel)->propname.c_str(),_property.c_str())){
			return (*tel)->GetValue().ToString();
		}
	}
	return NO_SUCH_PROPERTY;
}

bool Section_line::HandleInputline(const std::string &line)
{
	if (!data.empty()) data += "\n"; //Add return to previous line in buffer
	data += line;
	return true;
}

void Section_line::PrintData(FILE *outfile) const
{
	fprintf(outfile, "%s", data.c_str());
}

std::string Section_line::GetPropValue(const std::string &) const
{
	return NO_SUCH_PROPERTY;
}

bool Config::PrintConfig(const std::string &filename) const
{
	char temp[50];
	char helpline[256];
	FILE *outfile = fopen(filename.c_str(), "w+t");
	if (outfile == NULL) return false;

	/* Print start of configfile and add a return to improve readibility. */
	fprintf(outfile, MSG_Get("CONFIGFILE_INTRO"), VERSION);
	fprintf(outfile, "\n");

	for (auto tel = sectionlist.cbegin(); tel != sectionlist.cend(); ++tel) {
		/* Print out the Section header */
		safe_strcpy(temp, (*tel)->GetName());
		lowcase(temp);
		fprintf(outfile,"[%s]\n",temp);

		Section_prop *sec = dynamic_cast<Section_prop *>(*tel);
		if (sec) {
			Property *p;
			size_t i = 0, maxwidth = 0;
			while ((p = sec->Get_prop(i++))) {
				size_t w = strlen(p->propname.c_str());
				if (w > maxwidth) maxwidth = w;
			}
			i=0;
			char prefix[80];
			int intmaxwidth = std::min<int>(60, maxwidth);
			snprintf(prefix, sizeof(prefix), "\n# %*s  ", intmaxwidth , "");
			while ((p = sec->Get_prop(i++))) {

				if (p->IsDeprecated())
					continue;

				std::string help = p->GetHelp();
				std::string::size_type pos = std::string::npos;
				while ((pos = help.find('\n', pos+1)) != std::string::npos) {
					help.replace(pos, 1, prefix);
				}

				fprintf(outfile, "# %*s: %s", intmaxwidth, p->propname.c_str(), help.c_str());

				std::vector<Value> values = p->GetValues();
				if (!values.empty()) {
					fprintf(outfile, "%s%s:", prefix, MSG_Get("CONFIG_SUGGESTED_VALUES"));
					std::vector<Value>::const_iterator it = values.begin();
					while (it != values.end()) {
						if((*it).ToString() != "%u") { //Hack hack hack. else we need to modify GetValues, but that one is const...
							if (it != values.begin()) fputs(",", outfile);
							fprintf(outfile, " %s", (*it).ToString().c_str());
						}
						++it;
					}
					fprintf(outfile,".");
				}
			fprintf(outfile, "\n");
			}
		} else {
			upcase(temp);
			strcat(temp,"_CONFIGFILE_HELP");
			const char * helpstr = MSG_Get(temp);
			const char * linestart = helpstr;
			char * helpwrite = helpline;
			while (*helpstr && static_cast<size_t>(helpstr - linestart) < sizeof(helpline)) {
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

		fprintf(outfile,"\n");
		(*tel)->PrintData(outfile);
		fprintf(outfile,"\n");		/* Always an empty line between sections */
	}
	fclose(outfile);
	return true;
}

Section_prop *Config::AddEarlySectionProp(const char *name,
                                          SectionFunction func,
                                          bool changeable_at_runtime)
{
	Section_prop *s = new Section_prop(name);
	s->AddEarlyInitFunction(func, changeable_at_runtime);
	sectionlist.push_back(s);
	return s;
}

Section_prop *Config::AddSection_prop(const char *section_name,
                                      SectionFunction func,
                                      bool changeable_at_runtime)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");
	Section_prop *s = new Section_prop(section_name);
	s->AddInitFunction(func, changeable_at_runtime);
	sectionlist.push_back(s);
	return s;
}

Section_prop::~Section_prop()
{
	//ExecuteDestroy should be here else the destroy functions use destroyed properties
	ExecuteDestroy(true);
	/* Delete properties themself (properties stores the pointer of a prop */
	for(it prop = properties.begin(); prop != properties.end(); ++prop)
		delete (*prop);
}

Section_line *Config::AddSection_line(const char *section_name, SectionFunction func)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");
	Section_line *blah = new Section_line(section_name);
	blah->AddInitFunction(func);
	sectionlist.push_back(blah);
	return blah;
}

void Config::Init() const
{
	for (const auto &sec : sectionlist)
		sec->ExecuteEarlyInit();

	for (const auto &sec : sectionlist)
		sec->ExecuteInit();
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
	for (const auto &fn : early_init_functions)
		if (init_all || fn.changeable_at_runtime)
			fn.function(this);
}

void Section::ExecuteInit(bool initall)
{
	for (const auto &fn : initfunctions)
		if (initall || fn.changeable_at_runtime)
			fn.function(this);
}

void Section::ExecuteDestroy(bool destroyall) {
	typedef std::deque<Function_wrapper>::iterator func_it;
	for (func_it tel = destroyfunctions.begin(); tel != destroyfunctions.end(); ) {
		if (destroyall || (*tel).changeable_at_runtime) {
			(*tel).function(this);
			tel = destroyfunctions.erase(tel); //Remove destroyfunction once used
		} else
			++tel;
	}
}

Config::~Config()
{
	for (auto cnt = sectionlist.rbegin(); cnt != sectionlist.rend(); ++cnt)
		delete (*cnt);
}

Section *Config::GetSection(const std::string &section_name) const
{
	for (auto *el : sectionlist) {
		if (!strcasecmp(el->GetName(), section_name.c_str()))
			return el;
	}
	return nullptr;
}

Section *Config::GetSectionFromProperty(const char *prop) const
{
	for (auto *el : sectionlist) {
		if (el->GetPropValue(prop) != NO_SUCH_PROPERTY)
			return el;
	}
	return nullptr;
}

bool Config::ParseConfigFile(char const * const configfilename) {
	//static bool first_configfile = true;
	ifstream in(configfilename);
	if (!in) return false;
	configfiles.push_back(configfilename);

	LOG_MSG("CONFIG: Loading %s file %s",
	        configfiles.size() == 1 ? "primary" : "additional",
	        configfilename);

	//Get directory from configfilename, used with relative paths.
	current_config_dir=configfilename;
	std::string::size_type pos = current_config_dir.rfind(CROSS_FILESPLIT);
	if(pos == std::string::npos) pos = 0; //No directory then erase string
	current_config_dir.erase(pos);

	string gegevens;
	Section* currentsection = NULL;
	Section* testsec = NULL;
	while (getline(in,gegevens)) {

		/* strip leading/trailing whitespace */
		trim(gegevens);
		if(!gegevens.size()) continue;

		switch(gegevens[0]){
		case '%':
		case '\0':
		case '#':
		case ' ':
		case '\n':
			continue;
			break;
		case '[':
		{
			string::size_type loc = gegevens.find(']');
			if(loc == string::npos) continue;
			gegevens.erase(loc);
			testsec = GetSection(gegevens.substr(1));
			if(testsec != NULL ) currentsection = testsec;
			testsec = NULL;
		}
			break;
		default:
			try {
				if(currentsection) currentsection->HandleInputline(gegevens);
			} catch(const char* message) {
				message=0;
				//EXIT with message
			}
			break;
		}
	}
	current_config_dir.clear();//So internal changes don't use the path information
	return true;
}

parse_environ_result_t parse_environ(const char * const * envp) noexcept
{
	assert(envp);

	// Filter envirnment variables in following format:
	// DOSBOX_SECTIONNAME_PROPNAME=VALUE (prefix, section, and property
	// names are case-insensitive).
	std::list<std::tuple<std::string, std::string>> props_to_set;
	for (const char * const *str = envp; *str; str++) {
		const char *env_var = *str;
		if (strncasecmp(env_var, "DOSBOX_", 7) != 0)
			continue;
		const std::string rest = (env_var + 7);
		const auto section_delimiter = rest.find('_');
		if (section_delimiter == string::npos)
			continue;
		const auto section_name = rest.substr(0, section_delimiter);
		if (section_name.empty())
			continue;
		const auto prop_name_and_value = rest.substr(section_delimiter + 1);
		if (prop_name_and_value.empty() || !isalpha(prop_name_and_value[0]))
			continue;
		props_to_set.emplace_back(std::make_tuple(section_name,
		                                          prop_name_and_value));
	}

	return props_to_set;
}

void Config::ParseEnv()
{
#ifdef _MSC_VER
	const char *const *envp = _environ;
#else
	const char *const *envp = environ;
#endif
	if (envp == nullptr)
		return;

	for (const auto &set_prop_desc : parse_environ(envp)) {
		const auto section_name = std::get<0>(set_prop_desc);
		Section *sec = GetSection(section_name);
		if (!sec)
			continue;
		const auto prop_name_and_value = std::get<1>(set_prop_desc);
		sec->HandleInputline(prop_name_and_value);
	}
}

void Config::SetStartUp(void (*_function)(void)) {
	_start_function=_function;
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

	if (user_choice == "high")
		return Verbosity::High;
	if (user_choice == "medium")
		return Verbosity::Medium;
	if (user_choice == "low")
		return Verbosity::Low;
	if (user_choice == "splash_only")
		return Verbosity::SplashOnly;
	if (user_choice == "quiet")
		return Verbosity::Quiet;
	// auto-mode
	if (cmdline->HasDirectory() || cmdline->HasExecutableName())
		return Verbosity::Low;
	else
		return Verbosity::High;
}

bool CommandLine::FindExist(char const * const name,bool remove) {
	cmd_it it;
	if (!(FindEntry(name,it,false))) return false;
	if (remove) cmds.erase(it);
	return true;
}

bool CommandLine::FindInt(char const * const name,int & value,bool remove) {
	cmd_it it,it_next;
	if (!(FindEntry(name,it,true))) return false;
	it_next=it;++it_next;
	value=atoi((*it_next).c_str());
	if (remove) cmds.erase(it,++it_next);
	return true;
}

bool CommandLine::FindString(char const * const name,std::string & value,bool remove) {
	cmd_it it,it_next;
	if (!(FindEntry(name,it,true))) return false;
	it_next=it;++it_next;
	value=*it_next;
	if (remove) cmds.erase(it,++it_next);
	return true;
}

bool CommandLine::FindCommand(unsigned int which,std::string & value) {
	if (which<1) return false;
	if (which>cmds.size()) return false;
	cmd_it it=cmds.begin();
	for (;which>1;which--) it++;
	value=(*it);
	return true;
}

// Was a directory provided on the command line?
bool CommandLine::HasDirectory() const
{
	for (const auto& arg : cmds)
		if (open_directory(arg.c_str()))
			return true;
	return false;
}

// Was an executable filename provided on the command line?
bool CommandLine::HasExecutableName() const
{
	for (const auto& arg : cmds)
		if (is_executable_filename(arg))
			return true;
	return false;
}

bool CommandLine::FindEntry(char const * const name,cmd_it & it,bool neednext) {
	for (it = cmds.begin(); it != cmds.end(); ++it) {
		if (!strcasecmp((*it).c_str(),name)) {
			cmd_it itnext=it;++itnext;
			if (neednext && (itnext==cmds.end())) return false;
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringBegin(char const* const begin,std::string & value, bool remove) {
	size_t len = strlen(begin);
	for (cmd_it it = cmds.begin(); it != cmds.end();++it) {
		if (strncmp(begin,(*it).c_str(),len)==0) {
			value=((*it).c_str() + len);
			if (remove) cmds.erase(it);
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringRemain(char const * const name,std::string & value) {
	cmd_it it;value.clear();
	if (!FindEntry(name,it)) return false;
	++it;
	for (;it != cmds.end();++it) {
		value += " ";
		value += (*it);
	}
	return true;
}

/* Only used for parsing command.com /C
 * Allowing /C dir and /Cdir
 * Restoring quotes back into the commands so command /C mount d "/tmp/a b" works as intended
 */
bool CommandLine::FindStringRemainBegin(char const * const name,std::string & value) {
	cmd_it it;value.clear();
	if (!FindEntry(name,it)) {
		size_t len = strlen(name);
			for (it = cmds.begin();it != cmds.end();++it) {
				if (strncasecmp(name,(*it).c_str(),len)==0) {
					std::string temp = ((*it).c_str() + len);
					//Restore quotes for correct parsing in later stages
					if(temp.find(' ') != std::string::npos)
						value = std::string("\"") + temp + std::string("\"");
					else
						value = temp;
					break;
				}
			}
		if (it == cmds.end()) return false;
	}
	++it;
	for (;it != cmds.end();++it) {
		value += " ";
		std::string temp = (*it);
		if(temp.find(' ') != std::string::npos)
			value += std::string("\"") + temp + std::string("\"");
		else
			value += temp;
	}
	return true;
}

bool CommandLine::GetStringRemain(std::string & value) {
	if (!cmds.size()) return false;

	cmd_it it = cmds.begin();value = (*it++);
	for(;it != cmds.end();++it) {
		value += " ";
		value += (*it);
	}
	return true;
}


unsigned int CommandLine::GetCount(void) {
	return (unsigned int)cmds.size();
}

void CommandLine::FillVector(std::vector<std::string> & vector) {
	for(cmd_it it = cmds.begin(); it != cmds.end(); ++it) {
		vector.push_back((*it));
	}
#ifdef WIN32
	// add back the \" if the parameter contained a space
	for(Bitu i = 0; i < vector.size(); i++) {
		if(vector[i].find(' ') != std::string::npos) {
			vector[i] = "\""+vector[i]+"\"";
		}
	}
#endif
}

int CommandLine::GetParameterFromList(const char* const params[], std::vector<std::string> & output) {
	// return values: 0 = P_NOMATCH, 1 = P_NOPARAMS
	// TODO return nomoreparams
	int retval = 1;
	output.clear();
	enum {
		P_START, P_FIRSTNOMATCH, P_FIRSTMATCH
	} parsestate = P_START;
	cmd_it it = cmds.begin();
	while(it != cmds.end()) {
		bool found = false;
		for(Bitu i = 0; *params[i]!=0; i++) {
			if (!strcasecmp((*it).c_str(),params[i])) {
				// found a parameter
				found = true;
				switch(parsestate) {
				case P_START:
					retval = i+2;
					parsestate = P_FIRSTMATCH;
					break;
				case P_FIRSTMATCH:
				case P_FIRSTNOMATCH:
					return retval;
				}
			}
		}
		if(!found)
			switch(parsestate) {
			case P_START:
				retval = 0; // no match
				parsestate = P_FIRSTNOMATCH;
				output.push_back(*it);
				break;
			case P_FIRSTMATCH:
			case P_FIRSTNOMATCH:
				output.push_back(*it);
				break;
			}
		cmd_it itold = it;
		++it;
		cmds.erase(itold);

	}

	return retval;
/*
bool CommandLine::FindEntry(char const * const name,cmd_it & it,bool neednext) {
	for (it=cmds.begin();it!=cmds.end();it++) {
		if (!strcasecmp((*it).c_str(),name)) {
			cmd_it itnext=it;itnext++;
			if (neednext && (itnext==cmds.end())) return false;
			return true;
		}
	}
	return false;
*/


/*
	cmd_it it=cmds.begin();value=(*it++);
	while(it != cmds.end()) {
		if(params.

		it++;
	}
*/
	// find next parameter
	//return -1;

}

CommandLine::CommandLine(int argc, char const *const argv[])
{
	if (argc>0) {
		file_name=argv[0];
	}
	int i=1;
	while (i<argc) {
		cmds.push_back(argv[i]);
		i++;
	}
}

Bit16u CommandLine::Get_arglength() {
	if (cmds.empty()) return 0;
	Bit16u i=1;
	for(cmd_it it = cmds.begin();it != cmds.end(); ++it)
		i+=(*it).size() + 1;
	return --i;
}

CommandLine::CommandLine(const char *name, const char *cmdline)
{
	if (name) file_name=name;
	/* Parse the cmds and put them in the list */
	bool inword,inquote;char c;
	inword=false;inquote=false;
	std::string str;
	const char * c_cmdline=cmdline;
	while ((c=*c_cmdline)!=0) {
		if (inquote) {
			if (c!='"') str+=c;
			else {
				inquote=false;
				cmds.push_back(str);
				str.erase();
			}
		} else if (inword) {
			if (c!=' ') str+=c;
			else {
				inword=false;
				cmds.push_back(str);
				str.erase();
			}
		}
		else if (c=='\"') { inquote=true;}
		else if (c!=' ') { str+=c;inword=true;}
		c_cmdline++;
	}
	if (inword || inquote) cmds.push_back(str);
}

void CommandLine::Shift(unsigned int amount) {
	while(amount--) {
		file_name = cmds.size()?(*(cmds.begin())):"";
		if(cmds.size()) cmds.erase(cmds.begin());
	}
}
