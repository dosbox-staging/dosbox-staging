// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Runs string instructions through every CPU core in protected mode with an
// ES selector whose limit is smaller than the destination range, and pins
// down the fault semantics documented in the Intel manuals:
//
// - A byte at offset > limit, a word at offset > limit - 1 and a dword at
//   offset > limit - 3 raise #GP(0) before anything is written.
// - A REP-prefixed instruction executes the iterations that fit, then faults
//   with (E)CX, (E)SI and (E)DI reflecting the completed iterations and the
//   pushed EIP pointing at the instruction, prefixes included.
// - With a 16-bit address size, (E)DI wraps within the segment after each
//   element; a single element never wraps.
// - A REP left part-way through (the cycle budget ran out, or an interrupt
//   arrived) resumes from the instruction start with the registers as they
//   were, and the limit of whatever ES holds at that point applies to the
//   remaining iterations.
//
// The last point is what allows the cores to check the limit once per start
// or restart of the instruction instead of once per iteration; see
// CPU_CheckStringEsLimit() in cpu.cpp.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

#include "config/config.h"
#include "cpu/cpu.h"
#include "cpu/paging.h"
#include "cpu/registers.h"
#include "cpu_config_param.h"
#include "dosbox_test_fixture.h"
#include "memory.h"
#include "misc/logging.h"

