// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/programs/mount.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "dosbox_test_fixture.h"

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

	static std::optional<MountParameters> Mount(const std::string& command_params)
	{
		auto cmd     = new CommandLine("Z:\\MOUNT.COM", command_params);
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
	const auto result = Mount("C " + P("bootable.img") + " -t hdd -chs notnumbers");
	EXPECT_FALSE(result.has_value());
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "overlay");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "floppy");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "none");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "none");
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "floppy");
	EXPECT_EQ(result->fstype, "none");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "none");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);
}

TEST_F(MountTest, LetterAtoDRemapsToDriveNumberWithFsNone)
{
	const auto result = Mount("C " + P("bootable.img") + " -fs none");

	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->is_drive_number);

	EXPECT_EQ(result->drive, '2'); // C -> drive number 2
	                               //
	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "none");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_EQ(result->sizes[0], 512);
	EXPECT_EQ(result->sizes[1], 32);
	EXPECT_EQ(result->sizes[2], 32765);
	EXPECT_EQ(result->sizes[3], 16000);
}

// ---------------------------------------------------------------------
// -t flag aliasing
// ---------------------------------------------------------------------

TEST_F(MountTest, CdromAliasesToIso)
{
	const auto result = Mount("R " + P("image.iso") + " -t cdrom");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "iso");
}

TEST_F(MountTest, FddAliasesToFloppy)
{
	const auto result = Mount("B " + P("raw.dat") + " -t fdd");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "floppy");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "none");
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
	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
}

TEST_F(MountTest, AutoDetectsHddTypeFromImgExtension)
{
	const auto result = Mount("U " + P("image.img"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "hdd");
}

TEST_F(MountTest, ExplicitTypeOverridesExtensionAutoDetection)
{
	const auto result = Mount("V " + P("image.img") + " -t iso");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "iso");
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
	EXPECT_EQ(result->type, "floppy");
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

TEST_F(MountTest, IdeFlagAsStringValueAlsoSetsIsIde)
{
	const auto result = Mount("3 " + P("bootable.img") +
	                          " -t hdd -size 512,63,16,100 -ide 1");
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->is_ide);
}

// ---------------------------------------------------------------------
// Extension-based auto-detection when no -t is given
// ---------------------------------------------------------------------

TEST_F(MountTest, AutoDetectsIsoFromCueExtension)
{
	const auto result = Mount("E " + P("image.cue"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
}

TEST_F(MountTest, AutoDetectsIsoFromBinExtension)
{
	const auto result = Mount("E " + P("image.bin"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
}

TEST_F(MountTest, AutoDetectsIsoFromMdsExtension)
{
	const auto result = Mount("E " + P("image.mds"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
}

TEST_F(MountTest, AutoDetectsIsoFromCcdExtension)
{
	const auto result = Mount("E " + P("image.ccd"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
}

TEST_F(MountTest, AutoDetectsHddFromImaExtension)
{
	const auto result = Mount("E " + P("image.ima"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "fat");
}

TEST_F(MountTest, AutoDetectsHddFromVhdExtension)
{
	const auto result = Mount("F " + P("image.vhd"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "dir");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "none");
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

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "fat");
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

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

	EXPECT_EQ(result->type, "floppy");
	EXPECT_EQ(result->fstype, "iso");

	// MediaId promotion only happens for floppy+fat.
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

TEST_F(MountTest, ExplicitIsoTypeOverridesFloppyExtension)
{
	const auto result = Mount("D " + P("bootable.img") + " -t iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

	EXPECT_EQ(result->type, "floppy");
	EXPECT_EQ(result->fstype, "fat");

	// Pin down whatever the parser currently does.
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

TEST_F(MountTest, ExplicitIsoFilesystemWithoutType)
{
	const auto result = Mount("D " + P("bootable.img") + " -fs iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "iso");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

TEST_F(MountTest, ExplicitFatFilesystemWithoutType)
{
	const auto result = Mount("D " + P("bootable.img") + " -fs fat");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "fat");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

TEST_F(MountTest, ExplicitIsoFilesystemOnIsoImage)
{
	const auto result = Mount("D " + P("image.iso") + " -fs iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);
}

// ---------------------------------------------------------------------
// Image geometry oddities
// ---------------------------------------------------------------------

TEST_F(MountTest, SizeAcceptedForIsoMount)
{
	const auto result = Mount("X " + P("image.iso") + " -t iso -size 512,63,16,99");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "fat");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	ASSERT_EQ(result->paths.size(), 2);
}

TEST_F(MountTest, FirstImageControlsAutoDetectionReverseOrder)
{
	const auto result = Mount("X " + P("image.iso") + " " + P("image.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	ASSERT_EQ(result->paths.size(), 2);
}

TEST_F(MountTest, FirstIsoImageControlsAutoDetection)
{
	const auto result = Mount("D " + P("image.iso") + " " + P("bootable.img"));

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	ASSERT_EQ(result->paths.size(), 2);
}

// ---------------------------------------------------------------------
// IDE interactions
// ---------------------------------------------------------------------

TEST_F(MountTest, IdeFlagDoesNotAllocateControllerForNonIsoType)
{
	const auto result = Mount("D " + P("bootable.img") +
	                          " -t hdd -fs iso -ide -size 512,63,16,100");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "hdd");
	EXPECT_EQ(result->fstype, "iso");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	EXPECT_TRUE(result->is_ide);
	EXPECT_EQ(result->ide_index, -1);
	EXPECT_FALSE(result->is_second_cable_slot);
}

// ---------------------------------------------------------------------
// Geometry precedence with ISO images
// ---------------------------------------------------------------------

TEST_F(MountTest, ExplicitSizeWithIsoImage)
{
	const auto result = Mount("D " + P("image.iso") + " -size 512,63,16,99");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
	EXPECT_EQ(result->mediaid, MediaId::HardDisk);

	// Pin down whether ISO defaults or explicit size wins.
}

TEST_F(MountTest, ExplicitChsWithIsoImage)
{
	const auto result = Mount("D " + P("image.iso") + " -chs 123,8,17");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "iso");
	EXPECT_EQ(result->fstype, "iso");
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

// ---------------------------------------------------------------------
// Duplicate option precedence
// ---------------------------------------------------------------------

TEST_F(MountTest, DuplicateLabelFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -label FIRST"
	                          " -label SECOND");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->label, "FIRST");
}

TEST_F(MountTest, DuplicateSizeFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -size 1,2,3,4"
	                          " -size 5,6,7,8");

	ASSERT_TRUE(result.has_value());

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

	EXPECT_EQ(result->fstype, "fat");
}

TEST_F(MountTest, DuplicateTypeFirstWins)
{
	const auto result = Mount("X " + P("image.img") +
	                          " -t floppy"
	                          " -t iso");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->type, "floppy");
	EXPECT_EQ(result->fstype, "fat");
	EXPECT_EQ(result->mediaid, MediaId::Floppy1_44MB);
}

} // namespace
