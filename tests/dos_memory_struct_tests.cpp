// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos_inc.h"

#include <gtest/gtest.h>

#include "dosbox_test_fixture.h"

namespace {

class DOS_MemoryStructTest : public DOSBoxTestFixture {};

uint16_t assert_AllocateMemory(const uint16_t blocks = 1)
{
	uint16_t segment    = 0;
	uint16_t tmp_blocks = blocks;

	EXPECT_TRUE(DOS_AllocateMemory(&segment, &tmp_blocks));
	EXPECT_EQ(tmp_blocks, blocks);
	EXPECT_NE(segment, 0);

	return segment;
}

void assert_FreeMemory(const uint16_t segment)
{
	EXPECT_TRUE(DOS_FreeMemory(segment));	
}

constexpr uint32_t CanaryValue = 0xdeadbeef;
constexpr uint8_t  CanarySize  = sizeof(CanaryValue);

TEST_F(DOS_MemoryStructTest, byte)
{
#ifdef _MSC_VER
#pragma pack(1)
#endif

	struct TestStruct {
		uint32_t canary_1 = 0;

		uint8_t test_byte = 0;

		uint32_t canary_2 = 0;
	} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

	constexpr uint8_t TestValue1    = 0xab;
	constexpr uint8_t TestValue2    = 0xcd;
	constexpr uint8_t TestValueSize = sizeof(TestValue1);

	// Allocate memory for test
	const auto segment = assert_AllocateMemory();
	// Exact name 'pt' is needed for SSET_* and SGET_* macros
	const auto pt = PhysicalMake(segment, 0);

	// Calculate data and canary location
	const auto canary_1_location  = pt;
	const auto test_data_location = canary_1_location + CanarySize;
	const auto canary_2_location  = test_data_location + TestValueSize;

	// Write canaries
	mem_writed(canary_1_location, CanaryValue);
	mem_writed(canary_2_location, CanaryValue);

	// Test data set/get - 1st case
	SSET_BYTE(TestStruct, test_byte, TestValue1);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_BYTE(TestStruct, test_byte));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readb(test_data_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Test data set/get - 2nd case
	SSET_BYTE(TestStruct, test_byte, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue2, SGET_BYTE(TestStruct, test_byte));
	// Check memory content directly
	EXPECT_EQ(TestValue2, mem_readb(test_data_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Free memory
	assert_FreeMemory(segment);
}

TEST_F(DOS_MemoryStructTest, word)
{
#ifdef _MSC_VER
#pragma pack(1)
#endif

	struct TestStruct {
		uint32_t canary_1 = 0;

		uint16_t test_word = 0;

		uint32_t canary_2 = 0;
	} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

	constexpr uint16_t TestValue1   = 0x1234;
	constexpr uint16_t TestValue2   = 0x5678;
	constexpr uint8_t TestValueSize = sizeof(TestValue1);

	// Allocate memory for test
	const auto segment = assert_AllocateMemory();
	// Exact name 'pt' is needed for SSET_* and SGET_* macros
	const auto pt = PhysicalMake(segment, 0);

	// Calculate data and canary location
	const auto canary_1_location  = pt;
	const auto test_data_location = canary_1_location + CanarySize;
	const auto canary_2_location  = test_data_location + TestValueSize;

	// Write canaries
	mem_writed(canary_1_location, CanaryValue);
	mem_writed(canary_2_location, CanaryValue);

	// Set test data - 1st case
	SSET_WORD(TestStruct, test_word, TestValue1);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_WORD(TestStruct, test_word));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readw(test_data_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Set test data - 2nd case
	SSET_WORD(TestStruct, test_word, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue2, SGET_WORD(TestStruct, test_word));
	// Check memory content directly
	EXPECT_EQ(TestValue2, mem_readw(test_data_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Free memory
	assert_FreeMemory(segment);
}

TEST_F(DOS_MemoryStructTest, double_word)
{
#ifdef _MSC_VER
#pragma pack(1)
#endif

	struct TestStruct {
		uint32_t canary_1 = 0;

		uint32_t test_dword = 0;

		uint32_t canary_2 = 0;
	} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

	constexpr uint32_t TestValue1   = 0xb001b001;
	constexpr uint32_t TestValue2   = 0xabcd1234;
	constexpr uint8_t TestValueSize = sizeof(TestValue1);

	// Allocate memory for test
	const auto segment = assert_AllocateMemory();
	// Exact name 'pt' is needed for SSET_* and SGET_* macros
	const auto pt = PhysicalMake(segment, 0);

	// Calculate data and canary location
	const auto canary_1_location  = pt;
	const auto test_data_location = canary_1_location + CanarySize;
	const auto canary_2_location  = test_data_location + TestValueSize;

	// Write canaries
	mem_writed(canary_1_location, CanaryValue);
	mem_writed(canary_2_location, CanaryValue);

	// Set test data - 1st case
	SSET_DWORD(TestStruct, test_dword, TestValue1);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_DWORD(TestStruct, test_dword));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readd(test_data_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Set test data - 2nd case
	SSET_DWORD(TestStruct, test_dword, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue2, SGET_DWORD(TestStruct, test_dword));
	// Check memory content directly
	EXPECT_EQ(TestValue2, mem_readd(test_data_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Free memory
	assert_FreeMemory(segment);
}

TEST_F(DOS_MemoryStructTest, byte_array)
{
#ifdef _MSC_VER
#pragma pack(1)
#endif

	struct TestStruct {
		uint32_t canary_1 = 0;

		uint8_t test_bytes[2] = { 0 };

		uint32_t canary_2 = 0;
	} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

	constexpr uint8_t TestValue1    = 0x12;
	constexpr uint8_t TestValue2    = 0x34;
	constexpr uint8_t TestValueSize = sizeof(TestValue1);

	// Allocate memory for test
	const auto segment = assert_AllocateMemory();
	// Exact name 'pt' is needed for SSET_* and SGET_* macros
	const auto pt = PhysicalMake(segment, 0);

	// Calculate data and canary location
	const auto canary_1_location    = pt;
	const auto test_data_0_location = canary_1_location + CanarySize;
	const auto test_data_1_location = test_data_0_location + TestValueSize;	
	const auto canary_2_location    = test_data_1_location + TestValueSize;

	// Write canaries
	mem_writed(canary_1_location, CanaryValue);
	mem_writed(canary_2_location, CanaryValue);

	// Set test data - 1st case
	SSET_BYTE_ARRAY(TestStruct, test_bytes, 0, TestValue1);
	SSET_BYTE_ARRAY(TestStruct, test_bytes, 1, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_BYTE_ARRAY(TestStruct, test_bytes, 0));
	EXPECT_EQ(TestValue2, SGET_BYTE_ARRAY(TestStruct, test_bytes, 1));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readb(test_data_0_location));
	EXPECT_EQ(TestValue2, mem_readb(test_data_1_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Set test data - 2nd case
	SSET_BYTE_ARRAY(TestStruct, test_bytes, 1, TestValue1);
	SSET_BYTE_ARRAY(TestStruct, test_bytes, 0, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_BYTE_ARRAY(TestStruct, test_bytes, 1));
	EXPECT_EQ(TestValue2, SGET_BYTE_ARRAY(TestStruct, test_bytes, 0));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readb(test_data_1_location));
	EXPECT_EQ(TestValue2, mem_readb(test_data_0_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Free memory
	assert_FreeMemory(segment);
}

TEST_F(DOS_MemoryStructTest, word_array)
{
#ifdef _MSC_VER
#pragma pack(1)
#endif

	struct TestStruct {
		uint32_t canary_1 = 0;

		uint16_t test_words[2] = { 0 };

		uint32_t canary_2 = 0;
	} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

	constexpr uint16_t TestValue1   = 0xc001;
	constexpr uint16_t TestValue2   = 0xdeaf;
	constexpr uint8_t TestValueSize = sizeof(TestValue1);

	// Allocate memory for test
	const auto segment = assert_AllocateMemory();
	// Exact name 'pt' is needed for SSET_* and SGET_* macros
	const auto pt = PhysicalMake(segment, 0);

	// Calculate data and canary location
	const auto canary_1_location    = pt;
	const auto test_data_0_location = canary_1_location + CanarySize;
	const auto test_data_1_location = test_data_0_location + TestValueSize;	
	const auto canary_2_location    = test_data_1_location + TestValueSize;

	// Write canaries
	mem_writed(canary_1_location, CanaryValue);
	mem_writed(canary_2_location, CanaryValue);

	// Set test data - 1st case
	SSET_WORD_ARRAY(TestStruct, test_words, 0, TestValue1);
	SSET_WORD_ARRAY(TestStruct, test_words, 1, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_WORD_ARRAY(TestStruct, test_words, 0));
	EXPECT_EQ(TestValue2, SGET_WORD_ARRAY(TestStruct, test_words, 1));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readw(test_data_0_location));
	EXPECT_EQ(TestValue2, mem_readw(test_data_1_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Set test data - 2nd case
	SSET_WORD_ARRAY(TestStruct, test_words, 1, TestValue1);
	SSET_WORD_ARRAY(TestStruct, test_words, 0, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_WORD_ARRAY(TestStruct, test_words, 1));
	EXPECT_EQ(TestValue2, SGET_WORD_ARRAY(TestStruct, test_words, 0));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readw(test_data_1_location));
	EXPECT_EQ(TestValue2, mem_readw(test_data_0_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Free memory
	assert_FreeMemory(segment);
}

TEST_F(DOS_MemoryStructTest, double_word_array)
{
#ifdef _MSC_VER
#pragma pack(1)
#endif

	struct TestStruct {
		uint32_t canary_1 = 0;

		uint32_t test_dwords[2] = { 0 };

		uint32_t canary_2 = 0;
	} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

	constexpr uint32_t TestValue1   = 0x10face01;
	constexpr uint32_t TestValue2   = 0xc0d0e0f0;
	constexpr uint8_t TestValueSize = sizeof(TestValue1);

	// Allocate memory for test
	const auto segment = assert_AllocateMemory();
	// Exact name 'pt' is needed for SSET_* and SGET_* macros
	const auto pt = PhysicalMake(segment, 0);

	// Calculate data and canary location
	const auto canary_1_location    = pt;
	const auto test_data_0_location = canary_1_location + CanarySize;
	const auto test_data_1_location = test_data_0_location + TestValueSize;	
	const auto canary_2_location    = test_data_1_location + TestValueSize;

	// Write canaries
	mem_writed(canary_1_location, CanaryValue);
	mem_writed(canary_2_location, CanaryValue);

	// Set test data - 1st case
	SSET_DWORD_ARRAY(TestStruct, test_dwords, 0, TestValue1);
	SSET_DWORD_ARRAY(TestStruct, test_dwords, 1, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_DWORD_ARRAY(TestStruct, test_dwords, 0));
	EXPECT_EQ(TestValue2, SGET_DWORD_ARRAY(TestStruct, test_dwords, 1));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readd(test_data_0_location));
	EXPECT_EQ(TestValue2, mem_readd(test_data_1_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Set test data - 2nd case
	SSET_DWORD_ARRAY(TestStruct, test_dwords, 1, TestValue1);
	SSET_DWORD_ARRAY(TestStruct, test_dwords, 0, TestValue2);

	// Check memory content using the macro
	EXPECT_EQ(TestValue1, SGET_DWORD_ARRAY(TestStruct, test_dwords, 1));
	EXPECT_EQ(TestValue2, SGET_DWORD_ARRAY(TestStruct, test_dwords, 0));
	// Check memory content directly
	EXPECT_EQ(TestValue1, mem_readd(test_data_1_location));
	EXPECT_EQ(TestValue2, mem_readd(test_data_0_location));
	// Check the canaries
	EXPECT_EQ(CanaryValue, mem_readd(canary_1_location));
	EXPECT_EQ(CanaryValue, mem_readd(canary_2_location));

	// Free memory
	assert_FreeMemory(segment);
}

}