namespace {

// Linear addresses used by the test, in the top of conventional memory
constexpr PhysPt EsBase       = 0x50000;
constexpr PhysPt SourceBase   = 0x7a000;
constexpr PhysPt GdtBase      = 0x70000;
constexpr PhysPt IdtBase      = 0x71000;
constexpr PhysPt HandlersBase = 0x72000;
constexpr PhysPt MarkerAddr   = 0x73000;
constexpr PhysPt CodeBase     = 0x74000;
constexpr uint32_t StackTop   = 0x7f000;

constexpr uint32_t CodeSlotSize    = 64;
constexpr uint32_t HandlerSlotSize = 16;

constexpr uint16_t CodeSelector     = 0x08;
constexpr uint16_t FlatDataSelector = 0x10;
constexpr uint16_t Es64KSelector    = 0x18; // base EsBase, limit 0xffff
constexpr uint16_t Es128KSelector   = 0x20; // base EsBase, limit 0x1ffff

// Written to MarkerAddr by the fault handler stubs (vector number) or by the
// code following the instruction under test
constexpr uint8_t NoMarker       = 0xff;
constexpr uint8_t FinishedMarker = 0xfe;
constexpr uint8_t GpVector       = 13;

// The handlers of these vectors reload ES and return to the interrupted
// instruction, like an interrupt handler that changes ES in the middle of a
// REP would
constexpr uint8_t SwitchEsTo64KVector  = 0x40;
constexpr uint8_t SwitchEsTo128KVector = 0x41;

constexpr uint32_t Fill      = 0x11223344;
constexpr uint32_t Untouched = 0xdeadbeef;

constexpr uint8_t Hlt = 0xf4;

void write_descriptor(const PhysPt addr, const uint32_t base, const uint32_t limit,
                      const uint8_t access, const uint8_t flags)
{
	mem_writew(addr + 0, limit & 0xffff);
	mem_writew(addr + 2, base & 0xffff);
	mem_writeb(addr + 4, (base >> 16) & 0xff);
	mem_writeb(addr + 5, access);
	mem_writeb(addr + 6, ((limit >> 16) & 0x0f) | flags);
	mem_writeb(addr + 7, (base >> 24) & 0xff);
}

void write_interrupt_gate(const PhysPt addr, const uint16_t selector,
                          const uint32_t offset)
{
	mem_writew(addr + 0, offset & 0xffff);
	mem_writew(addr + 2, selector);
	mem_writeb(addr + 4, 0);
	mem_writeb(addr + 5, 0x8e); // present, DPL 0, 386 interrupt gate
	mem_writew(addr + 6, offset >> 16);
}

PhysPt write_bytes(PhysPt addr, const std::vector<uint8_t>& bytes)
{
	for (const auto byte : bytes) {
		mem_writeb(addr++, byte);
	}
	return addr;
}

// mov byte [MarkerAddr], value
std::vector<uint8_t> set_marker_code(const uint8_t value)
{
	return {0xc6,
	        0x05,
	        MarkerAddr & 0xff,
	        (MarkerAddr >> 8) & 0xff,
	        (MarkerAddr >> 16) & 0xff,
	        (MarkerAddr >> 24) & 0xff,
	        value};
}

// push eax; mov ax, selector; mov es, ax; pop eax; iret
std::vector<uint8_t> switch_es_code(const uint16_t selector)
{
	return {0x50,
	        0x66,
	        0xb8,
	        static_cast<uint8_t>(selector & 0xff),
	        static_cast<uint8_t>(selector >> 8),
	        0x8e,
	        0xc0,
	        0x58,
	        0xcf};
}

// mov ebx, reps
// top: mov esi, SourceBase; mov edi, di; mov ecx, count; <rep_op>
// dec ebx; jnz top
std::vector<uint8_t> rep_loop_code(const std::vector<uint8_t>& rep_op,
                                   const uint32_t di, const uint32_t count,
                                   const uint32_t reps)
{
	std::vector<uint8_t> code = {};
	const auto mov_imm32 = [&](const uint8_t opcode, const uint32_t value) {
		code.push_back(opcode);
		for (auto shift = 0; shift < 32; shift += 8) {
			code.push_back((value >> shift) & 0xff);
		}
	};
	mov_imm32(0xbb, reps);
	const auto top = code.size();
	mov_imm32(0xbe, SourceBase);
	mov_imm32(0xbf, di);
	mov_imm32(0xb9, count);
	code.insert(code.end(), rep_op.begin(), rep_op.end());
	code.push_back(0x4b); // dec ebx
	code.push_back(0x75); // jnz top
	code.push_back(static_cast<uint8_t>(top - (code.size() + 1)));
	return code;
}

void prefill(const PhysPt start, const PhysPt end)
{
	for (auto addr = start; addr < end; addr += 4) {
		mem_writed(addr, Untouched);
	}
}

class SegmentLimitCoreTest : public DOSBoxTestFixture,
                             public testing::WithParamInterface<CpuConfig> {
public:
	void SetUp() override
	{
		const auto& cfg = GetParam();
		DOSBoxTestFixture::SetUp();

		set_section_property_value("cpu", "core", cfg.config_cpu);
		set_section_property_value("cpu", "cputype", cfg.config_cpu_type);
		CPU_Init();

		setup_descriptor_tables();
		setup_interrupt_handlers();
		enter_protected_mode();

		reset_limit_window();
		reg_eax = Fill;
	}

	// Sentinel-fills the memory around the 64 KB limit, dword aligned
	static void reset_limit_window()
	{
		prefill(EsBase + 0xff00, EsBase + 0x10100);
		mem_writed(EsBase + 0x1007c, Untouched);
	}

	// CPU_Init() only resets the CPU state once per process, so put the
	// real mode state back for the tests that follow
	void TearDown() override
	{
		CPU_SET_CRX(0, 0);
		CPU_JMP(false, 0, 0, 0);
		CPU_LIDT(1023, 0);
		CPU_SetFlags(FLAG_IF, FMASK_ALL);
		for (const auto seg : {ss, ds, es, fs, gs}) {
			CPU_SetSegGeneral(seg, 0);
		}
		DOSBoxTestFixture::TearDown();
	}

	// Writes the code followed by code that sets FinishedMarker to a fresh
	// slot and points CS:EIP at it. Every run in the process gets its own
	// slot: the prefetch core keeps its queue across CPU_Init, so reusing
	// an address would execute the previous test's bytes.
	void start(const std::vector<uint8_t>& code)
	{
		static uint32_t run_count = 0;
		last_start                = CodeBase + run_count * CodeSlotSize;
		++run_count;

		auto next = write_bytes(last_start, code);
		next      = write_bytes(next, set_marker_code(FinishedMarker));
		mem_writeb(next, Hlt);

		mem_writeb(MarkerAddr, NoMarker);
		reg_esp = StackTop;
		CPU_JMP(true, CodeSelector, last_start, 0);
	}

	// Keeps running the core until either the code after the instruction
	// or a handler stub has set the marker, as the cores leave a string
	// instruction part-way through when the cycle budget runs out
	void run_until_marker(const int cycles_per_run, const int max_runs) const
	{
		for (auto i = 0; i < max_runs && marker() == NoMarker; ++i) {
			CPU_Cycles = cycles_per_run;
			GetParam().runner();
		}
	}

	void run(const std::vector<uint8_t>& code)
	{
		start(code);
		run_until_marker(100000, 16);
	}

	// Runs with a tiny cycle budget until the string instruction has been
	// left part-way through, which is where an interrupt would be taken
	void run_until_interrupted(const uint32_t initial_ecx) const
	{
		for (auto i = 0;
		     i < 100 && marker() == NoMarker && reg_ecx == initial_ecx;
		     ++i) {
			CPU_Cycles = 4;
			GetParam().runner();
		}
		ASSERT_EQ(marker(), NoMarker);
		ASSERT_LT(reg_ecx, initial_ecx);
		ASSERT_EQ(reg_eip, last_start); // resumes from the instruction
	}

	uint8_t marker() const
	{
		return mem_readb(MarkerAddr);
	}

	// The fault handler stub halts without touching the stack, so the
	// frame pushed by the fault is still at SS:ESP. The instruction under
	// test starts at the given offset into the code.
	void expect_gp_fault_at_instruction_start(const uint32_t offset = 0) const
	{
		EXPECT_EQ(marker(), GpVector);
		EXPECT_EQ(mem_readd(reg_esp), 0u); // error code
		EXPECT_EQ(mem_readd(reg_esp + 4), last_start + offset);
		EXPECT_EQ(mem_readd(reg_esp + 8), CodeSelector);
	}

	void expect_no_fault() const
	{
		EXPECT_EQ(marker(), FinishedMarker);
	}

private:
	void setup_descriptor_tables() const
	{
		write_descriptor(GdtBase + 0x00, 0, 0, 0, 0);
		write_descriptor(GdtBase + CodeSelector, 0, 0xfffff, 0x9a, 0xc0);
		write_descriptor(GdtBase + FlatDataSelector, 0, 0xfffff, 0x92, 0xc0);
		write_descriptor(GdtBase + Es64KSelector, EsBase, 0xffff, 0x92, 0x00);
		write_descriptor(GdtBase + Es128KSelector, EsBase, 0x1ffff, 0x92, 0x00);
		CPU_LGDT(Es128KSelector + 7, GdtBase);
		CPU_LIDT(256 * 8 - 1, IdtBase);
	}

	// Every vector gets its own stub that records the vector number and
	// halts, so an unexpected exception shows up as a wrong marker value.
	// Two vectors instead get a handler that reloads ES and returns.
	void setup_interrupt_handlers() const
	{
		for (uint32_t vector = 0; vector < 256; ++vector) {
			const auto stub = HandlersBase + vector * HandlerSlotSize;
			const auto next = write_bytes(stub, set_marker_code(vector));
			mem_writeb(next, Hlt);
			write_interrupt_gate(IdtBase + vector * 8, CodeSelector, stub);
		}
		write_bytes(HandlersBase + SwitchEsTo64KVector * HandlerSlotSize,
		            switch_es_code(Es64KSelector));
		write_bytes(HandlersBase + SwitchEsTo128KVector * HandlerSlotSize,
		            switch_es_code(Es128KSelector));
	}

	void enter_protected_mode() const
	{
		CPU_SET_CRX(0, cpu.cr0 | CR0_PROTECTION);
		CPU_SetFlags(0, FMASK_ALL); // forward direction, interrupts off
		CPU_SetSegGeneral(ss, FlatDataSelector);
		CPU_SetSegGeneral(ds, FlatDataSelector);
		CPU_SetSegGeneral(es, Es64KSelector);
	}

	uint32_t last_start = 0;
};

TEST_P(SegmentLimitCoreTest, RepStosdExecutesIterationsThatFitThenFaults)
{
	reg_edi = 0xff00;
	reg_ecx = 200;
	run({0xf3, 0xab}); // rep stosd

	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_ecx, 200u - 64u);
	EXPECT_EQ(reg_edi, 0x10000u);
	for (auto offset = 0xff00u; offset < 0x10000u; offset += 4) {
		EXPECT_EQ(mem_readd(EsBase + offset), Fill) << "offset " << offset;
	}
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

TEST_P(SegmentLimitCoreTest, RepStosdFittingExactlyDoesNotFault)
{
	reg_edi = 0xff00;
	reg_ecx = 64;
	run({0xf3, 0xab}); // rep stosd

	expect_no_fault();
	EXPECT_EQ(reg_ecx, 0u);
	EXPECT_EQ(reg_edi, 0x10000u);
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Fill);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

// What Viper (1998) does to switch VESA banks: start past the limit
TEST_P(SegmentLimitCoreTest, RepMovsdStartingPastLimitFaultsBeforeFirstWrite)
{
	reg_esi = SourceBase;
	reg_edi = 0x1007c;
	reg_ecx = 200;
	run({0xf3, 0xa5}); // rep movsd

	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_ecx, 200u);
	EXPECT_EQ(reg_esi, SourceBase);
	EXPECT_EQ(reg_edi, 0x1007cu);
	EXPECT_EQ(mem_readd(EsBase + 0x1007c), Untouched);
}

