// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imgmount.h"
#include "misc/notifications.h"
#include "mount.h"

// IMGMOUNT is a stub that forwards execution to the unified MOUNT command.
void IMGMOUNT::Run(void)
{
	// Print deprecation notice
	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "IMGMOUNT",
	                      "PROGRAM_IMGMOUNT_DEPRECATED");

	// Create a MOUNT command instance on the stack
	MOUNT mount_cmd;

	// Save the MOUNT instance's original command pointer
	CommandLine* original_mount_cmd = mount_cmd.cmd;

	// Temporarily point MOUNT's cmd to IMGMOUNT's cmd so it can process the
	// arguments
	mount_cmd.cmd = this->cmd;

	// Execute the unified mount logic
	mount_cmd.Run();

	// Restore MOUNT's original pointer before it goes out of scope.
	// If we don't do this, 'mount_cmd's destructor will delete 'this->cmd'.
	// Then, when IMGMOUNT dies later, it will delete 'this->cmd' AGAIN
	// (Double Free)
	mount_cmd.cmd = original_mount_cmd;
}

void IMGMOUNT::AddMessages()
{
	// Messages are now handled in MOUNT::AddMessages
}

void IMGMOUNT::ListImgMounts(void)
{
	// Deprecated. MOUNT::ListMounts() handles this now
	MOUNT m;
	m.ListMounts();
}
