// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "choice.h"

#include <string>

#include "shell/shell.h"
#include "utils/string_utils.h"

extern unsigned int result_errorcode;

void CHOICE::Run()
{
	std::string tmp = "";
	cmd->GetStringRemain(tmp);

	char args[CMD_MAXLINE];
	safe_strcpy(args, tmp.c_str());

	auto shell = DOS_GetFirstShell();
	assert(shell);

	shell->CMD_CHOICE(args);
	result_errorcode = dos.return_code;
}
