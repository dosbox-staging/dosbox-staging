// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// This header file is generated.
// See "extras/generate_tracy_header" for details.

#ifndef DOSBOX_TRACY_H
#define DOSBOX_TRACY_H

#if C_TRACY
#	ifndef TRACY_ENABLE
#		define TRACY_ENABLE 1
#	endif
#else
#	ifdef TRACY_ENABLE
#		undef TRACY_ENABLE
#	endif
#endif

#ifndef TRACY_ENABLE

#	define ZoneNamed(x, y)
#	define ZoneNamedN(x, y, z)
#	define ZoneNamedC(x, y, z)
#	define ZoneNamedNC(x, y, z, w)

#	define ZoneTransient(x, y)
#	define ZoneTransientN(x, y, z)

#	define ZoneScoped
#	define ZoneScopedN(x)
#	define ZoneScopedC(x)
#	define ZoneScopedNC(x, y)

#	define ZoneText(x, y)
#	define ZoneTextV(x, y, z)
#	define ZoneName(x, y)
#	define ZoneNameV(x, y, z)
#	define ZoneColor(x)
#	define ZoneColorV(x, y)
#	define ZoneValue(x)
#	define ZoneValueV(x, y)
#	define ZoneIsActive     false
#	define ZoneIsActiveV(x) false

#	define FrameMark
#	define FrameMarkNamed(x)
#	define FrameMarkStart(x)
#	define FrameMarkEnd(x)

#	define FrameImage(x, y, z, w, a)

#	define TracyLockable(type, varname)              type varname;
#	define TracyLockableN(type, varname, desc)       type varname;
#	define TracySharedLockable(type, varname)        type varname;
#	define TracySharedLockableN(type, varname, desc) type varname;
#	define LockableBase(type)                        type
#	define SharedLockableBase(type)                  type
#	define LockMark(x)                               (void)x;
#	define LockableName(x, y, z)                     ;

#	define TracyPlot(x, y)
#	define TracyPlotConfig(x, y, z, w, a)

#	define TracyMessage(x, y)
#	define TracyMessageL(x)
#	define TracyMessageC(x, y, z)
#	define TracyMessageLC(x, y)
#	define TracyAppInfo(x, y)

#	define TracyAlloc(x, y)
#	define TracyFree(x)
#	define TracySecureAlloc(x, y)
#	define TracySecureFree(x)

#	define TracyAllocN(x, y, z)
#	define TracyFreeN(x, y)
#	define TracySecureAllocN(x, y, z)
#	define TracySecureFreeN(x, y)

#	define ZoneNamedS(x, y, z)
#	define ZoneNamedNS(x, y, z, w)
#	define ZoneNamedCS(x, y, z, w)
#	define ZoneNamedNCS(x, y, z, w, a)

#	define ZoneTransientS(x, y, z)
#	define ZoneTransientNS(x, y, z, w)

#	define ZoneScopedS(x)
#	define ZoneScopedNS(x, y)
#	define ZoneScopedCS(x, y)
#	define ZoneScopedNCS(x, y, z)

#	define TracyAllocS(x, y, z)
#	define TracyFreeS(x, y)
#	define TracySecureAllocS(x, y, z)
#	define TracySecureFreeS(x, y)

#	define TracyAllocNS(x, y, z, w)
#	define TracyFreeNS(x, y, z)
#	define TracySecureAllocNS(x, y, z, w)
#	define TracySecureFreeNS(x, y, z)

#	define TracyMessageS(x, y, z)
#	define TracyMessageLS(x, y)
#	define TracyMessageCS(x, y, z, w)
#	define TracyMessageLCS(x, y, z)

#	define TracySourceCallbackRegister(x, y)
#	define TracyParameterRegister(x, y)
#	define TracyParameterSetup(x, y, z, w)
#	define TracyIsConnected false

#	define TracyFiberEnter(x)
#	define TracyFiberLeave

#else
#	include <tracy/Tracy.hpp>
#endif

// close the header
#endif
