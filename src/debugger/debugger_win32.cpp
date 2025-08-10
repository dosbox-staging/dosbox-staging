// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"
#if C_DEBUGGER
#ifdef WIN32

#include <cstdarg>
#include <cstdio>
#include <windows.h>

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/* 
	Have to remember where i ripped this code sometime ago.

*/
static void ResizeConsole( HANDLE hConsole, SHORT xSize, SHORT ySize ) {   
	CONSOLE_SCREEN_BUFFER_INFO csbi; // Hold Current Console Buffer Info 
	BOOL bSuccess;   
	SMALL_RECT srWindowRect;         // Hold the New Console Size 
	COORD coordScreen;    
	
	bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
	
	// Get the Largest Size we can size the Console Window to 
	coordScreen = GetLargestConsoleWindowSize( hConsole );
	
	// Define the New Console Window Size and Scroll Position 
	srWindowRect.Right  = (SHORT)(min(xSize, coordScreen.X) - 1);
	srWindowRect.Bottom = (SHORT)(min(ySize, coordScreen.Y) - 1);
	srWindowRect.Left   = srWindowRect.Top = (SHORT)0;
	
	// Define the New Console Buffer Size    
	coordScreen.X = xSize;
	coordScreen.Y = ySize;
	
	// If the Current Buffer is Larger than what we want, Resize the 
	// Console Window First, then the Buffer 
	if( (DWORD)csbi.dwSize.X * csbi.dwSize.Y > (DWORD) xSize * ySize)
	{
		bSuccess = SetConsoleWindowInfo( hConsole, TRUE, &srWindowRect );
		bSuccess = SetConsoleScreenBufferSize( hConsole, coordScreen );
	}
	
	// If the Current Buffer is Smaller than what we want, Resize the 
	// Buffer First, then the Console Window 
	if( (DWORD)csbi.dwSize.X * csbi.dwSize.Y < (DWORD) xSize * ySize )
	{
		bSuccess = SetConsoleScreenBufferSize( hConsole, coordScreen );
		bSuccess = SetConsoleWindowInfo( hConsole, TRUE, &srWindowRect );
	}
	
	// If the Current Buffer *is* the Size we want, Don't do anything! 
	return;
   }


void WIN32_Console() {
	AllocConsole();
	SetConsoleTitle("DOSBox Debugger");
	ResizeConsole(GetStdHandle(STD_OUTPUT_HANDLE),80,50);
	HWND hWnd = GetConsoleWindow();
	SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) &
		~(WS_SIZEBOX | WS_MAXIMIZEBOX | WS_VSCROLL | WS_HSCROLL ));
}
#endif
#endif
