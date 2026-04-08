# Joystick


## Configuration settings

You can set the joystick parameters in the `[joystick]` configuration
section.


##### autofire

:   Fire continuously as long as the button is pressed.

    Possible values: `on`, `off` *default*{ .default }


##### buttonwrap

:   Enable button wrapping at the number of emulated buttons.

    Possible values: `on`, `off` *default*{ .default }


##### circularinput

:   Enable translation of circular input to square output. Try enabling this
    if your left analog stick can only move in a circle.

    Possible values: `on`, `off` *default*{ .default }


##### deadzone

:   Percentage of motion to ignore (`10` by default). Valid range is 0 to
    100. 100 turns the stick into a digital one.


##### joy_x_calibration

:   Apply X-axis calibration parameters from the hotkeys (`auto` by
    default).


##### joy_y_calibration

:   Apply Y-axis calibration parameters from the hotkeys (`auto` by
    default).


##### joysticktype

:   Type of joystick to emulate.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Detect and use any joystick(s), if
      possible. Joystick emulation is disabled if no joystick is found.
    - `2axis` -- Support up to two joysticks, each with 2 axes.
    - `4axis` -- Support the first joystick only, as a 4-axis type.
    - `4axis_2` -- Support the second joystick only, as a 4-axis type.
    - `fcs` -- Emulate joystick as an original Thrustmaster FCS.
    - `ch` -- Emulate joystick as an original CH Flightstick.
    - `hidden` -- Prevent DOS from seeing the joystick(s), but enable them
      for mapping.
    - `disabled` -- Fully disable joysticks: won't be polled, mapped, or
      visible in DOS.

    </div>

    !!! note

        Remember to reset DOSBox's mapperfile if you saved it earlier.


##### swap34

:   Swap the 3rd and the 4th axis. Can be useful for certain joysticks.

    Possible values: `on`, `off` *default*{ .default }


##### timed

:   Enable timed intervals for axis. Experiment with this option if your
    joystick drifts away.

    Possible values: `on` *default*{ .default }, `off`


##### use_joy_calibration_hotkeys

:   Enable hotkeys to allow real-time calibration of the joystick's X and Y
    axes. Only consider this as a last resort if in-game calibration doesn't
    work correctly.

    Possible values: `on`, `off` *default*{ .default }

    Instructions:

    - Press Ctrl/Cmd+Arrow-keys to adjust the axis' scalar value:
      Left and Right diminish or magnify the x-axis scalar, respectively.
      Down and Up diminish or magnify the y-axis scalar, respectively.
    - Press Alt+Arrow-keys to adjust the axis' offset position:
      Left and Right shift X-axis offset in the given direction.
      Down and Up shift the Y-axis offset in the given direction.
    - Reset the X and Y calibration using Ctrl+Delete and Ctrl+Home,
      respectively.

    Each tap will report X or Y calibration values you can set in
    [joy_x_calibration](#joy_x_calibration) and
    [joy_y_calibration](#joy_y_calibration). When you find parameters that
    work, quit the game, switch this setting back to `off`, and populate the
    reported calibration parameters.
