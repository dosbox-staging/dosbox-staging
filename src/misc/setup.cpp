/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "cross.h"
#include "setup.h"
#include <fstream>
#include <string>
#include <list>
#include <stdlib.h>
#include "support.h"

using namespace std;


void Prop_int::SetValue(char* input){
	input=trim(input);
	__value._int= atoi(input);
}

void Prop_string::SetValue(char* input){
	input=trim(input);
	__value._string->assign(input);
}
	
void Prop_bool::SetValue(char* input){
	input=trim(input);
	if((input[0]=='0') ||(input[0]=='D')||( (input[0]=='O') && (input[0]=='F'))){
		__value._bool=false;
	}else{
		__value._bool=true;
	}
}
void Prop_hex::SetValue(char* input){
	input=trim(input);
	if(!sscanf(input,"%X",&(__value._hex))) __value._hex=0;
}

void Section_prop::Add_int(const char* _propname, int _value) {
	Property* test=new Prop_int(_propname,_value);
	properties.push_back(test);
}

void Section_prop::Add_string(const char* _propname, char* _value) {
	Property* test=new Prop_string(_propname,_value);
	properties.push_back(test);
}

void Section_prop::Add_bool(const char* _propname, bool _value) {
	Property* test=new Prop_bool(_propname,_value);
	properties.push_back(test);
}
void Section_prop::Add_hex(const char* _propname, int _value) {
	Property* test=new Prop_hex(_propname,_value);
	properties.push_back(test);
}
int Section_prop::Get_int(const char* _propname){
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue())._int;
		}
	}
	return 0;
}

bool Section_prop::Get_bool(const char* _propname){
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue())._bool;
		}
	}
	return false;
}


const char* Section_prop::Get_string(const char* _propname){
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue())._string->c_str();
		}
	}
	return NULL;
}
int Section_prop::Get_hex(const char* _propname){
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue())._hex;
		}
	}
	return 0;
}

void Section_prop::HandleInputline(char *gegevens){
	char * rest=strrchr(gegevens,'=');
	*rest=0;
	gegevens=trim(gegevens);
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==gegevens){
			(*tel)->SetValue(rest+1);
			return;
		}
	}
}


void Section_line::HandleInputline(char* gegevens){ 
	data+=gegevens;
	data+="\n";
}

Section* Config::AddSection(const char* _name,void (*_initfunction)(Section*)){
	Section* blah = new Section(_name,_initfunction);
	sectionlist.push_back(blah);
	return blah;
}

Section_prop* Config::AddSection_prop(const char* _name,void (*_initfunction)(Section*)){
	Section_prop* blah = new Section_prop(_name,_initfunction);
	sectionlist.push_back(blah);
	return blah;
}

Section_line* Config::AddSection_line(const char* _name,void (*_initfunction)(Section*)){
	Section_line* blah = new Section_line(_name,_initfunction);
	sectionlist.push_back(blah);
	return blah;
}


void Config::Init(){
	for (it tel=sectionlist.begin(); tel!=sectionlist.end(); tel++){ 
		(*tel)->ExecuteInit();
	}
}

Section* Config::GetSection(const char* _sectionname){
	for (it tel=sectionlist.begin(); tel!=sectionlist.end(); tel++){
		if ( (*tel)->sectionname==_sectionname) return (*tel);
	}
	return NULL;
}

void Config::ParseConfigFile(const char* configfilename){
	ifstream in(configfilename);
	if (!in) LOG_MSG("CONFIG:Can't find config file %s, using default settings",configfilename);
	char gegevens[150];
	Section* currentsection;
	while (in) {
		in.getline(gegevens,150);
		char* temp;
		switch(gegevens[0]){
		case '%':
		case '\0':
		case '\n':
		case ' ':
			continue;
			break;
		case '[':
			temp = strrchr(gegevens,']');
			*temp=0;
			currentsection=GetSection(&gegevens[1]);
			break;
		default:
			try{
				currentsection->HandleInputline(gegevens);
			}catch(const char* message){
				message=0;
				//EXIT with message
			}
			break;
		}
	}
}



