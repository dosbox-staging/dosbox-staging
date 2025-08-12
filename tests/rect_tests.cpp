// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdio>

#include "utils/rect.h"

#include <gtest/gtest.h>

constexpr auto AbsError = 0.000001f;

// Construction
//
TEST(Rect, constructor_default)
{
	const DosBox::Rect r = {};

	EXPECT_EQ(r.x, 0.0f);
	EXPECT_EQ(r.y, 0.0f);
	EXPECT_EQ(r.w, 0.0f);
	EXPECT_EQ(r.h, 0.0f);
}

TEST(Rect, constructor_float)
{
	const DosBox::Rect r = {1.1f, 5.5f, 2.3f, 8.4f};

	EXPECT_EQ(r.x, 1.1f);
	EXPECT_EQ(r.y, 5.5f);
	EXPECT_EQ(r.w, 2.3f);
	EXPECT_EQ(r.h, 8.4f);
}

TEST(Rect, constructor_int)
{
	const DosBox::Rect r = {1, 5, 2, 8};

	EXPECT_EQ(r.x, 1.0f);
	EXPECT_EQ(r.y, 5.0f);
	EXPECT_EQ(r.w, 2.0f);
	EXPECT_EQ(r.h, 8.0f);
}

TEST(Rect, constructor_size_only_float)
{
	const DosBox::Rect r = {2.3f, 8.4f};

	EXPECT_EQ(r.x, 0.0f);
	EXPECT_EQ(r.y, 0.0f);
	EXPECT_EQ(r.w, 2.3f);
	EXPECT_EQ(r.h, 8.4f);
}

TEST(Rect, constructor_size_only_int)
{
	const DosBox::Rect r = {2, 8};

	EXPECT_EQ(r.x, 0.0f);
	EXPECT_EQ(r.y, 0.0f);
	EXPECT_EQ(r.w, 2.0f);
	EXPECT_EQ(r.h, 8.0f);
}

TEST(Rect, constructor_zero_size_allowed)
{
	const DosBox::Rect r1 = {1.1f, 5.5f, 0.0f, 0.0f};

	EXPECT_EQ(r1.x, 1.1f);
	EXPECT_EQ(r1.y, 5.5f);
	EXPECT_EQ(r1.w, 0.0f);
	EXPECT_EQ(r1.h, 0.0f);

	const DosBox::Rect r2 = {0.0f, 0.0f};

	EXPECT_EQ(r2.x, 0.0f);
	EXPECT_EQ(r2.y, 0.0f);
	EXPECT_EQ(r2.w, 0.0f);
	EXPECT_EQ(r2.h, 0.0f);
}

TEST(Rect, constructor_negative_size_allowed)
{
	const DosBox::Rect r = {1.1f, 5.5f, -2.3f, -8.4f};

	EXPECT_EQ(r.x, 1.1f);
	EXPECT_EQ(r.y, 5.5f);
	EXPECT_EQ(r.w, -2.3f);
	EXPECT_EQ(r.h, -8.4f);
}

// Getters
//
TEST(Rect, coordinate_getters)
{
	const DosBox::Rect r = {1.1f, 5.5f, 2.3f, 8.4f};

	EXPECT_EQ(r.x1(), 1.1f);
	EXPECT_EQ(r.y1(), 5.5f);
	EXPECT_EQ(r.x2(), 3.4f);
	EXPECT_EQ(r.y2(), 13.9f);
}

TEST(Rect, center_getters)
{
	const DosBox::Rect r = {1.1f, 5.5f, 2.3f, 8.4f};

	EXPECT_EQ(r.cx(), 2.25f);
	EXPECT_EQ(r.cy(), 9.7f);
}

// Equality
//
TEST(Rect, equality)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, 8.4f};
	const DosBox::Rect b = {0.0f, 3.4f, 1.2f, 3.5f};

	EXPECT_EQ(a, a);
	EXPECT_NE(a, b);
}

// IsEmpty()
//
TEST(Rect, IsEmpty_zero_width)
{
	const DosBox::Rect r = {0.0f, 8.4f};
	EXPECT_TRUE(r.IsEmpty());
}

TEST(Rect, IsEmpty_zero_height)
{
	const DosBox::Rect r = {2.3f, 0.0f};
	EXPECT_TRUE(r.IsEmpty());
}

