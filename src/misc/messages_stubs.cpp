// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Empty functions to suppress output in unit tests as well as a to eliminate
// dependency-sprawl

#include "messages.h"

void MSG_NotifyNewCodePage() {}

void MSG_Add(const std::string&, const std::string&) {}

const char* MSG_Get(const std::string&)
{
	return {};
}

const char* MSG_GetEnglishRaw(const std::string&)
{
	return {};
}

const char* MSG_GetTranslatedRaw(const std::string&)
{
	return {};
}

bool MSG_Exists(const std::string&)
{
	return true;
}

bool MSG_WriteToFile(const std::string&)
{
	return true;
}

void MSG_LoadMessages() {}
