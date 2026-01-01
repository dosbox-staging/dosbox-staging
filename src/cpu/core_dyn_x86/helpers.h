// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/flags.h"

static bool dyn_helper_divb(uint8_t val) {
	if (!val) return CPU_PrepareException(0,0);
	Bitu quo=reg_ax / val;
	const auto rem  = static_cast<uint8_t>(reg_ax % val);
	const auto quo8 = static_cast<uint8_t>(quo & 0xff);
	if (quo>0xff) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8;
	set_cpu_test_flags_for_division(quo8);
	return false;
}

static bool dyn_helper_idivb(int8_t val) {
	if (!val) return CPU_PrepareException(0,0);
	Bits quo=(int16_t)reg_ax / val;
	const auto rem   = static_cast<int8_t>((int16_t)reg_ax % val);
	const auto quo8s = static_cast<int8_t>(quo & 0xff);
	if (quo!=(int16_t)quo8s) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8s;
	set_cpu_test_flags_for_division(quo8s);
	return false;
}

static bool dyn_helper_divw(uint16_t val) {
	if (!val) return CPU_PrepareException(0,0);
	const uint32_t num   = (((uint32_t)reg_dx)<<16)|reg_ax;
	const uint32_t quo   = num/val;
	const auto     rem   = static_cast<uint16_t>(num % val);
	const auto     quo16 = static_cast<uint16_t>(quo & 0xffff);
	if (quo!=(uint32_t)quo16) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16;
	set_cpu_test_flags_for_division(quo16);
	return false;
}

static bool dyn_helper_idivw(int16_t val) {
	if (!val) return CPU_PrepareException(0,0);
	const int32_t num = (((uint32_t)reg_dx)<<16)|reg_ax;
	const int32_t quo = num/val;
	const auto rem    = static_cast<int16_t>(num % val);
	const auto quo16s = static_cast<int16_t>(quo);
	if (quo!=(int32_t)quo16s) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16s;
	set_cpu_test_flags_for_division(quo16s);
	return false;
}

static bool dyn_helper_divd(uint32_t val) {
	if (!val) return CPU_PrepareException(0,0);
	const uint64_t num = (((uint64_t)reg_edx)<<32)|reg_eax;
	const uint64_t quo = num/val;
	const auto rem     = static_cast<uint32_t>(num % val);
	const auto quo32   = static_cast<uint32_t>(quo & 0xffffffff);
	if (quo!=(uint64_t)quo32) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32;
	set_cpu_test_flags_for_division(quo32);
	return false;
}

static bool dyn_helper_idivd(int32_t val) {
	if (!val) return CPU_PrepareException(0,0);
	const int64_t num = (((uint64_t)reg_edx)<<32)|reg_eax;
	const int64_t quo = num/val;
	const auto rem    = static_cast<int32_t>(num % val);
	const auto quo32s = static_cast<int32_t>(quo & 0xffffffff);
	if (quo!=(int64_t)quo32s) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32s;
	set_cpu_test_flags_for_division(quo32s);
	return false;
}