TEST_P(SegmentLimitCoreTest, RepMovsdCrossingLimitUpdatesAllIndexRegisters)
{
	for (auto i = 0u; i < 200; ++i) {
		mem_writed(SourceBase + i * 4, 0xa0000000 + i);
	}
	reg_esi = SourceBase;
	reg_edi = 0xff00;
	reg_ecx = 200;
	run({0xf3, 0xa5}); // rep movsd

	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_ecx, 200u - 64u);
	EXPECT_EQ(reg_esi, SourceBase + 64 * 4);
	EXPECT_EQ(reg_edi, 0x10000u);
	for (auto i = 0u; i < 64; ++i) {
		EXPECT_EQ(mem_readd(EsBase + 0xff00 + i * 4), 0xa0000000 + i);
	}
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

TEST_P(SegmentLimitCoreTest, WholeOperandMustFitWithinLimit)
{
	// stosw: a word at limit - 1 fits, a word at the limit straddles it
	reg_edi = 0xfffe;
	run({0x66, 0xab});
	expect_no_fault();
	EXPECT_EQ(mem_readw(EsBase + 0xfffe), Fill & 0xffff);
	EXPECT_EQ(reg_edi, 0x10000u);

	reset_limit_window();
	reg_edi = 0xffff;
	run({0x66, 0xab});
	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Untouched);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
	EXPECT_EQ(reg_edi, 0xffffu);

	// stosb: a byte at the limit fits, a byte past it faults
	reset_limit_window();
	reg_edi = 0xffff;
	run({0xaa});
	expect_no_fault();
	EXPECT_EQ(mem_readb(EsBase + 0xffff), Fill & 0xff);
	EXPECT_EQ(reg_edi, 0x10000u);

	reset_limit_window();
	reg_edi = 0x10000;
	run({0xaa});
	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
	EXPECT_EQ(reg_edi, 0x10000u);

	// stosd: a dword at limit - 3 fits, a dword at limit - 2 straddles it
	reset_limit_window();
	reg_edi = 0xfffc;
	run({0xab});
	expect_no_fault();
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Fill);
	EXPECT_EQ(reg_edi, 0x10000u);

	reset_limit_window();
	reg_edi = 0xfffd;
	run({0xab});
	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Untouched);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
	EXPECT_EQ(reg_edi, 0xfffdu);
}

