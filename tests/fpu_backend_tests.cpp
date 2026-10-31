// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dosbox.h"

#if !C_FPU_X86
#include "cpu/cpu.h"
#include "cpu_config_param.h"
#include "dosbox_test_fixture.h"
#include "fpu/fpu.h"
#include "hardware/memory.h"
#include <tuple>

namespace {
class FpuBackendConfig : public DOSBoxTestFixture {
protected:
	void TearDown() override
	{
		DOSBoxTestFixture::TearDown();
		FPU_SelectBackend(false);
		fpu_operations.FPU_FINIT();
	}
};
class FpuFastCompare : public DOSBoxTestFixture {
protected:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();
		FPU_SelectBackend(false);
		fpu_operations.FPU_FINIT();
	}
	void TearDown() override
	{
		FPU_SelectBackend(false);
		fpu_operations.FPU_FINIT();
		DOSBoxTestFixture::TearDown();
	}
	void set(const Bitu reg, const uint64_t bits, const FPU_Tag tag)
	{
		fpu.regs[reg].set_bits(bits);
		fpu.tags[reg] = tag;
	}
};
TEST_F(FpuFastCompare, FcomSignalsQuietNanButFucomDoesNot)
{
	set(0, UINT64_C(0x7ff8000000000001), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4501);

	fpu_operations.FPU_FINIT();
	set(0, UINT64_C(0x7ff8000000000001), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FUCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4500);
}
TEST_F(FpuFastCompare, FucomSignalsSignalingNanAndUnmaskedInvalidLeavesFlags)
{
	set(0, UINT64_C(0x7ff0000000000001), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FUCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4501);

	fpu_operations.FPU_FINIT();
	FPU_SetCW(0x37e);
	set(0, UINT64_C(0x7ff0000000000001), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FUCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 1);
}
TEST_F(FpuFastCompare, WeirdTaggedInfinityStillComparesNumerically)
{
	set(0, UINT64_C(0x7ff0000000000000), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0);
}
TEST_F(FpuFastCompare, DenormalCompareHonorsTheControlWord)
{
	set(0, UINT64_C(1), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4702, 0x0102); // DE, and ST(0) < SRC

	fpu_operations.FPU_FINIT();
	FPU_SetCW(0x37d); // Unmask denormal-operand exception
	set(0, UINT64_C(1), TAG_Weird);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4702, 0x0002); // DE; condition codes unchanged
}
TEST_F(FpuFastCompare, EmptyOperandRaisesMaskedStackFault)
{
	set(0, UINT64_C(0x3ff0000000000000), TAG_Empty);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4541, 0x4541);
}
TEST_F(FpuFastCompare, Load80PreservesSignalingNanForFucom)
{
	mem_writeq(0x9000, UINT64_C(0x8000000000000001));
	mem_writew(0x9008, 0x7fff);
	fpu_operations.FPU_FLD_F80(0x9000);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FUCOM(TOP, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4501);
}
TEST_F(FpuFastCompare, Load80UnsupportedEncodingSignalsFucom)
{
	mem_writeq(0x9000, UINT64_C(0x4000000000000000)); // Missing integer bit
	mem_writew(0x9008, 0x4000);
	fpu_operations.FPU_FLD_F80(0x9000);
	set(1, UINT64_C(0x3ff0000000000000), TAG_Valid);
	fpu_operations.FPU_FUCOM(TOP, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4501);
}
TEST_F(FpuBackendConfig, FastIsTheStartupDefault)
{
	auto section  = get_section("cpu");
	auto property = section->GetProperty("fpu");
	ASSERT_NE(property, nullptr);
	EXPECT_EQ(property->GetDefaultValue().ToString(), "fast");
	EXPECT_EQ(property->GetChange(), Property::Changeable::OnlyAtStart);
	EXPECT_EQ(section->GetString("fpu"), "fast");
	FPU_Init();
	EXPECT_FALSE(FPU_IsAccurate());
}
TEST_F(FpuBackendConfig, QueuedChangeDoesNotSwitchTheRunningBackend)
{
	auto section = get_section("cpu");
	FPU_Init();
	const auto previous_add = fpu_operations.FPU_FADD;
	section->GetProperty("fpu")->SetQueuedValue("accurate");
	EXPECT_EQ(section->GetString("fpu"), "fast");
	EXPECT_EQ(section->GetProperty("fpu")->GetQueuedValue(),
	          std::optional<std::string>("accurate"));
	EXPECT_FALSE(FPU_IsAccurate());
	EXPECT_EQ(fpu_operations.FPU_FADD, previous_add);
}
TEST_F(FpuBackendConfig, CpuSettingUpdatesPreserveTheSelectedBackend)
{
	auto section = get_section("cpu");
	ASSERT_TRUE(section->HandleInputLine("fpu=accurate"));
	FPU_Init();
	ASSERT_TRUE(FPU_IsAccurate());
	const auto previous_add = fpu_operations.FPU_FADD;
	set_section_property_value("cpu", "cpu_cycles", "4000");
	section->ExecuteUpdate(*section->GetProperty("cpu_cycles"));
	EXPECT_TRUE(FPU_IsAccurate());
	EXPECT_EQ(fpu_operations.FPU_FADD, previous_add);
}