TEST(Rect, IsEmpty_zero_size)
{
	const DosBox::Rect r = {};
	EXPECT_TRUE(r.IsEmpty());
}

TEST(Rect, IsEmpty_negative_width)
{
	const DosBox::Rect r = {-2.3f, 8.4f};
	EXPECT_FALSE(r.IsEmpty());
}

TEST(Rect, IsEmpty_negative_height)
{
	const DosBox::Rect r = {2.3f, -8.4f};
	EXPECT_FALSE(r.IsEmpty());
}

TEST(Rect, IsEmpty_negative_size)
{
	const DosBox::Rect r = {-2.3f, -8.4f};
	EXPECT_FALSE(r.IsEmpty());
}

TEST(Rect, IsEmpty_positive_size)
{
	const DosBox::Rect r = {2.3f, 8.4f};
	EXPECT_FALSE(r.IsEmpty());
}

// HasPositiveSize()
//
TEST(Rect, HasPositiveSize_zero_size)
{
	const DosBox::Rect r = {};
	EXPECT_FALSE(r.HasPositiveSize());
}

TEST(Rect, HasPositiveSize_negative_width)
{
	const DosBox::Rect r = {-2.3f, 8.4f};
	EXPECT_FALSE(r.HasPositiveSize());
}

TEST(Rect, HasPositiveSize_negative_height)
{
	const DosBox::Rect r = {2.3f, -8.4f};
	EXPECT_FALSE(r.HasPositiveSize());
}

TEST(Rect, HasPositiveSize_negative_size)
{
	const DosBox::Rect r = {-2.3f, -8.4f};
	EXPECT_FALSE(r.HasPositiveSize());
}

TEST(Rect, HasPositiveSize_positive_size)
{
	const DosBox::Rect r = {2.3f, 8.4f};
	EXPECT_TRUE(r.HasPositiveSize());
}

// HasNegativeSize()
//
TEST(Rect, HasNegativeSize_zero_size)
{
	const DosBox::Rect r = {};
	EXPECT_FALSE(r.HasNegativeSize());
}

TEST(Rect, HasNegativeSize_negative_width)
{
	const DosBox::Rect r = {-2.3f, 8.4f};
	EXPECT_TRUE(r.HasNegativeSize());
}

TEST(Rect, HasNegativeSize_negative_height)
{
	const DosBox::Rect r = {2.3f, -8.4f};
	EXPECT_TRUE(r.HasNegativeSize());
}

TEST(Rect, HasNegativeSize_negative_size)
{
	const DosBox::Rect r = {-2.3f, -8.4f};
	EXPECT_TRUE(r.HasNegativeSize());
}

TEST(Rect, HasNegativeSize_positive_size)
{
	const DosBox::Rect r = {2.3f, 8.4f};
	EXPECT_FALSE(r.HasNegativeSize());
}

// IsExistant()
//
TEST(Rect, IsExistant_zero_width)
{
	const DosBox::Rect r = {0.0f, 8.4f};
	EXPECT_TRUE(r.IsExistant());
}

TEST(Rect, IsExistant_zero_height)
{
	const DosBox::Rect r = {2.3f, 0.0f};
	EXPECT_TRUE(r.IsExistant());
}

TEST(Rect, IsExistant_zero_size)
{
	const DosBox::Rect r = {};
	EXPECT_TRUE(r.IsExistant());
}

TEST(Rect, IsExistant_negative_width)
{
	const DosBox::Rect r = {-2.3f, 8.4f};
	EXPECT_FALSE(r.IsExistant());
}

TEST(Rect, IsExistant_negative_height)
{
	const DosBox::Rect r = {2.3f, -8.4f};
	EXPECT_FALSE(r.IsExistant());
}

TEST(Rect, IsExistant_negative_size)
{
	const DosBox::Rect r = {-2.3f, -8.4f};
	EXPECT_FALSE(r.IsExistant());
}

TEST(Rect, IsExistant_positive_size)
{
	const DosBox::Rect r = {2.3f, 8.4f};
	EXPECT_TRUE(r.IsExistant());
}

// Copy()
//
TEST(Rect, Copy)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, 8.4f};
	auto b               = a.Copy();

	EXPECT_EQ(b, a);

	b.w = 0.0f;
	EXPECT_NE(b, a);
}