TEST_P(SegmentLimitCoreTest, BackwardRepStosdFaultsWhenIndexWrapsBelowZero)
{
	prefill(EsBase, EsBase + 0x10);
	CPU_SetFlags(FLAG_DF, FMASK_ALL);
	reg_edi = 8;
	reg_ecx = 4;
	run({0xf3, 0xab}); // rep stosd

	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_ecx, 1u);
	EXPECT_EQ(reg_edi, 0xfffffffcu);
	EXPECT_EQ(mem_readd(EsBase + 8), Fill);
	EXPECT_EQ(mem_readd(EsBase + 4), Fill);
	EXPECT_EQ(mem_readd(EsBase + 0), Fill);
}

TEST_P(SegmentLimitCoreTest, SixteenBitAddressSizeWrapsIndexWithinSegment)
{
	CPU_SetSegGeneral(es, Es128KSelector);
	prefill(EsBase, EsBase + 0x10);
	reg_edi = 0x1234fff0;
	reg_ecx = 0x56780008;
	run({0x67, 0xf3, 0xab}); // rep stosd with 16-bit address size

	expect_no_fault();
	EXPECT_EQ(reg_edi, 0x12340010u);
	EXPECT_EQ(reg_ecx, 0x56780000u);
	for (const auto offset :
	     {0xfff0u, 0xfff4u, 0xfff8u, 0xfffcu, 0u, 4u, 8u, 12u}) {
		EXPECT_EQ(mem_readd(EsBase + offset), Fill) << "offset " << offset;
	}
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

TEST_P(SegmentLimitCoreTest, SixteenBitAddressSizeOperandDoesNotWrap)
{
	// With a 128 KB limit the dword at DI=0xfffe extends past 64 KB
	// without faulting and without wrapping to offset 0
	CPU_SetSegGeneral(es, Es128KSelector);
	prefill(EsBase, EsBase + 0x10);
	reg_edi = 0xfffe;
	run({0x67, 0xab}); // stosd with 16-bit address size

	expect_no_fault();
	EXPECT_EQ(reg_edi, 0x0002u);
	EXPECT_EQ(mem_readd(EsBase + 0xfffe), Fill);
	EXPECT_EQ(mem_readd(EsBase + 0), Untouched);

	// With a 64 KB limit the same dword straddles the limit
	CPU_SetSegGeneral(es, Es64KSelector);
	mem_writed(EsBase + 0xfffe, Untouched);
	reg_edi = 0xfffe;
	run({0x67, 0xab});

	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_edi, 0xfffeu);
	EXPECT_EQ(mem_readd(EsBase + 0xfffe), Untouched);
}

