// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_COMMON_H
#define DOSBOX_GUI_COMMON_H

enum class PresentationMode {
	// In DOS rate presentation mode, the video frames are presented at the
	// emulated DOS refresh rate, irrespective of the host operating system's
	// display refresh rate (e.g., ~70 Hz for the common 320x200 VGA mode). In
	// other words, the DOS rate and only that determines the presentation
	// rate.
	//
	// The best use-case for presenting at the DOS rate is variable refresh
	// rate (VRR) monitors; in this case, our present rate dictates the
	// refresh rate of the monitor, so to speak, so we can handle any weird
	// DOS refresh rate without tearing. Another common use case is presenting
	// on a fixed refresh rate monitor without vsync.
	DosRate,

	// In host rate presentation mode, the video frames are presented at the
	// refresh rate of the host monitor (the refresh rate set at the host
	// operating system level), irrespective of the emulated DOS video mode's
	// refresh rate. This effectively means we present the most recently
	// rendered frame at regularly spaced intervals determined by the host
	// rate.
	//
	// Host rate only really makes sense with vsync enabled on fixed refresh
	// rate monitors. Without vsync, we aren't better off than simply
	// presenting at the DOS rate (there would be a lot of tearing in both
	// cases; it doesn't matter how exactly the tearing happens). But with
	// vsync enabled, we're effectively "sampling" the stream of emulated
	// video frames at the host refresh rate and display them vsynced without
	// tearing. This means that some frames might be presented twice and some
	// might be skipped due to the mismatch between the DOS and the host rate.
	//
	// The most common use case for vsynced host rate presentation is
	// displaying ~70 Hz 320x200 VGA content on a fixed 60 Hz refresh rate
	// monitor.
	HostRate
};

enum class RenderingBackend { Texture, OpenGl };

#endif // DOSBOX_GUI_COMMON_H
