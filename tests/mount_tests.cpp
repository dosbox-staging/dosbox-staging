// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/programs/mount.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dos/drives.h"
#include "dosbox_test_fixture.h"
#include "hardware/ide.h"
#include "ints/bios_disk.h"
#include "misc/cross.h"
#include "utils/env_utils.h"

namespace {

namespace std_fs = std::filesystem;

class MountTest : public DOSBoxTestFixture {
protected:
	static std_fs::path test_file_path;

	static void write_file(const std_fs::path& path,
	                       size_t num_bytes = 1024, char fill = 0)
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		std::vector<char> buf(num_bytes, fill);
		out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
	}

	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		std_fs::create_directories(test_file_path);
		std_fs::create_directories(test_file_path / "plain_dir");
		std_fs::create_directories(test_file_path / "overlay_base");
		std_fs::create_directories(test_file_path / "overlay_layer");

		write_file(test_file_path / "plain_dir" / "readme.txt", 16, 'x');

		// Generic image-ish files. None need valid boot-sector bytes
		// or filesystem content: every test either supplies explicit
		// -chs/-size (skipping the "autosize" file-probing path
		// entirely) or uses a type where that path isn't taken.

		write_file(test_file_path / "image.iso", 2048 * 4);
		write_file(test_file_path / "image.img", 4096);
		write_file(test_file_path / "bootable.img", 65536);
		write_file(test_file_path / "raw.dat", 1440 * 1024);

		write_file(test_file_path / "disk1.img", 512);
		write_file(test_file_path / "disk02.img", 512);
		write_file(test_file_path / "disk03.img", 512);

		write_file(test_file_path / "image.cue", 2048);
		write_file(test_file_path / "image.bin", 2048);
		write_file(test_file_path / "image.mds", 2048);
		write_file(test_file_path / "image.ccd", 2048);
		write_file(test_file_path / "image.ima", 2048);
		write_file(test_file_path / "image.vhd", 2048);

		write_file(test_file_path / "image.flac", 2048);
		write_file(test_file_path / "image.opus", 2048);
		write_file(test_file_path / "image.ogg", 2048);
		write_file(test_file_path / "image.mp3", 2048);
		write_file(test_file_path / "image.wav", 2048);

		write_file(test_file_path / "noextfile", 2048);
	}

	void TearDown() override
	{
		DOSBoxTestFixture::TearDown();
	}

	// Runs once after all tests in this suite.
	static void TearDownTestSuite()
	{
		std::error_code ec;
		std_fs::remove_all(test_file_path, ec);
	}

	static std::string P(const std::string& name)
	{
		return (test_file_path / name).string();
	}

	// File names of the collected image paths, in order
	static std::vector<std::string> FileNames(const MountParameters& params)
	{
		std::vector<std::string> names = {};
		for (const auto& path : params.paths) {
			names.push_back(std_fs::path(path).filename().string());
		}
		return names;
	}

	static std::optional<MountParameters> Mount(
	        const std::string& command_params,
	        const std::string& program_path = "Z:\\MOUNT.COM")
	{
		auto cmd     = new CommandLine(program_path, command_params);
		auto program = new MOUNT();
		return program->ProcessArguments(cmd);
	}
};

// Use unique test file paths otherwise when when run tests in parallell
// chunks with the -j option (e.g. -j 16) the teardown and the setup steps of
// two chunks can overlap and cause test failures.
//
std_fs::path MountTest::test_file_path = [] {
	const auto now = duration_cast<std::chrono::nanoseconds>(
	                         std::chrono::system_clock::now().time_since_epoch())
	                         .count();

	// Anchored to this source file's location rather than the
	// process's CWD
	return std_fs::path(__FILE__).parent_path() /
	       (std::to_string(now) + "_mount_test_files");
}();

// ---------------------------------------------------------------------
// Error paths: ProcessArguments() must return std::nullopt.
// ---------------------------------------------------------------------

TEST_F(MountTest, RejectsUnknownType)
{
	const auto result = Mount("C " + P("plain_dir") + " -t bogus");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsInvalidChsFormat)
{
	// Every part must be a whole number, and all three must be present
	for (const auto* chs :
	     {"notnumbers", "200,16", "200,16,63,1", "200,16,63x", "200,,63", "200,16,0x3f"}) {
		SCOPED_TRACE(chs);

		const auto result = Mount("C " + P("bootable.img") +
		                          " -t hdd -chs " + chs);
		EXPECT_FALSE(result.has_value());
	}
}

TEST_F(MountTest, RejectsOptionValueThatIsAnotherOption)
{
	const auto result = Mount("1 " + P("bootable.img") + " -size -t hdd");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsOptionValueThatIsAFlag)
{
	const auto result = Mount("N " + P("plain_dir") + " -label -ro");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsOptionValueThatIsTheIdeFlag)
{
	const auto result = Mount("N " + P("plain_dir") + " -label -ide");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsOptionMissingValueAtEnd)
{
	const auto result = Mount("N " + P("plain_dir") + " -label");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsEveryValueOptionMissingValueAtEnd)
{
	for (const auto* option :
	     {"-t", "-fs", "-label", "-freesize", "-size", "-chs"}) {
		SCOPED_TRACE(option);

		const auto result = Mount("N " + P("plain_dir") + " " + option);
		EXPECT_FALSE(result.has_value());
	}
}

TEST_F(MountTest, RejectsOptionValueThatIsAnotherValueOption)
{
	EXPECT_FALSE(Mount("C " + P("bootable.img") + " -t -fs none").has_value());

	EXPECT_FALSE(Mount("Q " + P("plain_dir") + " -freesize -size 512,63,16,42")
	                     .has_value());
}

TEST_F(MountTest, RejectsOptionValueThatIsThePathRelativeFlag)
{
	const auto result = Mount("N " + P("plain_dir") + " -label -pr");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsMissingValueRegardlessOfCase)
{
	EXPECT_FALSE(Mount("N " + P("plain_dir") + " -LABEL -RO").has_value());
	EXPECT_FALSE(Mount("1 " + P("bootable.img") + " -Size -T hdd").has_value());
}