// REP with a zero count does nothing, so it doesn't fault past the limit
TEST_P(SegmentLimitCoreTest, RepWithZeroCountPastLimitDoesNotFault)
{
	reg_edi = 0x10000;
	reg_ecx = 0;
	run({0xf3, 0xab}); // rep stosd

	expect_no_fault();
	EXPECT_EQ(reg_edi, 0x10000u);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);

	// With a 16-bit address size only CX counts
	reg_edi = 0xfffe;
	reg_ecx = 0x12340000;
	run({0x67, 0xf3, 0xab});

	expect_no_fault();
	EXPECT_EQ(reg_edi, 0xfffeu);
	EXPECT_EQ(reg_ecx, 0x12340000u);
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Untouched);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

// Every core leaves the REP part-way through when the cycle budget runs
// out and resumes it later; the limit still applies to the remainder
TEST_P(SegmentLimitCoreTest, RepStosdRestartedByCycleBudgetStillFaultsAtLimit)
{
	reg_edi = 0xff00;
	reg_ecx = 200;
	start({0xf3, 0xab}); // rep stosd
	run_until_marker(4, 1000);

	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_ecx, 200u - 64u);
	EXPECT_EQ(reg_edi, 0x10000u);
	for (auto offset = 0xff00u; offset < 0x10000u; offset += 4) {
		EXPECT_EQ(mem_readd(EsBase + offset), Fill) << "offset " << offset;
	}
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

// An interrupt handler that reloads ES in the middle of a REP changes the
// limit for the remaining iterations, in both directions
TEST_P(SegmentLimitCoreTest, InterruptHandlerShrinkingEsLimitMidRepCausesFault)
{
	CPU_SetSegGeneral(es, Es128KSelector); // 200 dwords from 0xff00 fit
	reg_edi = 0xff00;
	reg_ecx = 200;
	start({0xf3, 0xab}); // rep stosd
	ASSERT_NO_FATAL_FAILURE(run_until_interrupted(200));
	ASSERT_GT(reg_ecx, 200u - 64u); // not at the 64 KB boundary yet

	CPU_HW_Interrupt(SwitchEsTo64KVector);
	run_until_marker(100000, 16);

	EXPECT_EQ(SegValue(es), Es64KSelector);
	expect_gp_fault_at_instruction_start();
	EXPECT_EQ(reg_ecx, 200u - 64u);
	EXPECT_EQ(reg_edi, 0x10000u);
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Fill);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Untouched);
}

TEST_P(SegmentLimitCoreTest, InterruptHandlerGrowingEsLimitMidRepAvoidsFault)
{
	prefill(EsBase + 0x10100, EsBase + 0x10300);
	reg_edi = 0xff00;
	reg_ecx = 200;
	start({0xf3, 0xab}); // rep stosd
	ASSERT_NO_FATAL_FAILURE(run_until_interrupted(200));
	ASSERT_GT(reg_ecx, 200u - 64u);

	CPU_HW_Interrupt(SwitchEsTo128KVector);
	run_until_marker(100000, 16);

	EXPECT_EQ(SegValue(es), Es128KSelector);
	expect_no_fault();
	EXPECT_EQ(reg_ecx, 0u);
	EXPECT_EQ(reg_edi, 0xff00u + 200 * 4);
	EXPECT_EQ(mem_readd(EsBase + 0x10000), Fill);
	EXPECT_EQ(mem_readd(EsBase + 0x1021c), Fill);
	EXPECT_EQ(mem_readd(EsBase + 0x10220), Untouched);
}

// A memory-mapped device whose access delay isn't capped by the cycle budget,
// like video memory with 'vmem_delay' enabled
class DelayedDevicePageHandler final : public PageHandler {
public:
	static constexpr int DelayCycles = 1000;

