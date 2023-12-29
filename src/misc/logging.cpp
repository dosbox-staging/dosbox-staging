/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#include <memory>

#include "control.h"
#include "dosbox.h"
#include "logging.h"
#include "setup.h"
#include "string_utils.h"

static loguru::Verbosity ConvertSeverityToVerbosity(LOG_SEVERITIES severity)
{
	switch (severity) {
	case LOG_NORMAL: return loguru::Verbosity_INFO;
	case LOG_WARN: return loguru::Verbosity_WARNING;
	case LOG_ERROR: return loguru::Verbosity_ERROR;
	// This should never happen, but if it does, we'll set the verbosity to
	// be as low as possible.
	default: return loguru::Verbosity_9;
	}
}

struct _LogGroup {
	const char* front = nullptr;
	bool enabled      = false;
};

static _LogGroup loggrp[LOG_MAX] = {{"", true}, {nullptr, false}};

// A thread_local value that is used to pass along the message type (if any) to
// the logger.
//
// This makes the assumption that the log callback will be called in the dynamic
// scope of the log call itself. If this does not happen, then the value will be
// lost.
thread_local LOG_TYPES curr_msg_log_type = LOG_ALL;

static void OnLogCallback(void* logger, const loguru::Message& message)
{
	static_cast<Logger*>(logger)->Log(loggrp[curr_msg_log_type].front, message);
}

static void OnFlushCallback(void* logger)
{
	static_cast<Logger*>(logger)->Flush();
}

static void OnCloseCallback(void* logger)
{
	delete static_cast<Logger*>(logger);
}

void Logger::AddLogger(const char* id, std::unique_ptr<Logger> logger,
                       LOG_SEVERITIES severity)
{
	loguru::Verbosity verbosity = ConvertSeverityToVerbosity(severity);
	loguru::add_callback(id,
	                     OnLogCallback,
	                     logger.release(),
	                     verbosity,
	                     OnCloseCallback,
	                     OnFlushCallback);
}

void Logger::RemoveLogger(const char* id)
{
	loguru::remove_callback(id);
}

static const char* const LOGURU_FILE_CALLBACK_ID = "dosbox_file_logger";

class FileLogger : public Logger {
public:
	explicit FileLogger(FILE* file_ptr) : file_ptr_(file_ptr) {}
	~FileLogger()
	{
		fclose(file_ptr_);
	}

	FileLogger(const FileLogger&)            = delete;
	FileLogger& operator=(const FileLogger&) = delete;

	void Log(const char* log_group_name, const loguru::Message& message) override
	{
		fprintf(file_ptr_,
		        "%s%s:%s\n",
		        message.preamble,
		        log_group_name,
		        message.message);
	}

	void Flush() override
	{
		fflush(file_ptr_);
	}

private:
	FILE* file_ptr_;
};

static void LOG_Destroy(Section*)
{
	Logger::RemoveLogger(LOGURU_FILE_CALLBACK_ID);
}

static void LOG_Init(Section* sec)
{
	Section_prop* sect = static_cast<Section_prop*>(sec);
	std::string blah   = sect->Get_string("logfile");
	FILE* debuglog;
	if (!blah.empty() && (debuglog = fopen(blah.c_str(), "wt+"))) {
		Logger::AddLogger(LOGURU_FILE_CALLBACK_ID,
		                  std::make_unique<FileLogger>(debuglog),
		                  LOG_WARN);
		sect->AddDestroyFunction(&LOG_Destroy);
	}
	char buf[64];
	for (Bitu i = LOG_ALL + 1; i < LOG_MAX; i++) { // Skip LOG_ALL, it is
		                                       // always enabled
		safe_strcpy(buf, loggrp[i].front);
		lowcase(buf);
		loggrp[i].enabled = sect->Get_bool(buf);
	}
}

void LOG_StartUp(void)
{
	/* Setup logging groups */
	loggrp[LOG_ALL].front        = "ALL";
	loggrp[LOG_VGA].front        = "VGA";
	loggrp[LOG_VGAGFX].front     = "VGAGFX";
	loggrp[LOG_VGAMISC].front    = "VGAMISC";
	loggrp[LOG_INT10].front      = "INT10";
	loggrp[LOG_SB].front         = "SBLASTER";
	loggrp[LOG_DMACONTROL].front = "DMA_CONTROL";

	loggrp[LOG_FPU].front    = "FPU";
	loggrp[LOG_CPU].front    = "CPU";
	loggrp[LOG_PAGING].front = "PAGING";

	loggrp[LOG_FCB].front     = "FCB";
	loggrp[LOG_FILES].front   = "FILES";
	loggrp[LOG_IOCTL].front   = "IOCTL";
	loggrp[LOG_EXEC].front    = "EXEC";
	loggrp[LOG_DOSMISC].front = "DOSMISC";

	loggrp[LOG_PIT].front      = "PIT";
	loggrp[LOG_KEYBOARD].front = "KEYBOARD";
	loggrp[LOG_PIC].front      = "PIC";

	loggrp[LOG_MOUSE].front = "MOUSE";
	loggrp[LOG_BIOS].front  = "BIOS";
	loggrp[LOG_GUI].front   = "GUI";
	loggrp[LOG_MISC].front  = "MISC";

	loggrp[LOG_IO].front        = "IO";
	loggrp[LOG_PCI].front       = "PCI";
	loggrp[LOG_REELMAGIC].front = "REELMAGIC";

	/* Register the log section */
	Section_prop* sect   = control->AddSection_prop("log", LOG_Init);
	Prop_string* Pstring = sect->Add_string("logfile",
	                                        Property::Changeable::Always,
	                                        "");
	Pstring->Set_help("File where the log messages will be saved to");
	char buf[64];
	for (Bitu i = LOG_ALL + 1; i < LOG_MAX; i++) {
		safe_strcpy(buf, loggrp[i].front);
		lowcase(buf);
		Prop_bool* Pbool = sect->Add_bool(buf,
		                                  Property::Changeable::Always,
		                                  true);
		Pbool->Set_help("Enable/disable logging of this type.");
	}
	//	MSG_Add("LOG_CONFIGFILE_HELP","Logging related options for the
	// debugger.\n");
}

void LogHelper::operator()(const char* format, ...)
{
	if (d_type >= LOG_MAX) {
		return;
	}
	if ((d_severity != LOG_ERROR) && (!loggrp[d_type].enabled)) {
		return;
	}

	loguru::Verbosity verbosity = ConvertSeverityToVerbosity(d_severity);

	// On the current thread, update the log type so that Logger classes
	// will get the correct value when they are called.
	auto old_msg_log_type = curr_msg_log_type;
	curr_msg_log_type     = d_type;

	va_list msg;
	va_start(msg, format);
	loguru::vlog(verbosity, file, line, format, msg);
	va_end(msg);

	curr_msg_log_type = old_msg_log_type;
}