TEST_F(MountTest, RejectsMissingPath)
{
	const auto result = Mount("C");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsDriveTokenTooLong)
{
	const auto result = Mount("WWW " + P("plain_dir"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsSecondCharNotColon)
{
	const auto result = Mount("WQ " + P("plain_dir"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsOutOfRangeDriveNumber)
{
	// Only digits '0'-'3' are valid drive numbers.
	const auto result = Mount("4 " + P("bootable.img"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsBootableLetterOutsideAtoD)
{
	// -fs none forces the A-D -> 0-3 remap in ParseDrive; the switch's
	// default case rejects any other letter.
	const auto result = Mount("E " + P("bootable.img") + " -fs none");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsNonexistentPath)
{
	// Not a directory or regular file -> PROGRAM_MOUNT_ERROR_2.
	const auto result = Mount("G " + P("does_not_exist_at_all"));
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, RejectsOverlayWithoutMountedBase)
{
	const auto result = Mount("E " + P("overlay_layer") + " -t overlay");
	EXPECT_FALSE(result.has_value());
}

TEST_F(MountTest, WildcardMatchingNothingFallsBackToLiteralPath)
{
	// A glob that matches no files does NOT cause ProcessArguments() to
	// fail. AddWildcardPaths() expansion comes up empty, so ProcessPaths()
	// falls back to treating the literal, unexpanded glob string itself as
	// the single path. Whether that literal "path" turns out to be openable
	// is left to MountImage().
	const auto result = Mount("F " + P("nomatch_*.img") + " -t floppy");

	ASSERT_TRUE(result.has_value());
	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_NE(result->paths[0].find("nomatch_*.img"), std::string::npos);
}

TEST_F(MountTest, RejectsAlreadyMountedDrive)
{
	// Mounts and re-mounts the same letter within one test.
	const auto first = Mount("J " + P("plain_dir"));
	ASSERT_TRUE(first.has_value());

	const auto second = Mount("J " + P("overlay_layer"));
	EXPECT_FALSE(second.has_value());
}

// ---------------------------------------------------------------------
// Directory / overlay mounts (MountLocal() path).
//
// Note `params.paths` is NOT populated for this branch (it's only used for
// image mounts) so these tests check the fields MountLocal() itself consumes
// or mutates (drive, type, sizes, roflag, label, mediaid).
// ---------------------------------------------------------------------

TEST_F(MountTest, MountsPlainDirectoryWithDefaults)
{
	const auto result = Mount("L " + P("plain_dir"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->drive, 'L');
	EXPECT_FALSE(result->is_drive_number);
	EXPECT_FALSE(result->roflag);

	// Default "dir" geometry from ParseGeometry
	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);

	// MountLocal mutates params.label to "<drive>_DRIVE" when none given
	EXPECT_EQ(result->label, "L_DRIVE");
}

TEST_F(MountTest, DirectoryMountOnDriveA_UsesFloppyMediaId)
{
	// ParseGeometry special-cases drive A/B for dir/overlay mounts.
	const auto result = Mount("A " + P("plain_dir"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);
}

TEST_F(MountTest, DirectoryMountReadOnly)
{
	const auto result = Mount("M " + P("plain_dir") + " -ro");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_TRUE(result->roflag);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);
}

TEST_F(MountTest, DirectoryMountWithExplicitLabelIsNotOverwritten)
{
	const auto result = Mount("N " + P("plain_dir") + " -label MYLABEL");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);

	EXPECT_EQ(result->label, "MYLABEL");
}

TEST_F(MountTest, OverlayMountsOnTopOfExistingDrive)
{
	const auto base = Mount("O " + P("overlay_base"));
	ASSERT_TRUE(base.has_value());

	const auto result = Mount("O " + P("overlay_layer") + " -t overlay");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Overlay);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->drive, 'O');
	EXPECT_EQ(result->label, "O_DRIVE");

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);
}

// ---------------------------------------------------------------------
// Geometry parsing (ParseGeometry)
// ---------------------------------------------------------------------

TEST_F(MountTest, FloppyDefaultsGeometryAndMediaId)
{
	// -t floppy is an "explicit image type", so this hits the image
	// branch via a regular file, but the geometry defaults come from
	// `ParseGeometry()` regardless of that branch.
	const auto result = Mount("B " + P("raw.dat") + " -t floppy");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 2880);
	EXPECT_EQ(result->sizes[3], 2880);
}

TEST_F(MountTest, IsoDefaultsGeometryAndFstype)
{
	const auto result = Mount("P " + P("image.iso") + " -t iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, ExplicitSizeOverridesDefaults)
{
	const auto result = Mount("1 " + P("bootable.img") +
	                          " -t hdd -size 512,63,16,100");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 100);
}

TEST_F(MountTest, ExplicitChsOverridesExplicitSize)
{
	// -chs is parsed after -size in ParseGeometry, so it should win.
	const auto result = Mount("2 " + P("bootable.img") +
	                          " -t hdd -size 512,63,16,50 -chs 200,16,63");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);  // sectors
	EXPECT_EQ(result->sizes[2], 16);  // heads
	EXPECT_EQ(result->sizes[3], 200); // cylinders
}

TEST_F(MountTest, FreesizeOverridesDirDefaults)
{
	const auto result = Mount("Q " + P("plain_dir") + " -freesize 100");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// total_size_cyl stays 32765 (100MB free is under the ~250MB
	// implied default); free_size_cyl = 100*1024*1024/(512*32) = 6400.
	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 6400);
}

TEST_F(MountTest, FreesizeForFloppyIsInKbNotMb)
{
	const auto result = Mount("D " + P("raw.dat") +
	                          " -t floppy -fs none -freesize 720");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 2880);
	EXPECT_EQ(result->sizes[3], 720 * 1024 / 512);
}

// ---------------------------------------------------------------------
// Drive parsing (ParseDrive)
// ---------------------------------------------------------------------

TEST_F(MountTest, DriveNumberForcesNoneFstypeWhenNotExplicit)
{
	const auto result = Mount("0 " + P("bootable.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->is_drive_number);
	EXPECT_EQ(result->drive, '0');

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// MountImageFat() will autodetect the HDD image's geometry if sizes
	// contains all zeroes.
	//
	// We're not testing the autodetection logic here; we'd need to write a
	// valid HDD image for that in the test setup. Maybe later.
	//
	EXPECT_EQ(result->sizes[0], 0);
	EXPECT_EQ(result->sizes[1], 0);
	EXPECT_EQ(result->sizes[2], 0);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, LetterAtoDRemapsToDriveNumberWithFsNone)
{
	const auto result = Mount("C " + P("bootable.img") + " -fs none");

	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->is_drive_number);

	// C -> drive number 2
	EXPECT_EQ(result->drive, '2');

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// MountImageFat() will autodetect the HDD image's geometry if sizes
	// contains all zeroes.
	EXPECT_EQ(result->sizes[0], 0);
	EXPECT_EQ(result->sizes[1], 0);
	EXPECT_EQ(result->sizes[2], 0);
	EXPECT_EQ(result->sizes[3], 0);
}

// ---------------------------------------------------------------------
// -t flag aliasing
// ---------------------------------------------------------------------

TEST_F(MountTest, CdromAliasesToIso)
{
	const auto result = Mount("R " + P("image.iso") + " -t cdrom");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, MountType::CdRomImage);
}

TEST_F(MountTest, FddAliasesToFloppy)
{
	const auto result = Mount("B " + P("raw.dat") + " -t fdd");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, MountType::FloppyImage);
}

TEST_F(MountTest, TypeValuesAreCaseInsensitive)
{
	struct TestCase {
		std::string args        = {};
		MountType expected_type = {};
	};

	const std::vector<TestCase> test_cases = {
	        {	              "A " + P("raw.dat") + " -t FLOPPY",MountType::FloppyImage                                                                          },
	        {	                 "B " + P("raw.dat") + " -t Fdd", MountType::FloppyImage},
	        {"3 " + P("bootable.img") + " -t HDD -size 512,63,16,100",
	         MountType::HardDiskImage	                                                },
	        {	               "D " + P("image.iso") + " -t ISO",  MountType::CdRomImage},
	        {	             "E " + P("image.iso") + " -t CdRom",  MountType::CdRomImage},
	        {	               "F " + P("plain_dir") + " -t DIR",   MountType::Directory},
	};

	for (const auto& [args, expected_type] : test_cases) {
		SCOPED_TRACE(args);

		const auto result = Mount(args);

		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->type, expected_type);
	}
}

TEST_F(MountTest, OverlayTypeValueIsCaseInsensitive)
{
	ASSERT_TRUE(Mount("O " + P("overlay_base")).has_value());

	const auto result = Mount("O " + P("overlay_layer") + " -t OverLay");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, MountType::Overlay);
}

// ---------------------------------------------------------------------
// -ide flag
// ---------------------------------------------------------------------

TEST_F(MountTest, IdeFlagSetWithoutInvokingCableSlotLookupForNonIsoType)
{
	const auto result = Mount("3 " + P("bootable.img") +
	                          " -t hdd -size 512,63,16,100 -ide");

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->is_ide);
	// ide_index/is_second_cable_slot are only touched by
	// IDE_Get_Next_Cable_Slot, which the source only calls for -t iso.
	EXPECT_EQ(result->ide_index, -1);
	EXPECT_FALSE(result->is_second_cable_slot);

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 100);
}

// ---------------------------------------------------------------------
// Image-mode path collection (ProcessPaths)
// ---------------------------------------------------------------------

TEST_F(MountTest, ImplicitImageModeAutoDetectsIsoFromExtension)
{
	// No -t given; a plain existing regular file still triggers image
	// mode, and the ".iso" extension auto-sets type+fstype.
	const auto result = Mount("S " + P("image.iso"));

	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
}

