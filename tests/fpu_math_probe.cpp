// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone software transcendental probe for check_fpu_math.py.
#include "fpu/float80.h"
#include <cstdio>
using namespace softfloat;
int main()
{
	int op, rc;
	unsigned ex;
	unsigned long long sig;
	while (scanf("%d %d %x %llx", &op, &rc, &ex, &sig) == 4) {
		const int modes[]        = {0, 2, 3, 1};
		softfloat_roundingMode   = modes[rc];
		extF80_roundingPrecision = 80;
		softfloat_exceptionFlags = 0;
		auto a = x87::raw(ex, sig), out = a;
		switch (op) {
		case 0: {
			x87::Float c;
			x87::sincos(a, out, c);
			break;
		}
		case 1: {
			x87::Float s;
			x87::sincos(a, s, out);
			break;
		}
		case 2: out = x87::tangent(a); break;
		case 3: out = x87::exp2m1(a); break;
		case 4: out = x87::log2(a); break;
		case 5: out = x87::log2(a, true); break;
		case 6: out = x87::atan2(a, x87::one()); break;
		}
		printf("%04x %016llx\n", out.signExp, (unsigned long long)out.signif);
	}
}
