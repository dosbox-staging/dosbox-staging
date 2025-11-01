// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Empty functions to suppress output in unit tests as well as a to eliminate
// dependency-sprawl

#include "misc/messages.h"

void MSG_NotifyNewCodePage() {}

void MSG_Add(const std::string&, const std::string&, const MessageLocation&) {}

std::string MSG_Get(const std::string&)
{
	return {};
}

std::string MSG_GetEnglishRaw(const std::string&)
{
	return {};
}

std::string MSG_GetTranslatedRaw(const std::string&)
{
	return {};
}

std::string MSG_GetLanguage()
{
	return "en";
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