TEST_F(MountTest, AutoDetectsHddTypeFromImgExtension)
{
	const auto result = Mount("U " + P("image.img"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, MountType::HardDiskImage);
}

TEST_F(MountTest, ExplicitTypeOverridesExtensionAutoDetection)
{
	const auto result = Mount("V " + P("image.img") + " -t iso");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, MountType::CdRomImage);
}

TEST_F(MountTest, GeometryOptionsAreNotCollectedAsPaths)
{
	const auto result = Mount("2 " + P("bootable.img") +
	                          " -t hdd -freesize 100 -size 512,63,16,50"
	                          " -chs 200,16,63");

	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_NE(result->paths[0].find("bootable.img"), std::string::npos);
}

TEST_F(MountTest, MultipleExplicitPathsArePreservedInOrder)
{
	const auto result = Mount("W " + P("disk03.img") + " " + P("disk1.img") +
	                          " " + P("disk02.img") + " -t floppy");

	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(result->paths.size(), 3);
	EXPECT_NE(result->paths[0].find("disk03.img"), std::string::npos);
	EXPECT_NE(result->paths[1].find("disk1.img"), std::string::npos);
	EXPECT_NE(result->paths[2].find("disk02.img"), std::string::npos);
}

TEST_F(MountTest, WildcardExpandsToMatchingFileSetUsingNaturalSort)
{
	const auto result = Mount("K " + P("disk*.img") + " -t floppy");

	ASSERT_TRUE(result.has_value());
	ASSERT_EQ(result->paths.size(), 3);

	for (const auto* expected : {"disk1.img", "disk02.img", "disk03.img"}) {
		const bool found = std::any_of(result->paths.begin(),
		                               result->paths.end(),
		                               [&](const std::string& p) {
			                               return p.find(expected) !=
			                                      std::string::npos;
		                               });
		EXPECT_TRUE(found) << "missing " << expected;
	}
}

TEST_F(MountTest, FloppyMediaIdSetWhenTypeFloppyAndFstypeFat)
{
	const auto result = Mount("H " + P("raw.dat") + " -t floppy -fs fat");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

// ---------------------------------------------------------------------
// Various edge cases
// ---------------------------------------------------------------------

TEST_F(MountTest, DirectoryOverridesExplicitFloppyTypeAndSetsFloppyLabel)
{
	const auto result = Mount("T " + P("plain_dir") + " -t floppy");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->label, "T_FLOPPY");
}

TEST_F(MountTest,
       RawHddDriveNumberMissingGeometrySucceedsAtParseLayerDespiteInternalFailure)
{
	// MountImageRaw() internally detects the missing geometry and
	// returns false, but ProcessPaths() ignores that return value on
	// every image-mount branch, so ProcessArguments() still returns a
	// populated MountParameters.
	const auto result = Mount("2 " + P("raw.dat") + " -t hdd -fs none");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->sizes[0], 0);
	EXPECT_EQ(result->sizes[1], 0);
	EXPECT_EQ(result->sizes[2], 0);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, ResolvesPathThroughAlreadyMountedDosDrive)
{
	// First mount a real directory to a drive letter, then reference
	// that drive's virtual path as the *source* path for a second
	// mount, exercising GetDosMappedHostPath's fallback branch in
	// ProcessPaths (stat on the host fails, so it checks whether the
	// path maps through an existing Local drive instead).
	const auto base = Mount("I " + P("plain_dir"));
	ASSERT_TRUE(base.has_value());

	const auto via_dos_path = Mount("J I:\\");
	ASSERT_TRUE(via_dos_path.has_value());
	EXPECT_EQ(via_dos_path->drive, 'J');
}

// ---------------------------------------------------------------------
// Extension-based auto-detection when no -t is given
// ---------------------------------------------------------------------