// Normalise()
//
TEST(Rect, Normalise_zero_size)
{
	const DosBox::Rect a = {};
	const auto b         = a.Copy().Normalise();

	EXPECT_EQ(b, a);
}

TEST(Rect, Normalise_positive_size)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, 8.4f};
	const auto b         = a.Copy().Normalise();

	EXPECT_EQ(b, a);
}

TEST(Rect, Normalise_negative_size)
{
	const DosBox::Rect a = {1.1f, 5.5f, -2.3f, -8.4f};
	const auto b         = a.Copy().Normalise();

	EXPECT_NEAR(b.x, -1.2f, AbsError);
	EXPECT_NEAR(b.y, -2.9f, AbsError);
	EXPECT_EQ(b.w, -a.w);
	EXPECT_EQ(b.h, -a.h);
}

// Scale()
//
TEST(Rect, Scale_zero_size)
{
	const DosBox::Rect a = {};
	const auto b         = a.Copy().Scale(2.0f);

	EXPECT_EQ(b, a);
}

TEST(Rect, Scale_positive)
{
	DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	a.Scale(2.0f);

	const DosBox::Rect expected = {2.2f, 11.0f, 4.6f, -16.8f};
	EXPECT_EQ(a, expected);
}

TEST(Rect, Scale_negative)
{
	DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	a.Scale(-2.0f);

	const DosBox::Rect expected = {-2.2f, -11.0f, -4.6f, 16.8f};
	EXPECT_EQ(a, expected);
}

TEST(Rect, Scale_zero)
{
	DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	a.Scale(0.0f);

	const DosBox::Rect expected = {};
	EXPECT_EQ(a, expected);
}

// ScaleSize()
//
TEST(Rect, ScaleSize_zero_size)
{
	const DosBox::Rect a = {};
	const auto b         = a.Copy().Scale(2.0f);

	EXPECT_EQ(b, a);
}
TEST(Rect, ScaleSize_positive)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleSize(2.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, 4.6f);
	EXPECT_EQ(b.h, -16.8f);
}

TEST(Rect, ScaleSize_negative)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleSize(-2.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, -4.6f);
	EXPECT_EQ(b.h, 16.8f);
}

TEST(Rect, ScaleSize_zero)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleSize(0.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, 0.0f);
	EXPECT_EQ(b.h, 0.0f);
}

// ScaleWidth()
//
TEST(Rect, ScaleWidth_positive)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleWidth(2.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, 4.6f);
	EXPECT_EQ(b.h, a.h);
}

TEST(Rect, ScaleWidth_negative)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleWidth(-2.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, -4.6f);
	EXPECT_EQ(b.h, a.h);
}

TEST(Rect, ScaleWidth_zero)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleWidth(0.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, 0.0f);
	EXPECT_EQ(b.h, a.h);
}

// ScaleHeight()
//
TEST(Rect, ScaleHeight_positive)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleHeight(2.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, a.w);
	EXPECT_EQ(b.h, -16.8f);
}

TEST(Rect, ScaleHeight_negative)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleHeight(-2.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, a.w);
	EXPECT_EQ(b.h, 16.8f);
}

TEST(Rect, ScaleHeight_zero)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().ScaleHeight(0.0f);

	EXPECT_EQ(b.x, a.x);
	EXPECT_EQ(b.y, a.y);
	EXPECT_EQ(b.w, a.w);
	EXPECT_EQ(b.h, 0.0f);
}

// Translate()
//
TEST(Rect, Translate)
{
	const DosBox::Rect a = {1.1f, 5.5f, 2.3f, -8.4f};
	const auto b         = a.Copy().Translate(-2.2f, 3.3f);

	EXPECT_EQ(b.x, -1.1f);
	EXPECT_EQ(b.y, 8.8f);
	EXPECT_EQ(b.w, a.w);
	EXPECT_EQ(b.h, a.h);
}

