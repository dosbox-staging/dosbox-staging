// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone FSIN/FCOS probe: input operation, raw exponent, significand, CW.
// operation 0 = sine, 1 = cosine; compile with SOFTWARE for this backend.
// NATIVE_DOSBOX selects the native wrapper's host-CW behavior instead of guest RC.
#include <cstdint>
#include <cstdio>
#ifdef SOFTWARE
#include "cpu/cpu.h"
#include "dosbox.h"
#include "fpu/fpu.h"
#include "fpu/fpu_instructions.h"
FPU_rec fpu = {};
#endif
struct __attribute__((packed)) Raw {
	uint64_t sig;
	uint16_t exp;
};
int main()
{
	int op;
	unsigned exp, cw;
	unsigned long long sig;
	while (scanf("%d %x %llx %x", &op, &exp, &sig, &cw) == 4) {
		Raw a{sig, (uint16_t)exp}, out{};
		uint16_t c = cw, sw;
#ifdef SOFTWARE
		FPU_FINIT();
		FPU_SetCW(c);
		FPU_put(0, x87::raw(exp, sig));
		if (op == 0) {
			FPU_FSIN();
		} else {
			FPU_FCOS();
		}
		out = {fpu.regs[0].value.signif, fpu.regs[0].value.signExp};
		sw  = fpu.sw;
#else
		// FPUD_TRIG in the native DOSBox backend does not
		// load the guest CW. FNINIT supplies the host's default nearest
		// RC for a reproducible comparison of that wrapper's behavior.
#ifdef NATIVE_DOSBOX
		asm volatile("fninit; fldt %0; fclex" ::"m"(a) : "st");
#else
		asm volatile("fninit; fldcw %0; fldt %1" ::"m"(c), "m"(a)
		             : "st");
#endif
		if (op == 0) {
			asm volatile("fsin" ::: "st");
		} else {
			asm volatile("fcos" ::: "st");
		}
		asm volatile("fnstsw %0; fstpt %1; fninit"
		             : "=m"(sw), "=m"(out)::"st");
#endif
		printf("%d %04x %016llx %04x -> %04x %016llx %04x\n",
		       op,
		       exp,
		       sig,
		       cw,
		       out.exp,
		       (unsigned long long)out.sig,
		       sw & 0x23f);
	}
}