TEST_F(MountTest, AutoDetectsIsoFromCueExtension)
{
	const auto result = Mount("E " + P("image.cue"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, AutoDetectsIsoFromBinExtension)
{
	const auto result = Mount("E " + P("image.bin"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, AutoDetectsIsoFromMdsExtension)
{
	const auto result = Mount("E " + P("image.mds"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, AutoDetectsIsoFromCcdExtension)
{
	const auto result = Mount("E " + P("image.ccd"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, AutoDetectsHddFromImgExtension)
{
	const auto result = Mount("E " + P("image.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// MountImageFat() will autodetect the HDD image's geometry if sizes
	// contains all zeroes.
	//
	// We're not testing the autodetection logic here; we'd need to write a
	// valid HDD image for that in the test setup. Maybe later.
	//
	EXPECT_EQ(result->sizes[0], 0);
	EXPECT_EQ(result->sizes[1], 0);
	EXPECT_EQ(result->sizes[2], 0);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, AutoDetectsHddFromImaExtension)
{
	const auto result = Mount("E " + P("image.ima"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// MountImageFat() will autodetect the HDD image's geometry if sizes
	// contains all zeroes.
	EXPECT_EQ(result->sizes[0], 0);
	EXPECT_EQ(result->sizes[1], 0);
	EXPECT_EQ(result->sizes[2], 0);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, AutoDetectsHddFromVhdExtension)
{
	const auto result = Mount("F " + P("image.vhd"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// MountImageFat() will autodetect the HDD image's geometry if sizes
	// contains all zeroes.
	EXPECT_EQ(result->sizes[0], 0);
	EXPECT_EQ(result->sizes[1], 0);
	EXPECT_EQ(result->sizes[2], 0);
	EXPECT_EQ(result->sizes[3], 0);
}

// ---------------------------------------------------------------------
// Known-crashing combinations
// ---------------------------------------------------------------------

/* TODO this test only passes in debug mode; fix this at some point

TEST_F(MountTest, DriveNumberWithExplicitFatFstypeCrashesOnDriveIndex)
{
        // MountImageFat() calls drive_index() on params.drive, which is a
        // digit character ('1') when is_drive_number is true and -fs fat
        // is given explicitly. drive_index() asserts drive_letter is 'A'-'Z',
        // so this combination currently aborts the process. Pinning that
        // behaviour rather than hiding it.
        EXPECT_DEATH(Mount("1 " + P("bootable.img") +
                           " -fs fat -t hdd -size 512,63,16,100"),
                     "drive_letter");
}
*/

// ---------------------------------------------------------------------
// Option precedence
// ---------------------------------------------------------------------

TEST_F(MountTest, ExplicitSizeOverridesFreesize)
{
	const auto result = Mount("X " + P("plain_dir") +
	                          " -freesize 100 -size 512,63,16,42");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 42);
}

TEST_F(MountTest, ChsOverridesFreesize)
{
	const auto result = Mount("X " + P("plain_dir") +
	                          " -freesize 100 -chs 200,16,63");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 200);
}

TEST_F(MountTest, ChsOverridesSizeRegardlessOfArgumentOrder)
{
	const auto result = Mount("1 " + P("bootable.img") +
	                          " -chs 200,16,63 -size 512,63,16,50 -t hdd");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::None);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 200);
}

// ---------------------------------------------------------------------
// Auto-detection precedence
// ---------------------------------------------------------------------

TEST_F(MountTest, ExplicitIsoTypeOverridesImgExtension)
{
	const auto result = Mount("X " + P("image.img") + " -t iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, ExplicitHddTypeKeepsFatFilesystem)
{
	const auto result = Mount("X " + P("image.iso") +
	                          " -t hdd -size 512,63,16,100");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 100);
}

// ---------------------------------------------------------------------
// Explicit -fs interactions
// ---------------------------------------------------------------------

TEST_F(MountTest, IsoExtensionOverridesExplicitFatFs)
{
	const auto result = Mount("X " + P("image.iso") + " -fs fat");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

TEST_F(MountTest, IsoTypeRejectsNoneFilesystem)
{
	const auto result = Mount("X " + P("image.iso") + " -t iso -fs none");

	EXPECT_FALSE(result.has_value());
}

// TODO seems wrong; should be rejected
TEST_F(MountTest, ExplicitIsoFsIsPreservedForFloppyType)
{
	const auto result = Mount("A " + P("raw.dat") + " -t floppy -fs iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);

	// MediaId promotion only happens for floppy+fat.
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

TEST_F(MountTest, ExplicitIsoTypeOverridesFloppyExtension)
{
	const auto result = Mount("D " + P("bootable.img") + " -t iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 2048);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 65535);
	EXPECT_EQ(result->sizes[3], 0);
}

TEST_F(MountTest, ExplicitFloppyTypeOverridesIsoExtension)
{
	const auto result = Mount("A " + P("image.iso") + " -t floppy");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);

	// Pin down whatever the parser currently does.
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

TEST_F(MountTest, ExplicitIsoFilesystemWithoutType)
{
	const auto result = Mount("D " + P("bootable.img") + " -fs iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

TEST_F(MountTest, ExplicitFatFilesystemWithoutType)
{
	const auto result = Mount("D " + P("bootable.img") + " -fs fat");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

TEST_F(MountTest, FilesystemValuesAreCaseInsensitive)
{
	struct TestCase {
		std::string args                    = {};
		MountFileSystemType expected_fstype = {};
	};

	const std::vector<TestCase> test_cases = {
	        {	                    "D " + P("bootable.img") + " -fs FAT",MountFileSystemType::Fat16	                                                                           },
	        {	                    "E " + P("bootable.img") + " -fs Iso",   MountFileSystemType::Iso},
	        {"2 " + P("bootable.img") + " -t hdd -fs NONE -size 512,63,16,100",
	         MountFileSystemType::None	                                                            },
	};

	for (const auto& [args, expected_fstype] : test_cases) {
		SCOPED_TRACE(args);

		const auto result = Mount(args);

		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->fstype, expected_fstype);
	}
}

TEST_F(MountTest, ExplicitIsoFilesystemOnIsoImage)
{
	const auto result = Mount("D " + P("image.iso") + " -fs iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

// ---------------------------------------------------------------------
// Image geometry oddities
// ---------------------------------------------------------------------

TEST_F(MountTest, SizeAcceptedForIsoMount)
{
	const auto result = Mount("X " + P("image.iso") + " -t iso -size 512,63,16,99");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 99);
}

TEST_F(MountTest, ChsAcceptedForIsoMount)
{
	const auto result = Mount("X " + P("image.iso") + " -t iso -chs 123,8,17");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 17);
	EXPECT_EQ(result->sizes[2], 8);
	EXPECT_EQ(result->sizes[3], 123);
}

TEST_F(MountTest, FreesizeMutatesGeometryForImageMount)
{
	const auto result = Mount("1 " + P("bootable.img") + " -t hdd -freesize 50");

	ASSERT_TRUE(result.has_value());

	EXPECT_NE(result->sizes[3], 0);
}

// ---------------------------------------------------------------------
// Multiple image behaviour
// ---------------------------------------------------------------------

TEST_F(MountTest, FirstImageControlsAutoDetection)
{
	const auto result = Mount("X " + P("image.img") + " " + P("image.iso"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	ASSERT_EQ(result->paths.size(), 2);
}

TEST_F(MountTest, FirstImageControlsAutoDetectionReverseOrder)
{
	const auto result = Mount("X " + P("image.iso") + " " + P("image.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	ASSERT_EQ(result->paths.size(), 2);
}

TEST_F(MountTest, FirstIsoImageControlsAutoDetection)
{
	const auto result = Mount("D " + P("image.iso") + " " + P("bootable.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	ASSERT_EQ(result->paths.size(), 2);
}

TEST_F(MountTest, MultipleImagesWithOptionsInterleaved)
{
	const auto result = Mount("W " + P("disk03.img") + " -t floppy " +
	                          P("disk1.img") + " -ro " + P("disk02.img") +
	                          " -label SWAP");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk03.img", "disk1.img", "disk02.img"}));

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_TRUE(result->roflag);
	EXPECT_EQ(result->label, "SWAP");
}

TEST_F(MountTest, MultipleImagesWithOptionsBeforePaths)
{
	const auto result = Mount("W -t floppy -label SWAP " + P("disk03.img") +
	                          " " + P("disk1.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk03.img", "disk1.img"}));

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->label, "SWAP");
}

TEST_F(MountTest, MultipleImagesOnDriveWithColon)
{
	const auto result = Mount("W: " + P("disk1.img") + " " +
	                          P("disk02.img") + " -t floppy");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->drive, 'W');
	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk1.img", "disk02.img"}));
}

TEST_F(MountTest, WildcardImagesWithGeometryAndFlagOptions)
{
	const auto result = Mount("A " + P("disk*.img") +
	                          " -t floppy -freesize 720 -ro");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk1.img", "disk02.img", "disk03.img"}));

	EXPECT_TRUE(result->roflag);

	// 720 KB of free space on a 1.44 MB floppy
	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 1);
	EXPECT_EQ(result->sizes[2], 2880);
	EXPECT_EQ(result->sizes[3], 1440);
}

TEST_F(MountTest, MultipleHddImagesShareExplicitGeometry)
{
	const auto result = Mount("C " + P("disk1.img") + " " +
	                          P("disk02.img") + " -t hdd -chs 40,16,63");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk1.img", "disk02.img"}));

	EXPECT_EQ(result->type, MountType::HardDiskImage);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 40);
}

TEST_F(MountTest, MultipleCdImagesWithLabel)
{
	const auto result = Mount("D " + P("image.iso") + " " + P("image.cue") +
	                          " -t iso -label MYCD");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"image.iso", "image.cue"}));

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->label, "MYCD");
}

TEST_F(MountTest, MultipleImagesWithLabelStartingWithDash)
{
	const auto result = Mount("W " + P("disk1.img") + " " +
	                          P("disk02.img") + " -t floppy -label -SWAP-");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk1.img", "disk02.img"}));

	EXPECT_EQ(result->label, "-SWAP-");
}

TEST_F(MountTest, SequentialMountsDoNotShareOptionState)
{
	const auto first = Mount("E " + P("plain_dir") + " -label ONE -ro");
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->label, "ONE");
	EXPECT_TRUE(first->roflag);

	// A mount rejected during option parsing must not occupy the drive
	EXPECT_FALSE(Mount("F " + P("overlay_base") + " -label").has_value());

	const auto second = Mount("F " + P("overlay_base"));
	ASSERT_TRUE(second.has_value());
	EXPECT_NE(second->label, "ONE");
	EXPECT_FALSE(second->roflag);

	const auto third = Mount("W " + P("disk1.img") + " " + P("disk02.img") +
	                         " -t floppy");
	ASSERT_TRUE(third.has_value());
	EXPECT_EQ(FileNames(*third),
	          (std::vector<std::string>{"disk1.img", "disk02.img"}));
	EXPECT_TRUE(third->label.empty());
	EXPECT_FALSE(third->roflag);
}

// ---------------------------------------------------------------------
// IDE interactions
// ---------------------------------------------------------------------

TEST_F(MountTest, IdeFlagDoesNotAllocateControllerForNonIsoType)
{
	const auto result = Mount("D " + P("bootable.img") +
	                          " -t hdd -fs iso -ide -size 512,63,16,100");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_TRUE(result->is_ide);
	EXPECT_EQ(result->ide_index, -1);
	EXPECT_FALSE(result->is_second_cable_slot);
}

TEST_F(MountTest, IdeFlagDoesNotConsumeFollowingOption)
{
	const auto result = Mount("3 " + P("bootable.img") +
	                          " -t hdd -ide -size 512,63,16,100");

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->is_ide);
	EXPECT_EQ(result->paths.size(), 1);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 100);
}

TEST_F(MountTest, IdeFlagDoesNotConsumeFollowingPath)
{
	const auto result = Mount("3 -t hdd -size 512,63,16,100 -ide " +
	                          P("bootable.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->is_ide);

	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_NE(result->paths[0].find("bootable.img"), std::string::npos);
}

TEST_F(MountTest, IdeFlagDoesNotConsumeAnyFollowingValue)
{
	// -ide takes no value. DOSBox-X slot values such as `auto` or `2m` are
	// not recognised and are treated as paths like anything else.
	for (const auto* value : {"auto", "none", "1", "2m", "0", "1x"}) {
		SCOPED_TRACE(value);

		const auto result = Mount("3 " + P("bootable.img") +
		                          " -t hdd -size 512,63,16,100 -ide " +
		                          value);

		ASSERT_TRUE(result.has_value());

		EXPECT_TRUE(result->is_ide);
		EXPECT_EQ(FileNames(*result),
		          (std::vector<std::string>{"bootable.img", value}));
	}
}

TEST_F(MountTest, IdeFlagFollowedBySecondImageKeepsBothImages)
{
	const auto result = Mount("W " + P("disk1.img") + " -t floppy -ide " +
	                          P("disk02.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->is_ide);
	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk1.img", "disk02.img"}));
}

// ---------------------------------------------------------------------
// Geometry precedence with ISO images
// ---------------------------------------------------------------------

TEST_F(MountTest, ExplicitSizeWithIsoImage)
{
	const auto result = Mount("D " + P("image.iso") + " -size 512,63,16,99");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// Pin down whether ISO defaults or explicit size wins.
}

TEST_F(MountTest, ExplicitChsWithIsoImage)
{
	const auto result = Mount("D " + P("image.iso") + " -chs 123,8,17");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
}

// ---------------------------------------------------------------------
// Miscellaneous parser state
// ---------------------------------------------------------------------

TEST_F(MountTest, LabelPreservedForIsoMount)
{
	const auto result = Mount("D " + P("image.iso") + " -label MYDISC");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->label, "MYDISC");
}

TEST_F(MountTest, LabelCanStartWithDash)
{
	const auto result = Mount("D " + P("image.iso") + " -label -MYDISC-");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->label, "-MYDISC-");
	EXPECT_EQ(result->paths.size(), 1);
}

TEST_F(MountTest, ReadOnlyPreservedForIsoMount)
{
	const auto result = Mount("D " + P("image.iso") + " -ro");

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->roflag);
}

TEST_F(MountTest, ReadOnlyPreservedForDirectoryMount)
{
	const auto result = Mount("D " + P("plain_dir") + " -ro");

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->roflag);
}

TEST_F(MountTest, LabelBeforeDirectoryPathIsNotTakenAsPath)
{
	const auto result = Mount("N -label MYLABEL " + P("plain_dir"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);
	EXPECT_EQ(result->label, "MYLABEL");
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"plain_dir"}));
}

TEST_F(MountTest, OptionNamesAreCaseInsensitive)
{
	const auto result = Mount("3 " + P("bootable.img") +
	                          " -T hdd -SIZE 512,63,16,100 -Ro -IDE");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::HardDiskImage);
	EXPECT_TRUE(result->roflag);
	EXPECT_TRUE(result->is_ide);
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"bootable.img"}));

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 63);
	EXPECT_EQ(result->sizes[2], 16);
	EXPECT_EQ(result->sizes[3], 100);
}

// ---------------------------------------------------------------------
// Duplicate option precedence
// ---------------------------------------------------------------------

TEST_F(MountTest, DuplicateLabelFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -label FIRST"
	                          " -label SECOND");

	ASSERT_TRUE(result.has_value());

	// The repeated option must not be collected as an image path
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"image.img"}));

	EXPECT_EQ(result->label, "FIRST");
}

