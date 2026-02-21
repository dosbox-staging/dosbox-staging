#include <gtest/gtest.h>

#include "config/config.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "cpu_config_param.h"
#include "dosbox_test_fixture.h"
#include "memory.h"

namespace {

class MultiPrefixTest : public DOSBoxTestFixture,
                        public testing::WithParamInterface<CpuConfig> {
public:
	void SetUp() override
	{
		const auto& cfg = GetParam();

		DOSBoxTestFixture::SetUp();

		set_section_property_value("cpu", "core", cfg.config_cpu);
		set_section_property_value("cpu", "cputype", cfg.config_cpu_type);
		CPU_Init();

		reg_eip = 0x100;
		clear_code_mem(reg_eip);
	}

private:
	static constexpr uint32_t TestMemSize = 0x100;

	void clear_code_mem(PhysPt start_addr)
	{
		for (PhysPt addr = start_addr; addr < start_addr + TestMemSize;
		     ++addr) {
			mem_writeb(addr, 0x90); // NOP
		}
	}
};

template <typename... Bytes>
void mem_write(PhysPt addr, Bytes... bytes)
{
	(mem_writeb(addr++, static_cast<uint8_t>(bytes)), ...);
}

TEST_P(MultiPrefixTest, LockPrefixGcc)
{
	reg_eax = 0x10000;

	// gcc -m16 -march=i386 generates this for prefix-increment on an
	// _Atomic var 67 66 f0 ff 80 00 00 00 00  lock inc DWORD PTR [eax]
	mem_write(reg_eip, 0x67, 0x66, 0xf0, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00);
	mem_writed(reg_eax, 0xDEADBEEF);
	// Incorrect implementations will see this as multiple instructions
	CPU_Cycles = 2;
	GetParam().runner();

	EXPECT_EQ(mem_readd(reg_eax), 0xDEADBEEF + 1);
}

TEST_P(MultiPrefixTest, LockPrefixReordered)
{
	reg_eax = 0x10000;

	// lock inc DWORD PTR [eax] as assembled by Keystone, different prefix
	// order
	mem_write(reg_eip, 0xf0, 0x67, 0x66, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00);
	mem_writed(reg_eax, 0xDEADBEEF);

	CPU_Cycles = 2;
	GetParam().runner();

	EXPECT_EQ(mem_readd(reg_eax), 0xDEADBEEF + 1);
}

TEST_P(MultiPrefixTest, AllPrefixes)
{
	if (GetParam().test_name == "Prefetch") {
		GTEST_SKIP() << "Not supported on prefetch core (386 doesn't have CMPXCHG)";
	}

	reg_eax = 0xDEADBEEF;
	reg_ebx = 0x10000;
	reg_ecx = 0x12345678;
	CPU_SetSegGeneral(SegNames::fs, 0x1);
	PhysPt test_addr = SegPhys(SegNames::fs) + reg_ebx;

	// Valid instruction, but unlikely to exist in any real old code
	// 64 67 66 f0 0f b1 0b lock cmpxchg DWORD PTR fs:[ebx], ecx
	mem_write(reg_eip, 0x64, 0x67, 0x66, 0xf0, 0x0f, 0xb1, 0x0b);
	mem_writed(test_addr, reg_eax);

	CPU_Cycles = 2;
	GetParam().runner();

	EXPECT_EQ(reg_eax, 0xDEADBEEF);
	EXPECT_EQ(mem_readd(test_addr), reg_ecx);
}

INSTANTIATE_TEST_SUITE_P(CpuVariations, MultiPrefixTest, AllCpuConfigs);

} // namespace
