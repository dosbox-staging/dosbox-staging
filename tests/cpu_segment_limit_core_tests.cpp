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

#include <gtest/gtest.h>

#include <vector>

#include "config/config.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "cpu_config_param.h"
#include "dosbox_test_fixture.h"
#include "memory.h"

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

constexpr uint16_t CodeSelector     = 0x08;
constexpr uint16_t FlatDataSelector = 0x10;
constexpr uint16_t Es64KSelector    = 0x18; // base EsBase, limit 0xffff
constexpr uint16_t Es128KSelector   = 0x20; // base EsBase, limit 0x1ffff

// Written to MarkerAddr by the fault handler stubs (vector number) or by the
// code following the instruction under test
constexpr uint8_t NoMarker       = 0xff;
constexpr uint8_t FinishedMarker = 0xfe;
constexpr uint8_t GpVector       = 13;

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
		if (cfg.test_name == "Full") {
			GTEST_SKIP()
			        << "The full core has its own string "
			           "implementation without segment limit checks";
		}
		DOSBoxTestFixture::SetUp();

		set_section_property_value("cpu", "core", cfg.config_cpu);
		set_section_property_value("cpu", "cputype", cfg.config_cpu_type);
		CPU_Init();

		setup_descriptor_tables();
		setup_fault_handlers();
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

	// Executes the instruction followed by code that sets FinishedMarker.
	// Keeps running the core until either that code or a fault handler stub
	// has set the marker, as the dynamic cores may leave the core to
	// restart a partially executed string instruction.
	//
	// Every run in the process gets its own code slot: the prefetch core
	// keeps its queue across CPU_Init, so reusing an address would execute
	// the previous test's bytes.
	void run(const std::vector<uint8_t>& code)
	{
		static uint32_t run_count = 0;
		const auto start          = CodeBase + run_count * 32;
		++run_count;

		auto next = write_bytes(start, code);
		next      = write_bytes(next, set_marker_code(FinishedMarker));
		mem_writeb(next, Hlt);

		mem_writeb(MarkerAddr, NoMarker);
		reg_esp = StackTop;
		CPU_JMP(true, CodeSelector, start, 0);

		for (auto i = 0; i < 16 && marker() == NoMarker; ++i) {
			CPU_Cycles = 100000;
			GetParam().runner();
		}
		last_start = start;
	}

	uint8_t marker() const
	{
		return mem_readb(MarkerAddr);
	}

	// The fault handler stub halts without touching the stack, so the
	// frame pushed by the fault is still at SS:ESP
	void expect_gp_fault_at_instruction_start() const
	{
		EXPECT_EQ(marker(), GpVector);
		EXPECT_EQ(mem_readd(reg_esp), 0u); // error code
		EXPECT_EQ(mem_readd(reg_esp + 4), last_start);
		EXPECT_EQ(mem_readd(reg_esp + 8), CodeSelector);
	}

	void expect_no_fault() const
	{
		EXPECT_EQ(marker(), FinishedMarker);
	}

private:
	void setup_descriptor_tables()
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
	// halts, so an unexpected exception shows up as a wrong marker value
	void setup_fault_handlers()
	{
		for (uint32_t vector = 0; vector < 256; ++vector) {
			const auto stub = HandlersBase + vector * 8;
			const auto next = write_bytes(stub, set_marker_code(vector));
			mem_writeb(next, Hlt);
			write_interrupt_gate(IdtBase + vector * 8, CodeSelector, stub);
		}
	}

	void enter_protected_mode()
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

INSTANTIATE_TEST_SUITE_P(CpuVariations, SegmentLimitCoreTest, AllCpuConfigs);

} // namespace