TEST_F(MountTest, DuplicateSizeFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -size 1,2,3,4"
	                          " -size 5,6,7,8");

	ASSERT_TRUE(result.has_value());

	// The repeated option must not be collected as an image path
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"image.img"}));

	EXPECT_EQ(result->sizes[0], 1);
	EXPECT_EQ(result->sizes[1], 2);
	EXPECT_EQ(result->sizes[2], 3);
	EXPECT_EQ(result->sizes[3], 4);
}

TEST_F(MountTest, DuplicateChsFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -chs 100,17,8"
	                          " -chs 200,63,16");

	ASSERT_TRUE(result.has_value());

	// The repeated option must not be collected as an image path
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"image.img"}));

	// CHS is normalized into the size array:
	// sector size, sectors/track, heads, cylinders.
	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 8);
	EXPECT_EQ(result->sizes[2], 17);
	EXPECT_EQ(result->sizes[3], 100);
}

TEST_F(MountTest, DuplicateFilesystemFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -fs fat"
	                          " -fs iso");

	ASSERT_TRUE(result.has_value());

	// The repeated option must not be collected as an image path
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"image.img"}));

	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
}

TEST_F(MountTest, DuplicateTypeFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -t floppy"
	                          " -t iso");

	ASSERT_TRUE(result.has_value());

	// The repeated option must not be collected as an image path
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"image.img"}));

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

TEST_F(MountTest, DuplicateFreesizeFirstWins)
{
	const auto result = Mount("X " + P("plain_dir") +
	                          " -freesize 100"
	                          " -freesize 200");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 6400);
}

TEST_F(MountTest, DuplicateFlagsAreNotCollectedAsPaths)
{
	const auto result = Mount("3 " + P("bootable.img") +
	                          " -t hdd -size 512,63,16,100"
	                          " -ro -ro -pr -pr -ide -ide -ide");

	ASSERT_TRUE(result.has_value());

	EXPECT_TRUE(result->roflag);
	EXPECT_TRUE(result->is_ide);
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"bootable.img"}));
}

TEST_F(MountTest, DuplicateOptionsWithMultipleImages)
{
	const auto result = Mount("W " + P("disk1.img") + " -label ONE " +
	                          P("disk02.img") + " -t floppy -label TWO " +
	                          P("disk03.img") + " -t hdd");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(FileNames(*result),
	          (std::vector<std::string>{"disk1.img", "disk02.img", "disk03.img"}));

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(result->label, "ONE");
}

// ---------------------------------------------------------------------
// Real-world command lines mounting actual disk images
//
// These tests build minimal but valid disk images (boot sectors, empty FATs
// and root directories, ISO 9660 volume descriptors), so they can check the
// mounted drives themselves, not just the parsed parameters. Most of each
// image is left sparse to keep the tests fast.
// ---------------------------------------------------------------------

constexpr int SectorSize    = 512;
constexpr int IsoSectorSize = 2048;

// 20 MB hard disk image, the smallest MAKEIMG preset
constexpr int HddCylinders       = 40;
constexpr int HddHeads           = 16;
constexpr int HddSectorsPerTrack = 63;
constexpr int HddTotalSectors    = HddCylinders * HddHeads * HddSectorsPerTrack;

const std::string HddSizeOption = "-size 512,63,16,40";
const std::string HddChsOption  = "-chs 40,16,63";

using Bytes = std::vector<uint8_t>;

void put_le16(Bytes& bytes, const size_t pos, const int value)
{
	bytes.at(pos)     = static_cast<uint8_t>(value & 0xff);
	bytes.at(pos + 1) = static_cast<uint8_t>((value >> 8) & 0xff);
}

void put_le32(Bytes& bytes, const size_t pos, const int value)
{
	put_le16(bytes, pos, value & 0xffff);
	put_le16(bytes, pos + 2, (value >> 16) & 0xffff);
}

void put_be16(Bytes& bytes, const size_t pos, const int value)
{
	bytes.at(pos)     = static_cast<uint8_t>((value >> 8) & 0xff);
	bytes.at(pos + 1) = static_cast<uint8_t>(value & 0xff);
}

void put_be32(Bytes& bytes, const size_t pos, const int value)
{
	put_be16(bytes, pos, (value >> 16) & 0xffff);
	put_be16(bytes, pos + 2, value & 0xffff);
}

// ISO 9660 stores numbers in both little and big-endian byte order
void put_both16(Bytes& bytes, const size_t pos, const int value)
{
	put_le16(bytes, pos, value);
	put_be16(bytes, pos + 2, value);
}

