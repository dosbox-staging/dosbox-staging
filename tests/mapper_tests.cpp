// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/mapper.h"

#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

constexpr int MaxAxes = 10;

SDL_GamepadBinding axis_binding(const int axis, const int axis_min,
                                const int axis_max, const SDL_GamepadAxis output)
{
	SDL_GamepadBinding binding = {};

	binding.input_type          = SDL_GAMEPAD_BINDTYPE_AXIS;
	binding.input.axis.axis     = axis;
	binding.input.axis.axis_min = axis_min;
	binding.input.axis.axis_max = axis_max;

	binding.output_type          = SDL_GAMEPAD_BINDTYPE_AXIS;
	binding.output.axis.axis     = output;
	binding.output.axis.axis_min = SDL_JOYSTICK_AXIS_MIN;
	binding.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
	return binding;
}

SDL_GamepadBinding full_axis_binding(const int axis, const SDL_GamepadAxis output)
{
	return axis_binding(axis, SDL_JOYSTICK_AXIS_MIN, SDL_JOYSTICK_AXIS_MAX, output);
}

// Holds the bindings and the pointers to them, the way
// SDL_GetGamepadBindings() returns them
class Bindings {
public:
	explicit Bindings(std::vector<SDL_GamepadBinding> bindings)
	        : bindings(std::move(bindings))
	{
		for (auto& binding : this->bindings) {
			pointers.push_back(&binding);
		}
	}

	// The pointers refer into 'bindings'
	Bindings(const Bindings&)            = delete;
	Bindings& operator=(const Bindings&) = delete;

	std::optional<int> FindAxis(const SDL_GamepadAxis gamepad_axis,
	                            const int num_axes = MaxAxes) const
	{
		return MAPPER_FindFullRangeAxis(pointers, gamepad_axis, num_axes);
	}

private:
	std::vector<SDL_GamepadBinding> bindings  = {};
	std::vector<SDL_GamepadBinding*> pointers = {};
};

// The axes of an Xbox Series X controller over Bluetooth as SDL3 reports
// them on Windows
const Bindings XboxBindings({
        full_axis_binding(0, SDL_GAMEPAD_AXIS_LEFTX),
        full_axis_binding(1, SDL_GAMEPAD_AXIS_LEFTY),
        full_axis_binding(2, SDL_GAMEPAD_AXIS_LEFT_TRIGGER),
        full_axis_binding(3, SDL_GAMEPAD_AXIS_RIGHTX),
        full_axis_binding(4, SDL_GAMEPAD_AXIS_RIGHTY),
        full_axis_binding(5, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER),
});

TEST(MAPPER_FindFullRangeAxis, sticks_and_triggers)
{
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX), 0);
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTY), 1);
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), 2);
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHTX), 3);
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHTY), 4);
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), 5);
}

TEST(MAPPER_FindFullRangeAxis, buttons_and_hats_are_no_axes)
{
	SDL_GamepadBinding button = full_axis_binding(0, SDL_GAMEPAD_AXIS_LEFTX);
	button.input_type   = SDL_GAMEPAD_BINDTYPE_BUTTON;
	button.input.button = 1;

	SDL_GamepadBinding hat = full_axis_binding(0, SDL_GAMEPAD_AXIS_LEFTY);
	hat.input_type         = SDL_GAMEPAD_BINDTYPE_HAT;
	hat.input.hat.hat      = 0;
	hat.input.hat.hat_mask = SDL_HAT_UP;

	const Bindings bindings({button, hat});
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX), std::nullopt);
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTY), std::nullopt);
}

TEST(MAPPER_FindFullRangeAxis, button_outputs_are_skipped)
{
	// SDL_GAMEPAD_BUTTON_SOUTH and SDL_GAMEPAD_AXIS_LEFTX are both 0
	SDL_GamepadBinding button_output = full_axis_binding(7, SDL_GAMEPAD_AXIS_LEFTX);
	button_output.output_type   = SDL_GAMEPAD_BINDTYPE_BUTTON;
	button_output.output.button = SDL_GAMEPAD_BUTTON_SOUTH;

	const Bindings bindings({button_output});
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX), std::nullopt);
}

TEST(MAPPER_FindFullRangeAxis, half_axes_are_no_full_axes)
{
	const Bindings bindings({
	        axis_binding(2, 0, SDL_JOYSTICK_AXIS_MAX, SDL_GAMEPAD_AXIS_LEFT_TRIGGER),
	        axis_binding(5, 0, SDL_JOYSTICK_AXIS_MIN, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER),
	});
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), std::nullopt);
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), std::nullopt);
}

TEST(MAPPER_FindFullRangeAxis, inverted_axis)
{
	const Bindings bindings({
	        axis_binding(4, SDL_JOYSTICK_AXIS_MAX, SDL_JOYSTICK_AXIS_MIN, SDL_GAMEPAD_AXIS_RIGHTY),
	});
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHTY), 4);
}

TEST(MAPPER_FindFullRangeAxis, missing_binding)
{
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_INVALID), std::nullopt);

	const Bindings no_bindings({});
	EXPECT_EQ(no_bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX), std::nullopt);
}

TEST(MAPPER_FindFullRangeAxis, axis_numbers_up_to_num_axes)
{
	const Bindings bindings({full_axis_binding(12, SDL_GAMEPAD_AXIS_LEFTX)});
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX), std::nullopt);
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX, 16), 12);

	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHTX, 3), std::nullopt);
	EXPECT_EQ(XboxBindings.FindAxis(SDL_GAMEPAD_AXIS_RIGHTX, 4), 3);
}

TEST(MAPPER_FindFullRangeAxis, full_range_binding_after_a_half_axis)
{
	const Bindings bindings({
	        axis_binding(0, 0, SDL_JOYSTICK_AXIS_MAX, SDL_GAMEPAD_AXIS_LEFTX),
	        full_axis_binding(6, SDL_GAMEPAD_AXIS_LEFTX),
	});
	EXPECT_EQ(bindings.FindAxis(SDL_GAMEPAD_AXIS_LEFTX), 6);
}

} // namespace
