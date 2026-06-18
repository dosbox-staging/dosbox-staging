// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_CYCLES_PARSER_H
#define DOSBOX_CONFIG_MIGRATE_CYCLES_PARSER_H

#include "rules.h"

namespace ConfigMigrate {

// Parse a legacy `[cpu] cycles = ...` value and emit the equivalent
// `cpu_cycles` / `cpu_cycles_protected` lines. Lossy modifiers
// (`<N>%` percent caps, `limit <N>` upper bounds) are dropped with a
// WARNING. Unrecognised input is preserved unchanged with a WARNING so
// the runtime legacy parser can keep handling it.
//
// Mirrors `ConfigureCyclesLegacy` in src/cpu/cpu.cpp; see config-findings.md
// for the full mapping table.
ApplyResult MigrateLegacyCycles(const InputLine& line);

} // namespace ConfigMigrate

#endif