void put_both32(Bytes& bytes, const size_t pos, const int value)
{
	put_le32(bytes, pos, value);
	put_be32(bytes, pos + 4, value);
}

// Writes the text into a fixed-length field padded with spaces
void put_text(Bytes& bytes, const size_t pos, const std::string_view text,
              const size_t field_length)
{
	for (size_t i = 0; i < field_length; ++i) {
		bytes.at(pos + i) = (i < text.size())
		                          ? static_cast<uint8_t>(text[i])
		                          : ' ';
	}
}

void create_blank_file(const std_fs::path& path, const uintmax_t size_bytes)
{
	std_fs::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary | std::ios::trunc).close();
	std_fs::resize_file(path, size_bytes);
}

void write_bytes(const std_fs::path& path, const int64_t offset, const Bytes& bytes)
{
	std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
	file.seekp(offset);
	file.write(reinterpret_cast<const char*>(bytes.data()),
	           static_cast<std::streamsize>(bytes.size()));
}

Bytes read_bytes(const std_fs::path& path, const int64_t offset, const size_t num_bytes)
{
	Bytes bytes(num_bytes, 0);
	std::ifstream file(path, std::ios::binary);
	file.seekg(offset);
	file.read(reinterpret_cast<char*>(bytes.data()),
	          static_cast<std::streamsize>(bytes.size()));
	return bytes;
}

struct FatVolume {
	int first_sector         = 0;
	int total_sectors        = 0;
	int sectors_per_cluster  = 0;
	int root_entries         = 0;
	int sectors_per_fat      = 0;
	int sectors_per_track    = 0;
	int heads                = 0;
	uint8_t media_descriptor = 0;
	bool is_fat16            = false;
};

// Writes an empty FAT volume containing only a volume label
void write_fat_volume(const std_fs::path& path, const FatVolume& volume,
                      const std::string_view label)
{
	Bytes boot_sector(SectorSize, 0);

	// Short jump over the BIOS parameter block
	boot_sector[0] = 0xeb;
	boot_sector[1] = 0x3c;
	boot_sector[2] = 0x90;

	put_text(boot_sector, 3, "MSDOS5.0", 8);
	put_le16(boot_sector, 11, SectorSize);
	boot_sector[13] = static_cast<uint8_t>(volume.sectors_per_cluster);
	put_le16(boot_sector, 14, 1); // reserved sectors
	boot_sector[16] = 2;          // number of FATs
	put_le16(boot_sector, 17, volume.root_entries);
	put_le16(boot_sector, 19, volume.total_sectors);
	boot_sector[21] = volume.media_descriptor;
	put_le16(boot_sector, 22, volume.sectors_per_fat);
	put_le16(boot_sector, 24, volume.sectors_per_track);
	put_le16(boot_sector, 26, volume.heads);
	put_le32(boot_sector, 28, volume.first_sector); // hidden sectors
	boot_sector[38] = 0x29; // extended boot signature
	put_text(boot_sector, 43, label, 11);
	put_text(boot_sector, 54, volume.is_fat16 ? "FAT16" : "FAT12", 8);
	boot_sector[510] = 0x55;
	boot_sector[511] = 0xaa;

	write_bytes(path, int64_t{volume.first_sector} * SectorSize, boot_sector);

	// The first two FAT entries hold the media descriptor and an
	// end-of-chain marker
	Bytes fat_start = {volume.media_descriptor, 0xff, 0xff};
	if (volume.is_fat16) {
		fat_start.push_back(0xff);
	}

	const auto first_fat_sector = volume.first_sector + 1;
	for (auto i = 0; i < 2; ++i) {
		const auto sector = first_fat_sector + i * volume.sectors_per_fat;
		write_bytes(path, int64_t{sector} * SectorSize, fat_start);
	}

	Bytes label_entry(32, 0);
	put_text(label_entry, 0, label, 11);
	label_entry[11] = 0x08; // volume label attribute

	const auto root_dir_sector = first_fat_sector + 2 * volume.sectors_per_fat;
	write_bytes(path, int64_t{root_dir_sector} * SectorSize, label_entry);
}

// 1.44 MB FAT12 floppy image
void write_floppy_image(const std_fs::path& path, const std::string_view label)
{
	constexpr auto TotalSectors = 2880;

	create_blank_file(path, TotalSectors * SectorSize);

	write_fat_volume(path,
	                 {.first_sector        = 0,
	                  .total_sectors       = TotalSectors,
	                  .sectors_per_cluster = 1,
	                  .root_entries        = 224,
	                  .sectors_per_fat     = 9,
	                  .sectors_per_track   = 18,
	                  .heads               = 2,
	                  .media_descriptor    = 0xf0,
	                  .is_fat16            = false},
	                 label);
}

// Hard disk image with a bootable MBR and a single FAT16 partition
void write_hdd_image(const std_fs::path& path, const std::string_view label)
{
	create_blank_file(path, int64_t{HddTotalSectors} * SectorSize);

	const auto first_sector      = HddSectorsPerTrack;
	const auto partition_sectors = HddTotalSectors - first_sector;

	constexpr auto PartitionEntry = 446;

	Bytes mbr(SectorSize, 0);
	mbr[PartitionEntry]     = 0x80; // active partition
	mbr[PartitionEntry + 4] = 0x04; // FAT16 smaller than 32 MB
	put_le32(mbr, PartitionEntry + 8, first_sector);
	put_le32(mbr, PartitionEntry + 12, partition_sectors);
	mbr[510] = 0x55;
	mbr[511] = 0xaa;

	write_bytes(path, 0, mbr);

	write_fat_volume(path,
	                 {.first_sector        = first_sector,
	                  .total_sectors       = partition_sectors,
	                  .sectors_per_cluster = 4,
	                  .root_entries        = 512,
	                  .sectors_per_fat     = 40,
	                  .sectors_per_track   = HddSectorsPerTrack,
	                  .heads               = HddHeads,
	                  .media_descriptor    = 0xf8,
	                  .is_fat16            = true},
	                 label);
}

Bytes make_iso_directory_record(const int extent_sector, const uint8_t identifier)
{
	Bytes record(34, 0);
	record[0] = 34; // record length
	put_both32(record, 2, extent_sector);
	put_both32(record, 10, IsoSectorSize); // data length
	record[25] = 0x02;                     // directory flag
	put_both16(record, 28, 1);             // volume sequence number
	record[32] = 1;                        // identifier length
	record[33] = identifier;
	return record;
}

// ISO 9660 image with an empty root directory
void write_iso_image(const std_fs::path& path, const std::string_view volume_id)
{
	constexpr auto PrimaryDescriptorSector = 16;
	constexpr auto TerminatorSector        = 17;
	constexpr auto RootDirectorySector     = 18;
	constexpr auto TotalSectors            = 19;

	create_blank_file(path, TotalSectors * IsoSectorSize);

	const auto root_record = make_iso_directory_record(RootDirectorySector, 0x00);

	Bytes primary_descriptor(IsoSectorSize, 0);
	primary_descriptor[0] = 1;
	put_text(primary_descriptor, 1, "CD001", 5);
	primary_descriptor[6] = 1;
	put_text(primary_descriptor, 8, "", 32); // system identifier
	put_text(primary_descriptor, 40, volume_id, 32);
	put_both32(primary_descriptor, 80, TotalSectors);
	put_both16(primary_descriptor, 120, 1); // volume set size
	put_both16(primary_descriptor, 124, 1); // volume sequence number
	put_both16(primary_descriptor, 128, IsoSectorSize);
	std::ranges::copy(root_record, primary_descriptor.begin() + 156);
	primary_descriptor[881] = 1; // file structure version

	write_bytes(path, PrimaryDescriptorSector * IsoSectorSize, primary_descriptor);

	Bytes terminator(IsoSectorSize, 0);
	terminator[0] = 255;
	put_text(terminator, 1, "CD001", 5);
	terminator[6] = 1;

	write_bytes(path, TerminatorSector * IsoSectorSize, terminator);

	// The root directory only contains the "." and ".." entries
	auto root_directory = root_record;
	const auto parent_record = make_iso_directory_record(RootDirectorySector,
	                                                     0x01);
	root_directory.insert(root_directory.end(),
	                      parent_record.begin(),
	                      parent_record.end());

	write_bytes(path, RootDirectorySector * IsoSectorSize, root_directory);
}

