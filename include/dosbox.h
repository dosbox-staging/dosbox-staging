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

#if !defined __DOSBOX_H
#define __DOSBOX_H


void E_Exit(char * message,...);

void S_Warn(char * message,...);

void AddMessage(const char*,const char*); //add messages to the internal langaugefile
const char* MSG_Get(char const *);         //get messages from the internal langaugafile

/* The internal types */
typedef  unsigned char     Bit8u;
typedef    signed char     Bit8s;
typedef unsigned short     Bit16u;
typedef   signed short     Bit16s;
typedef  unsigned long     Bit32u;
typedef    signed long     Bit32s;
typedef         double     Real64;
#if defined(_MSC_VER)
typedef unsigned __int64   Bit64u;
typedef   signed __int64   Bit64s;
#else
typedef unsigned long long Bit64u;
typedef   signed long long Bit64s;
#endif

typedef unsigned int Bitu;
typedef signed int Bits;

#include <stddef.h>
#include "config.h"
#include "../settings.h"

class Section;


typedef Bitu (LoopHandler)(void);

void DOSBOX_RunMachine();
void DOSBOX_SetLoop(LoopHandler * handler);
void DOSBOX_SetNormalLoop();

void DOSBOX_Init(void);

class Config;
extern Config * control;
extern Bitu errorlevel;

inline void LOG_MSG(char* message)
{
      if(errorlevel>=0) S_Warn(message);
}

template <class type1>
inline void LOG_MSG(char* message,type1 arg1)
{
   
      if(errorlevel>=0) S_Warn(message,arg1);
}

template <class type1,class type2>
inline void LOG_MSG(char* message,type1 arg1,type2 arg2)
{
   
      if(errorlevel>=0) S_Warn(message,arg1,arg2);
}

template <class type1,class type2, class type3>
inline void LOG_MSG(char* message,type1 arg1,type2 arg2,type3 arg3)
{
   
      if (errorlevel>=0)S_Warn(message,arg1,arg2,arg3);
}

#if C_LOGGING
inline void LOG_DEBUG(char * message)
{
   
      if(errorlevel>=2) S_Warn(message);
}

template <class type>
inline void LOG_DEBUG(char * message, type type1)
{
   
      if(errorlevel>=2) S_Warn(message,type1);
}

template <class type>
inline void LOG_WARN(char * message, type type1)
{
   
      if(errorlevel>=1) S_Warn(message,type1);
}

inline void LOG_WARN(char* message)
{
   
        if(errorlevel>=1) S_Warn(message);
}

inline void LOG_ERROR(char * message)
{
   
      if(errorlevel>=0) S_Warn(message);
}

template <class type>
inline void LOG_ERROR(char * message, type type1)
{
   
      if(errorlevel>=0) S_Warn(message,type1);
}

template <class type1, class type2>
inline void LOG_ERROR(char * message, type1 arg1,type2 arg2)
{
   
      if(errorlevel>=0) S_Warn(message,arg1,arg2);
}

template <class type1, class type2>
inline void LOG_WARN(char * message, type1 arg1,type2 arg2)
{
   
      if(errorlevel>=1) S_Warn(message,arg1,arg2);
}

template <class type1, class type2>
inline void LOG_DEBUG(char * message, type1 arg1,type2 arg2)
{
   
      if(errorlevel>=2) S_Warn(message,arg1,arg2);
}

#else
#define LOG_DEBUG
#define LOG_WARN
#define LOG_ERROR
#endif




#endif