// CenterTo()
//
TEST(Rect, CenterTo_zero_size)
{
	auto test = [] {
		const DosBox::Rect a = {1.1f, 5.5f, 0.0f, 0.0f};
		a.Copy().CenterTo(10.0f, -12.0f);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, CenterTo_positive_size)
{
	const DosBox::Rect a = {1.1f, 5.5f, 4.6f, 8.4f};
	const auto b         = a.Copy().CenterTo(10.0f, -12.0f);

	EXPECT_EQ(b.x, 7.7f);
	EXPECT_EQ(b.y, -16.2f);
	EXPECT_EQ(b.w, a.w);
	EXPECT_EQ(b.h, a.h);
}

TEST(Rect, CenterTo_negative_size)
{
	auto test = [] {
		const DosBox::Rect a = {1.1f, 5.5f, 4.6f, -8.4f};
		a.Copy().CenterTo(10.0f, -12.0f);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

// Contains()
//
TEST(Rect, Contains_self_non_empty)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	EXPECT_TRUE(a.Contains(a));
}

TEST(Rect, Contains_self_empty)
{
	const DosBox::Rect empty = {};
	EXPECT_FALSE(empty.Contains(empty));
}

TEST(Rect, Contains_contained)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {4.0f, 4.0f, 2.0f, 2.0f};

	EXPECT_TRUE(a.Contains(b));
}

TEST(Rect, Contains_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {4.0f, 4.0f, 9.0f, 12.0f};

	EXPECT_FALSE(a.Contains(b));
}

TEST(Rect, Contains_non_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {14.0f, 4.0f, 2.0f, 2.0f};

	EXPECT_FALSE(a.Contains(b));
}

TEST(Rect, Contains_touching)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {7.0f, 3.0f, 5.0f, 4.0f};

	EXPECT_FALSE(a.Contains(b));
}

TEST(Rect, Contains_source_zero_size)
{
	const DosBox::Rect a = {};
	const DosBox::Rect b = {1.1f, 5.5f};

	EXPECT_FALSE(a.Contains(b));
}

TEST(Rect, Contains_target_zero_size)
{
	const DosBox::Rect a = {1.1f, 5.5f};
	const DosBox::Rect b = {};

	EXPECT_TRUE(a.Contains(b));
}