using BackendAndCore = std::tuple<std::string, CpuConfig>;
class FpuBackendProgram : public DOSBoxTestFixture,
                          public testing::WithParamInterface<BackendAndCore> {
protected:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();
		const auto& [backend, core] = GetParam();
		set_section_property_value("cpu", "core", core.config_cpu);
		set_section_property_value("cpu", "cputype", core.config_cpu_type);
		CPU_Init();
		ASSERT_TRUE(get_section("cpu")->HandleInputLine("fpu=" + backend));
		FPU_Init();
		old_big      = cpu.code.big;
		old_cs       = SegValue(SegNames::cs);
		old_ds       = SegValue(SegNames::ds);
		cpu.code.big = false;
		CPU_SetSegGeneral(SegNames::cs, 0);
		CPU_SetSegGeneral(SegNames::ds, 0);
		reg_eip = 0x100;
	}
	void TearDown() override
	{
		cpu.code.big = old_big;
		CPU_SetSegGeneral(SegNames::cs, old_cs);
		CPU_SetSegGeneral(SegNames::ds, old_ds);
		DOSBoxTestFixture::TearDown();
		FPU_SelectBackend(false);
		fpu_operations.FPU_FINIT();
	}
	bool old_big    = false;
	uint16_t old_cs = 0;
	uint16_t old_ds = 0;
};
TEST_P(FpuBackendProgram, ExecutesTheConfiguredPrecision)
{
	// Add one to 2^53. The fast backend rounds back to 2^53; the accurate
	// backend retains the additional integer bit through arithmetic and FISTP.
	constexpr uint64_t input = UINT64_C(1) << 53;
	mem_writeq(0x9000, input);
	mem_writeq(0x9010, 0);
	constexpr uint8_t code[] = {
	        0xdb,
	        0xe3, // FNINIT
	        0xdf,
	        0x2e,
	        0x00,
	        0x90, // FILD qword [9000h]
	        0xd9,
	        0xe8, // FLD1
	        0xde,
	        0xc1, // FADDP ST(1), ST(0)
	        0xdf,
	        0x3e,
	        0x10,
	        0x90, // FISTP qword [9010h]
	        0xeb,
	        0xfe // JMP $ (end of block)
	};
	for (size_t i = 0; i < sizeof(code); ++i) {
		mem_writeb(0x100 + i, code[i]);
	}
	const auto& [backend, core] = GetParam();
	EXPECT_EQ(FPU_IsAccurate(), backend == "accurate");
	CPU_Cycles = 20;
	core.runner();
	EXPECT_EQ(mem_readq(0x9010), input + (backend == "accurate"));
}
INSTANTIATE_TEST_SUITE_P(PortableFpu, FpuBackendProgram,
                         testing::Combine(testing::Values(std::string("fast"),
                                                          std::string("accurate")),
                                          AllCpuConfigs),
                         [](const testing::TestParamInfo<BackendAndCore>& info) {
	                         return std::get<0>(info.param) +
				        std::get<1>(info.param).test_name;
                         });
} // namespace
#endif
