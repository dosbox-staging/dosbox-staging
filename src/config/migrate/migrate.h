// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_MIGRATE_H
#define DOSBOX_CONFIG_MIGRATE_MIGRATE_H

#include "misc/std_filesystem.h"

namespace ConfigMigrate {

struct Options {
	bool dry_run = true;
};

// Walk `root` (a file or directory), migrate every DOSBox config found, and
// report. Returns the process exit code: 0 on success, non-zero on error.
int Run(const std_fs::path& root, const Options& opts);

} // namespace ConfigMigrate

#endif
