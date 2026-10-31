// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone differential oracle. See docs/fpu-80bit.md for build/run commands.
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#ifdef SOFTWARE
#include "cpu/cpu.h"
#include "dosbox.h"
#include "fpu/fpu.h"
#include "fpu/fpu_instructions.h"
FPU_rec fpu = {};
struct Raw {
	uint64_t sig;
	uint16_t exp;
};
#else
struct __attribute__((packed)) Raw {
	uint64_t sig;
	uint16_t exp;
};
#endif
int main(int argc, char** argv)
{
	std::mt19937_64 rng(15757);
	int count = argc > 1 ? atoi(argv[1]) : 100000;
	for (int i = 0; i < count; ++i) {
		Raw a{rng() | (UINT64_C(1) << 63), static_cast<uint16_t>(rng())};
		Raw b{rng() | (UINT64_C(1) << 63), static_cast<uint16_t>(rng())};
		if ((i % 5) == 0) {
			a.exp = static_cast<uint16_t>(0x3fff + (rng() % 128) - 64);
			b.exp = static_cast<uint16_t>(0x3fff + (rng() % 128) - 64);
		}
		if ((a.exp & 0x7fff) == 0x7fff) {
			a.exp--;
		}
#ifdef SPECIAL
		static const Raw special[] = {
		        {                           0,      0},
		        {                           0, 0x8000},
		        {                           1,      0},
		        {UINT64_C(0x8000000000000000),      0},
		        {UINT64_C(0x8000000000000000), 0x7fff},
		        {UINT64_C(0x8000000000000000), 0xffff},
		        {UINT64_C(0xc000000000000001), 0x7fff},
		        {UINT64_C(0x8000000000000001), 0x7fff},
		        {UINT64_C(0x4000000000000000), 0x3fff},
		        {                           0, 0x7fff}
                };
		a = special[(i / (9 * 12 * 10)) % 10];
		b = special[(i / (9 * 12)) % 10];
#endif
#ifndef SPECIAL
		if ((b.exp & 0x7fff) == 0x7fff) {
			b.exp--;
		}
#endif
		int pc      = (i / 4) % 3;
		pc          = pc == 0 ? 0 : pc + 1;
		uint16_t cw = 0x7f | (pc << 8) | ((i % 4) << 10), sw = 0;
		int op = (i / 12) % 9;
#ifndef SPECIAL
		if (op == 6 || op == 7) {
			a.exp = (a.exp & 0x8000) | (0x3fff + rng() % 63);
			b.exp = (b.exp & 0x8000) | 0x3fff;
		}
#endif
#ifdef UNMASKED
		cw &= ~(8 << ((i / 108) % 3));
#endif
		Raw out{};
#ifdef SOFTWARE
		FPU_FINIT();
		FPU_SetCW(cw);
		FPU_put(0, x87::raw(a.exp, a.sig));
		FPU_put(1, x87::raw(b.exp, b.sig));
		switch (op) {
		case 0: FPU_FADD(0, 1); break;
		case 1: FPU_FSUB(0, 1); break;
		case 2: FPU_FMUL(0, 1); break;
		case 3: FPU_FDIV(0, 1); break;
		case 4: FPU_FSQRT(); break;
		case 5: FPU_FRNDINT(); break;
		case 6: FPU_FPREM(); break;
		case 7: FPU_FPREM1(); break;
		case 8: FPU_FSCALE(); break;
		}
		out = {fpu.regs[0].value.signif, fpu.regs[0].value.signExp};
		sw  = fpu.sw;
#else
		asm volatile("fninit; fldcw %0; fldt %1; fldt %2" ::"m"(cw),
		             "m"(b),
		             "m"(a)
		             : "st");
		switch (op) {
		case 0: asm volatile("fadd %%st(1),%%st(0)" ::: "st"); break;
		case 1: asm volatile("fsub %%st(1),%%st(0)" ::: "st"); break;
		case 2: asm volatile("fmul %%st(1),%%st(0)" ::: "st"); break;
		case 3: asm volatile("fdiv %%st(1),%%st(0)" ::: "st"); break;
		case 4: asm volatile("fsqrt" ::: "st"); break;
		case 5: asm volatile("frndint" ::: "st"); break;
		case 6: asm volatile("fprem" ::: "st"); break;
		case 7: asm volatile("fprem1" ::: "st"); break;
		case 8: asm volatile("fscale" ::: "st"); break;
		}
#ifdef UNMASKED
		uint8_t saved[108] = {};
		asm volatile("fnsave %0" : "=m"(saved)::"st");
		memcpy(&out, saved + 28, 10);
		sw = saved[4] | (saved[5] << 8);
#else
		asm volatile("fnstsw %0; fstpt %1; fninit"
		             : "=m"(sw), "=m"(out)::"st");
#endif
#endif
		printf("%04x %016llx %04x\n",
		       out.exp,
		       (unsigned long long)out.sig,
		       sw & ((op == 6 || op == 7 ? 0x473f : 0x23f)
#ifdef UNMASKED
		             | 0x8080
#endif
		             ));
	}
}
