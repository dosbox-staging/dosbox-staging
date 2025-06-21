// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/* ARMv4/ARMv7 (little endian) backend (switcher) by M-HT */

#include "risc_armv4le-common.h"

// choose your destiny:
#if C_TARGETCPU == ARMV7LE
	#include "risc_armv4le-o3.h"
#else
	#if defined(__THUMB_INTERWORK__)
		#include "risc_armv4le-thumb-iw.h"
	#else
		#include "risc_armv4le-o3.h"
//		#include "risc_armv4le-thumb-niw.h"
//		#include "risc_armv4le-thumb.h"
	#endif
#endif