// CUE sheet referencing a single data track stored in a BIN file next to it
void write_cue_image(const std_fs::path& cue_path, const std::string& bin_filename,
                     const std::string_view volume_id)
{
	write_iso_image(cue_path.parent_path() / bin_filename, volume_id);

	std::ofstream cue(cue_path, std::ios::trunc);
	cue << "FILE \"" << bin_filename << "\" BINARY\n"
	    << "  TRACK 01 MODE1/2048\n"
	    << "    INDEX 01 00:00:00\n";
}

class MountDiskImageTest : public MountTest {
protected:
	void SetUp() override
	{
		MountTest::SetUp();

		// The drive manager outlives the DOS module, so drop any drives
		// left behind by other tests to keep the tests independent
		UnmountAllDrives();
	}

	void TearDown() override
	{
		UnmountAllDrives();

		if (original_home) {
			SetEnvVar("HOME", *original_home);
		}

		MountTest::TearDown();
	}

	static void UnmountAllDrives()
	{
		for (auto drive = 0; drive < DOS_DRIVES; ++drive) {
			if (!DriveManager::GetFilesystemImages(drive).empty()) {
				DriveManager::UnmountDrive(drive);
			}
		}
	}

	// Points the home directory (~) to the test files directory
	void SetHomeToTestFiles()
	{
		original_home = get_env_var("HOME");
		SetEnvVar("HOME", test_file_path.string());
	}

	// Returns `~/<name>` using the host's path separator
	static std::string HomePath(const std::string& name)
	{
		return std::string("~") + CROSS_FILESPLIT + name;
	}

	static std::shared_ptr<DOS_Drive> DriveAt(const char drive_letter)
	{
		return Drives.at(drive_index(drive_letter));
	}

	static std::shared_ptr<imageDisk> BiosDiskAt(const char drive_letter)
	{
		return imageDiskList.at(drive_index(drive_letter));
	}

	static void ExpectHddGeometry(imageDisk& disk)
	{
		uint32_t heads       = 0;
		uint32_t cylinders   = 0;
		uint32_t sectors     = 0;
		uint32_t sector_size = 0;
		disk.Get_Geometry(&heads, &cylinders, &sectors, &sector_size);

		EXPECT_EQ(heads, HddHeads);
		EXPECT_EQ(cylinders, HddCylinders);
		EXPECT_EQ(sectors, HddSectorsPerTrack);
		EXPECT_EQ(sector_size, SectorSize);
	}

	// Checks what the BOOT command relies on: a BIOS hard disk for the
	// drive with a bootable first sector
	static void ExpectBootableHdd(const char drive_letter)
	{
		const auto disk = BiosDiskAt(drive_letter);
		ASSERT_TRUE(disk);

		EXPECT_TRUE(disk->hardDrive);
		ExpectHddGeometry(*disk);

		std::array<uint8_t, SectorSize> first_sector = {};
		EXPECT_EQ(disk->Read_Sector(0, 0, 1, first_sector.data()), 0);
		EXPECT_EQ(first_sector[510], 0x55);
		EXPECT_EQ(first_sector[511], 0xaa);
	}

	static void ExpectFatHddMountedAsDriveC(const std::optional<MountParameters>& result)
	{
		ASSERT_TRUE(result.has_value());

		EXPECT_EQ(result->type, MountType::HardDiskImage);
		EXPECT_EQ(result->fstype, MountFileSystemType::Fat16);
		EXPECT_FALSE(result->roflag);

		const auto drive = DriveAt('C');
		ASSERT_TRUE(drive);
		EXPECT_EQ(drive->GetType(), DosDriveType::Fat);
		EXPECT_FALSE(drive->IsReadOnly());
		EXPECT_STREQ(drive->GetLabel(), "HDDLABEL");

		ExpectBootableHdd('C');
	}

	static void SetEnvVar(const char* name, const std::string& value)
	{
#if defined(WIN32)
		_putenv_s(name, value.c_str());
#else
		setenv(name, value.c_str(), 1);
#endif
	}

	std::optional<std::string> original_home = {};
};

// Bootable images
// ---------------------------------------------------------------------

TEST_F(MountDiskImageTest, BootableHddImageOnDriveNumberCanBeBootedAsDriveC)
{
	// mount 2 hd20.img -fs none -t hdd -size 512,63,16,40 -ro
	// boot c:
	write_hdd_image(P("hd20.img"), "HDDLABEL");

	// IMGMOUNT is a deprecated alias that must behave the same as MOUNT
	for (const auto* program : {"Z:\\MOUNT.COM", "Z:\\IMGMOUNT.COM"}) {
		SCOPED_TRACE(program);

		const auto result = Mount("2 " + P("hd20.img") + " -fs none -t hdd " +
		                                  HddSizeOption + " -ro",
		                          program);

		ASSERT_TRUE(result.has_value());

		EXPECT_EQ(result->drive, '2');
		EXPECT_TRUE(result->is_drive_number);
		EXPECT_EQ(result->type, MountType::HardDiskImage);
		EXPECT_EQ(result->fstype, MountFileSystemType::None);
		EXPECT_TRUE(result->roflag);

		// Raw mounts only attach a BIOS disk, not a DOS drive
		EXPECT_FALSE(DriveAt('C'));

		ExpectBootableHdd('C');

		// Writes to the read-only image must fail and leave it intact
		const auto disk = BiosDiskAt('C');
		ASSERT_TRUE(disk);

		std::array<uint8_t, SectorSize> zeroes = {};
		EXPECT_NE(disk->Write_AbsoluteSector(0, zeroes.data()), 0);

		const auto mbr_signature = read_bytes(P("hd20.img"), 510, 2);
		EXPECT_EQ(mbr_signature, (Bytes{0x55, 0xaa}));
	}
}

// Directory mounts
// ---------------------------------------------------------------------

TEST_F(MountDiskImageTest, DirectoryMountedAsDriveC)
{
	// mount c /home/user/DOS
	std_fs::create_directories(test_file_path / "DOS");

	const auto result = Mount("C " + P("DOS"));
	ASSERT_TRUE(result.has_value());

	const auto drive = DriveAt('C');
	ASSERT_TRUE(drive);

	EXPECT_EQ(drive->GetType(), DosDriveType::Local);
	EXPECT_EQ(drive->GetMediaByte(), MediaId::HardDisk);
	EXPECT_FALSE(drive->IsReadOnly());
	EXPECT_STREQ(drive->GetLabel(), "C_DRIVE");
}

TEST_F(MountDiskImageTest, DirectoryInHomeMountedAsDriveC)
{
	// mount c ~/DOS
	std_fs::create_directories(test_file_path / "DOS");
	SetHomeToTestFiles();

	const auto result = Mount("C " + HomePath("DOS"));
	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_TRUE(std_fs::equivalent(result->paths[0], test_file_path / "DOS"));

	const auto drive = std::dynamic_pointer_cast<localDrive>(DriveAt('C'));
	ASSERT_TRUE(drive);
	EXPECT_TRUE(std_fs::equivalent(drive->GetBasedir(), test_file_path / "DOS"));
}

TEST_F(MountDiskImageTest, DirectoryMountedAsFloppyDriveA)
{
	// mount a /home/user/floppydir
	std_fs::create_directories(test_file_path / "floppydir");

	const auto result = Mount("A " + P("floppydir"));
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::Directory);

	const auto drive = DriveAt('A');
	ASSERT_TRUE(drive);

	EXPECT_EQ(drive->GetType(), DosDriveType::Local);
	EXPECT_EQ(drive->GetMediaByte(), MediaId::Floppy1_44MB);
	EXPECT_STREQ(drive->GetLabel(), "A_DRIVE");
}