	DelayedDevicePageHandler()
	{
		flags = PFLAG_NOCODE;
	}

	void writed(PhysPt, uint32_t) override
	{
		CPU_Cycles -= DelayCycles;
	}
};

// Device access delays can leave no cycles in the middle of a dynamic core
// block. A string instruction later in the block that only partly fits must
// still stop at the limit; the dynrec string helpers execute every
// iteration when no cycles are left.
TEST_P(SegmentLimitCoreTest, RepStosdAfterDeviceDelayUsedUpCyclesFaultsAtLimit)
{
	constexpr PhysPt DeviceAddr = 0x40000;
	constexpr auto DevicePage   = DeviceAddr / 4096;

	DelayedDevicePageHandler device = {};
	MEM_SetPageHandler(DevicePage, 1, &device);
	PAGING_ClearTLB();

	prefill(EsBase + 0x10100, EsBase + 0x10300);
	reg_edi = 0xff00;
	reg_ecx = 200;
	// mov [DeviceAddr], eax; rep stosd
	constexpr uint32_t RepStosdOffset = 5;
	start({0xa3,
	       DeviceAddr & 0xff,
	       (DeviceAddr >> 8) & 0xff,
	       (DeviceAddr >> 16) & 0xff,
	       (DeviceAddr >> 24) & 0xff,
	       0xf3,
	       0xab});
	run_until_marker(100, 1000);

	MEM_ResetPageHandler(DevicePage, 1);
	PAGING_ClearTLB();

	expect_gp_fault_at_instruction_start(RepStosdOffset);
	EXPECT_EQ(reg_ecx, 200u - 64u);
	EXPECT_EQ(reg_edi, 0x10000u);
	EXPECT_EQ(mem_readd(EsBase + 0xfffc), Fill);
	for (auto offset = 0x10000u; offset < 0x10300u; offset += 4) {
		EXPECT_EQ(mem_readd(EsBase + offset), Untouched)
		        << "offset " << offset;
	}
}

// Manual throughput benchmark for the cost of the limit checks, so not run
// by default. Run a release build with:
//   --gtest_also_run_disabled_tests --gtest_filter='*StringWriteThroughput*'
TEST_P(SegmentLimitCoreTest, DISABLED_StringWriteThroughput)
{
	constexpr uint32_t ElementsPerRep = 4096; // 16 KB per REP
	constexpr uint32_t Reps           = 32768;
	constexpr auto Elements = static_cast<double>(ElementsPerRep) * Reps;
	constexpr auto Rounds   = 3;

	struct Scenario {
		const char* op_name;
		std::vector<uint8_t> rep_op;
		const char* es_name;
		uint16_t es_selector;
		uint32_t di;
	};
	const std::vector<Scenario> scenarios = {
	        {"rep movsd", {0xf3, 0xa5}, "flat 4 GB ES", FlatDataSelector, EsBase},
	        {"rep movsd", {0xf3, 0xa5},     "64 KB ES",    Es64KSelector,      0},
	        {"rep stosd", {0xf3, 0xab}, "flat 4 GB ES", FlatDataSelector, EsBase},
	        {"rep stosd", {0xf3, 0xab},     "64 KB ES",    Es64KSelector,      0},
	};

	for (const auto& scenario : scenarios) {
		CPU_SetSegGeneral(es, scenario.es_selector);
		const auto code = rep_loop_code(scenario.rep_op,
		                                scenario.di,
		                                ElementsPerRep,
		                                Reps);

		auto best_ns = std::numeric_limits<double>::max();
		for (auto round = 0; round < Rounds; ++round) {
			start(code);
			const auto begin = std::chrono::steady_clock::now();
			run_until_marker(1 << 30, 1000);
			const auto end = std::chrono::steady_clock::now();
			ASSERT_EQ(marker(), FinishedMarker);

			const auto ns = std::chrono::duration<double, std::nano>(
			        end - begin);
			best_ns = std::min(best_ns, ns.count());
		}
		LOG_MSG("%s, %s, %s: %.2f ns per element, %.0f MB/s",
		        GetParam().test_name.c_str(),
		        scenario.op_name,
		        scenario.es_name,
		        best_ns / Elements,
		        4.0 * Elements / best_ns * 1000.0);
	}
}

INSTANTIATE_TEST_SUITE_P(CpuVariations, SegmentLimitCoreTest, AllCpuConfigs);

} // namespace
