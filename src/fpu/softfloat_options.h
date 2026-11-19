// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOSBOX_SOFTFLOAT_OPTIONS_H
#define DOSBOX_SOFTFLOAT_OPTIONS_H

// opts-GCC.h requires INLINE and the platform byte order before it is read.
// Force-include this only in SoftFloat C translation units, never in clients
// that wrap the public SoftFloat declarations in a C++ namespace.
#include "platform.h"
#include "opts-GCC.h"

#endif