TEST_F(MountDiskImageTest, DirectoryMountedAsFloppyDriveAWithFloppyType)
{
	// mount a /home/user/floppydir -t floppy
	std_fs::create_directories(test_file_path / "floppydir");

	const auto result = Mount("A " + P("floppydir") + " -t floppy");
	ASSERT_TRUE(result.has_value());

	// The floppy type is kept, but the directory itself is mounted (not
	// an image)
	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_FALSE(result->is_image_mode);

	const auto drive = DriveAt('A');
	ASSERT_TRUE(drive);

	EXPECT_EQ(drive->GetType(), DosDriveType::Local);
	EXPECT_EQ(drive->GetMediaByte(), MediaId::Floppy1_44MB);
	EXPECT_STREQ(drive->GetLabel(), "A_FLOPPY");

	// Directory mounts provide no BIOS disk, so they can't be booted
	EXPECT_FALSE(BiosDiskAt('A'));
}

// Hard disk images
// ---------------------------------------------------------------------

TEST_F(MountDiskImageTest, HddImageWithSizeGeometryMountedAsDriveC)
{
	// mount c hd20.img -t hdd -size 512,63,16,40
	write_hdd_image(P("hd20.img"), "HDDLABEL");

	ExpectFatHddMountedAsDriveC(
	        Mount("C " + P("hd20.img") + " -t hdd " + HddSizeOption));
}

TEST_F(MountDiskImageTest, HddImageWithChsGeometryMountedAsDriveC)
{
	// mount c hd20.img -t hdd -chs 40,16,63
	write_hdd_image(P("hd20.img"), "HDDLABEL");

	ExpectFatHddMountedAsDriveC(
	        Mount("C " + P("hd20.img") + " -t hdd " + HddChsOption));
}

// CD-ROM images
// ---------------------------------------------------------------------

TEST_F(MountDiskImageTest, IsoImageAttachedToIdeController)
{
	// mount d cdrom.img -t iso -ide
	write_iso_image(P("cdrom.img"), "MYCDROM");

	const auto result = Mount("D " + P("cdrom.img") + " -t iso -ide");
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);
	EXPECT_EQ(result->fstype, MountFileSystemType::Iso);
	EXPECT_TRUE(result->is_ide);
	EXPECT_GE(result->ide_index, 0);

	const auto drive = DriveAt('D');
	ASSERT_TRUE(drive);
	EXPECT_EQ(drive->GetType(), DosDriveType::Iso);
	EXPECT_TRUE(drive->IsReadOnly());

	// The CD-ROM must be attached to the IDE cable slot MOUNT picked
	int8_t attached_index = -1;
	bool attached_slave   = false;
	IDE_CDROM_Detach_Ret(attached_index, attached_slave, drive_index('D'));

	EXPECT_EQ(attached_index, result->ide_index);
	EXPECT_EQ(attached_slave, result->is_second_cable_slot);
}

TEST_F(MountDiskImageTest, CueImageInCurrentDosDirectory)
{
	// cd TIECD
	// mount e SWTIECD.CUE -t cdrom
	write_cue_image(test_file_path / "TIECD" / "SWTIECD.CUE",
	                "SWTIECD.BIN",
	                "SWTIECD");

	ASSERT_TRUE(Mount("C " + P("")).has_value());
	ASSERT_TRUE(DOS_SetDrive(drive_index('C')));
	ASSERT_TRUE(DOS_ChangeDir("TIECD"));

	const auto result = Mount("E SWTIECD.CUE -t cdrom");
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::CdRomImage);

	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_TRUE(std_fs::equivalent(result->paths[0],
	                               test_file_path / "TIECD" / "SWTIECD.CUE"));

	const auto drive = DriveAt('E');
	ASSERT_TRUE(drive);
	EXPECT_EQ(drive->GetType(), DosDriveType::Iso);
}

// Floppy images
// ---------------------------------------------------------------------

TEST_F(MountDiskImageTest, FloppyImageInHomeMountedAsDriveA)
{
	// mount a ~/DOS/disk01.img -t floppy
	write_floppy_image(test_file_path / "DOS" / "disk01.img", "MYDISK");
	SetHomeToTestFiles();

	const auto result = Mount("A " + HomePath("DOS") + CROSS_FILESPLIT +
	                          "disk01.img -t floppy");
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, MountType::FloppyImage);
	EXPECT_EQ(FileNames(*result), (std::vector<std::string>{"disk01.img"}));

	const auto drive = std::dynamic_pointer_cast<fatDrive>(DriveAt('A'));
	ASSERT_TRUE(drive);
	EXPECT_FALSE(drive->IsReadOnly());
	EXPECT_EQ(drive->GetMediaByte(), MediaId::Floppy1_44MB);
	EXPECT_STREQ(drive->GetLabel(), "MYDISK");

	// The image is also available as the first BIOS floppy disk (for BOOT)
	ASSERT_TRUE(BiosDiskAt('A'));
	EXPECT_EQ(BiosDiskAt('A'), drive->loadedDisk);
	EXPECT_FALSE(BiosDiskAt('A')->hardDrive);
}

TEST_F(MountDiskImageTest, FloppyImageInHomeMountedAsDriveAWithLabel)
{
	// mount a ~/DOS/disk01.img -t floppy -label TEST
	write_floppy_image(test_file_path / "DOS" / "disk01.img", "MYDISK");
	SetHomeToTestFiles();

	const auto result = Mount("A " + HomePath("DOS") + CROSS_FILESPLIT +
	                          "disk01.img -t floppy -label TEST");
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->label, "TEST");

	const auto drive = DriveAt('A');
	ASSERT_TRUE(drive);
	EXPECT_EQ(drive->GetType(), DosDriveType::Fat);

	// TODO -label is only applied to directory mounts; FAT images keep
	// the volume label stored in the image
	EXPECT_STREQ(drive->GetLabel(), "MYDISK");
}

TEST_F(MountDiskImageTest, FloppyImageRelativeToConfigFile)
{
	// mount a subdir/disk01.img -t floppy -pr
	write_floppy_image(test_file_path / "subdir" / "disk01.img", "MYDISK");

	control->config_files = {(test_file_path / "dosbox.conf").string()};

	const auto result = Mount(std::string("A subdir") + CROSS_FILESPLIT +
	                          "disk01.img -t floppy -pr");
	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(result->paths.size(), 1);
	EXPECT_TRUE(std_fs::equivalent(result->paths[0],
	                               test_file_path / "subdir" / "disk01.img"));

	const auto drive = DriveAt('A');
	ASSERT_TRUE(drive);
	EXPECT_EQ(drive->GetType(), DosDriveType::Fat);
}

// Overlays
// ---------------------------------------------------------------------

TEST_F(MountDiskImageTest, OverlayOnReadOnlyFloppyImageIsRefused)
{
	// mount a ~/DOS/disk01.img -t floppy -ro
	// mount a ~/DOS/floppyoverlay -t overlay
	write_floppy_image(test_file_path / "DOS" / "disk01.img", "MYDISK");
	std_fs::create_directories(test_file_path / "DOS" / "floppyoverlay");
	SetHomeToTestFiles();

	const auto base = Mount("A " + HomePath("DOS") + CROSS_FILESPLIT +
	                        "disk01.img -t floppy -ro");
	ASSERT_TRUE(base.has_value());
	EXPECT_TRUE(base->roflag);

	const auto base_drive = DriveAt('A');
	ASSERT_TRUE(base_drive);
	EXPECT_EQ(base_drive->GetType(), DosDriveType::Fat);
	EXPECT_TRUE(base_drive->IsReadOnly());

	Mount("A " + HomePath("DOS") + CROSS_FILESPLIT + "floppyoverlay -t overlay");

	// Overlays need a directory mount as their base, so the overlay is
	// refused and the read-only floppy image stays mounted
	EXPECT_EQ(DriveAt('A'), base_drive);
	EXPECT_TRUE(DriveAt('A')->IsReadOnly());
}

} // namespace
