// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/pacer.h"

#include <cinttypes>

Pacer::Pacer(const std::string &name, const int timeout, const LogLevel level)
        : pacer_name(name),
          iteration_start(GetTicksUs())
{
	assert(pacer_name.size() > 0);
	SetTimeout(timeout);
	SetLogLevel(level);
}

bool Pacer::CanRun()
{
	if (skip_timeout == 0)
		return true;

	if (can_run)
		iteration_start = GetTicksUs();
	return can_run;
}

void Pacer::Checkpoint()
{
	// Pacer is disabled
	if (skip_timeout == 0)
		return;

	// Pacer is reset
	if (was_reset) {
		if (log_level == LogLevel::CHECKPOINTS)
			LOG_MSG("PACER: %s reset ignored %" PRId64 "us of latency",
			        pacer_name.c_str(),
			        GetTicksUsSince(iteration_start));
		was_reset = false;
		return;
	}

	// Pacer was run, so compare the runtime against the timeout
	if (can_run) {
		const auto iteration_took = GetTicksUsSince(iteration_start);
		can_run = (iteration_took < skip_timeout);

		if (log_level != LogLevel::NOTHING) {
			if (!can_run) {
				LOG_WARNING("PACER: %s took %" PRId64 "us, skipping next",
				            pacer_name.c_str(), iteration_took);
			} else if (log_level == LogLevel::CHECKPOINTS) {
				LOG_MSG("PACER: %s took %" PRId64 "us, can run next",
				        pacer_name.c_str(), iteration_took);
			}
		}
		return;
	}

	// Otherwise we didn't run the previous time, so allow the next.
	assert(!can_run);
	can_run = true;
}

void Pacer::Reset()
{
	can_run = true;
	was_reset = true;
}

void Pacer::SetLogLevel(const LogLevel level)
{
	log_level = level;
}

void Pacer::SetTimeout(const int timeout)
{
	assert(timeout >= 0);
	skip_timeout = timeout;
}