TEST(Rect, Contains_source_negative_size)
{
	auto test = [] {
		const DosBox::Rect a = {-1.0f, -2.0f};
		const DosBox::Rect b = {1.1f, 5.5f};
		a.Contains(b);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, Contains_target_negative_size)
{
	auto test = [] {
		const DosBox::Rect a = {1.0f, 2.0f};
		const DosBox::Rect b = {1.1f, -5.5f};
		a.Contains(b);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

// Overlaps()
//
TEST(Rect, Overlaps_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {4.0f, 4.0f, 2.0f, 2.0f};

	EXPECT_TRUE(a.Overlaps(b));
}

TEST(Rect, Overlaps_non_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {14.0f, 4.0f, 2.0f, 2.0f};

	EXPECT_FALSE(a.Overlaps(b));
}

TEST(Rect, Overlaps_touching)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {7.0f, 3.0f, 5.0f, 4.0f};

	EXPECT_FALSE(a.Overlaps(b));
}

TEST(Rect, Overlaps_target_zero_size_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {3.0f, 4.0f, 0.0f, 0.0f};

	EXPECT_FALSE(a.Overlaps(b));
}

TEST(Rect, Overlaps_target_zero_size_non_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {13.0f, 4.0f, 0.0f, 0.0f};

	EXPECT_FALSE(a.Overlaps(b));
}

TEST(Rect, Overlaps_source_zero_size)
{
	const DosBox::Rect a = {};
	const DosBox::Rect b = {3.0f, 4.0f, 0.0f, 0.0f};

	EXPECT_FALSE(a.Overlaps(b));
}

TEST(Rect, Overlaps_source_negative_size)
{
	auto test = [] {
		const DosBox::Rect a = {-1.0f, -2.0f};
		const DosBox::Rect b = {1.1f, 5.5f};
		a.Overlaps(b);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, Overlaps_target_negative_size)
{
	auto test = [] {
		const DosBox::Rect a = {1.0f, 2.0f};
		const DosBox::Rect b = {1.1f, -5.5f};
		a.Overlaps(b);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

// Intersect()
//
TEST(Rect, Intersect_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.5f, 7.0f, 4.0f};
	const DosBox::Rect b = {4.0f, 4.5f, 12.0f, 6.0f};

	const DosBox::Rect expected = {4.0f, 4.5, 5.0f, 3.0f};
	EXPECT_EQ(a.Copy().Intersect(b), expected);
}

TEST(Rect, Intersect_non_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {14.0f, 4.0f, 2.0f, 2.0f};

	const DosBox::Rect expected = {};
	EXPECT_EQ(a.Copy().Intersect(b), expected);
}

TEST(Rect, Intersect_touching)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {7.0f, 3.0f, 5.0f, 4.0f};

	const DosBox::Rect expected = {};
	EXPECT_EQ(a.Copy().Intersect(b), expected);
}

TEST(Rect, Intersect_target_zero_size_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {3.0f, 4.0f, 0.0f, 0.0f};

	const DosBox::Rect expected = {};
	EXPECT_EQ(a.Copy().Intersect(b), expected);
}

TEST(Rect, Intersect_target_zero_size_non_overlapping)
{
	const DosBox::Rect a = {2.0f, 3.0f, 5.0f, 4.0f};
	const DosBox::Rect b = {13.0f, 4.0f, 0.0f, 0.0f};

	const DosBox::Rect expected = {};
	EXPECT_EQ(a.Copy().Intersect(b), expected);
}

TEST(Rect, Intersect_source_zero_size)
{
	const DosBox::Rect a = {};
	const DosBox::Rect b = {3.0f, 4.0f, 0.0f, 0.0f};

	const DosBox::Rect expected = {};
	EXPECT_EQ(a.Copy().Intersect(b), expected);
}

TEST(Rect, Intersect_source_negative_size)
{
	auto test = [] {
		const DosBox::Rect b = {1.1f, 5.5f};

		DosBox::Rect a = {-1.0f, -2.0f};
		a.Intersect(b);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, Intersect_target_negative_size)
{
	auto test = [] {
		const DosBox::Rect b = {1.1f, -5.5f};

		DosBox::Rect a = {1.0f, 2.0f};
		a.Intersect(b);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

// ScaleSizeToFit()
//
TEST(Rect, ScaleSizeToFit_target_wider_and_larger)
{
	const DosBox::Rect src  = {1.1f, 2.2f, 4.0f, 3.0};
	const DosBox::Rect dest = {0.0f, -15.0f, 100.0f, 9.0};

	const auto r = src.Copy().ScaleSizeToFit(dest);

	EXPECT_EQ(r.x, src.x);
	EXPECT_EQ(r.y, src.y);
	EXPECT_EQ(r.w, 12.0f);
	EXPECT_EQ(r.h, 9.0f);
}

TEST(Rect, ScaleSizeToFit_target_taller_and_smaller)
{
	const DosBox::Rect src  = {1.1f, 2.2f, 4.0f, 3.0};
	const DosBox::Rect dest = {0.0f, -15.0f, 2.0f, 9.0};

	const auto r = src.Copy().ScaleSizeToFit(dest);

	EXPECT_EQ(r.x, src.x);
	EXPECT_EQ(r.y, src.y);
	EXPECT_EQ(r.w, 2.0f);
	EXPECT_EQ(r.h, 1.5f);
}

TEST(Rect, ScaleSizeToFit_source_zero_size)
{
	auto test = [] {
		const DosBox::Rect dest = {1.1f, 5.5f};

		DosBox::Rect src = {};
		src.ScaleSizeToFit(dest);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, ScaleSizeToFit_target_zero_size)
{
	auto test = [] {
		const DosBox::Rect dest = {};

		DosBox::Rect src = {1.0f, 2.0f};
		src.ScaleSizeToFit(dest);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, ScaleSizeToFit_source_negative_size)
{
	auto test = [] {
		const DosBox::Rect dest = {1.1f, 5.5f};

		DosBox::Rect src = {-1.0f, -2.0f};
		src.ScaleSizeToFit(dest);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

TEST(Rect, ScaleSizeToFit_target_negative_size)
{
	auto test = [] {
		const DosBox::Rect dest = {1.1f, -5.5f};

		DosBox::Rect src = {1.0f, 2.0f};
		src.ScaleSizeToFit(dest);
	};
	EXPECT_DEBUG_DEATH(test(), "");
}

// ToString()
//
TEST(Rect, ToString)
{
	const DosBox::Rect a = {0.0f, -3.0f, 5.5f, (11.0f / 7.0f)};

	const std::string expected = "{x: 0, y: -3, w: 5.5, h: 1.57143}";
	EXPECT_EQ(a.ToString(), expected);
}
