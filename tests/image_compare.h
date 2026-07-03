// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TESTS_IMAGE_COMPARE_H
#define DOSBOX_TESTS_IMAGE_COMPARE_H

// Golden-image comparison support for tests.
//
// Tests produce an image, then compare it against a checked-in reference
// PNG with ExpectMatchesReference():
//
//  - If the reference is missing, the produced image is written to the
//    expected location AND the test fails: inspect the new reference
//    visually, then commit it. The next run passes.
//
//  - If the images differ beyond the tolerance, the produced image is
//    written next to the reference with an `-ACTUAL.png` suffix (add
//    `*-ACTUAL.png` to the data directory's .gitignore) and the test
//    fails.
//
// Tests run with the project root as the working directory
// (gtest_discover_tests sets WORKING_DIRECTORY), so data directories are
// referenced as "tests/<name>_data/...".

#include <cstdint>
#include <string>
#include <vector>

#include "misc/std_filesystem.h"

struct DecodedImage {
	int width  = 0;
	int height = 0;

	// Tightly packed 8-bit RGB triplets, top-left origin
	std::vector<uint8_t> rgb = {};
};

// Decode a PNG into a flat RGB buffer (palette expanded, alpha dropped).
// Returns false on failure.
bool DecodePng(const std_fs::path& path, DecodedImage& out);

// Write a flat RGB buffer as an 8-bit truecolour PNG.
// Returns false on failure.
bool EncodePng(const std_fs::path& path, const DecodedImage& image);

// Compare two decoded images. Returns the fraction of pixels that differ
// (0.0 = identical, 1.0 = every pixel differs). Returns 1.0 (worst) on
// dimension mismatch.
float ComparePixels(const DecodedImage& a, const DecodedImage& b);

// Compare a produced image against `expected_dir/<reference_name>.png`;
// see the comment at the top of this header for the workflow.
// `tolerance` is the accepted fraction of differing pixels (0.0f = exact
// match).
void ExpectMatchesReference(const DecodedImage& actual,
                            const std_fs::path& expected_dir,
                            const std::string& reference_name,
                            const float tolerance);

// Wipe stale `-ACTUAL.png` diagnostics from a prior run. Call it from the
// test suite's SetUpTestSuite(); without this, an old failing test's
// actual image hangs around in `expected/` confusing inspection after a
// later pass.
void RemoveStaleActualImages(const std_fs::path& expected_dir);

#endif // DOSBOX_TESTS_IMAGE_COMPARE_H
