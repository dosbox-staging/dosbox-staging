// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// DOS-style private stacks for hardware interrupts (MS-DOS 3.2+ STACKS=count,size).
//
// When a wrapped hardware IRQ fires, a small real-mode wrapper switches SS:SP
// to a stack drawn from a private pool, calls the previous handler, and
// restores SS:SP on the way out. This protects programs whose own stacks are
// too tight to absorb BIOS ISRs plus any chained handlers.
//
// The current implementation wraps only the timer interrupt (INT 08h, IRQ0).
// MS-DOS additionally wraps INT 02h and several other hardware-IRQ vectors;
// coverage may be expanded later with no change to the public surface.
//
// The wrapper, entry table, and stack pool live in a single conventional-
// memory segment owned by MCB_DOS so the block survives program exits.

#include "dos/dos.h"

#include <vector>

#include "cpu/callback.h"
#include "cpu/registers.h"
#include "dosbox.h"
#include "hardware/memory.h"
#include "ints/bios.h"
#include "misc/support.h"
#include "utils/math_utils.h"

namespace {

constexpr uint8_t Irq0Vector = 0x08;

// Match the MS-DOS defaults used when STACKS is active. The configurable
// minimum/maximum bounds are enforced by SetMinMax() on the config keys in
// dos.cpp; the value of MaxNestedInterrupts must track that maximum.
constexpr uint8_t DefaultStackCount = 9;
constexpr uint16_t DefaultStackSize = 128;

constexpr uint16_t EntrySize   = 8;
constexpr uint16_t WrapperSize = 0x20;
constexpr uint16_t TableOffset = WrapperSize;
constexpr uint8_t MaxNestedInterrupts = 64;

// One slot in the stack pool. `allocated` tracks whether the slot is in use;
// `saved_ss`/`saved_sp` hold the interrupted program's SS:SP so leave_irq_stack
// can restore it; `new_sp` is the precomputed top of this slot's stack within
// the pool segment.
struct StackEntry {
	bool allocated    = false;
	uint16_t saved_ss = 0;
	uint16_t saved_sp = 0;
	uint16_t new_sp   = 0;
};

// Runtime state for the interrupt-stack wrapper. The wrapper code, the per-slot
// table, and the stack pool all live in `segment` at offsets 0, TableOffset,
// and get_stack_area_offset() respectively.
//
// `active_depth` counts nested interrupts that have an entry in `active_entries`
// (which may itself be -1 if the pool was exhausted at entry); `untracked_depth`
// counts nested interrupts that arrived after `active_entries` was already full.
// Their sum is the true nesting depth, and both must be unwound on the way out.
struct InterruptStacks {
	bool installed                = false;
	bool exhausted_warning_logged = false;

	uint16_t segment       = 0;
	RealPt old_irq0_vector = 0;
	RealPt irq0_wrapper    = 0;

	CALLBACK_HandlerObject enter_callback = {};
	CALLBACK_HandlerObject leave_callback = {};

	std::vector<StackEntry> entries        = {};
	std::vector<int16_t> active_entries    = {};

	uint8_t stack_count     = DefaultStackCount;
	uint16_t stack_size     = DefaultStackSize;
	uint8_t active_depth    = 0;
	uint8_t untracked_depth = 0;
	uint8_t next_entry      = DefaultStackCount - 1;
};

InterruptStacks stacks = {};

// Segment layout: [wrapper code | per-slot table | stack pool].
// The wrapper occupies bytes [0, WrapperSize); the table follows at TableOffset
// with `stack_count` entries of EntrySize bytes; the stack pool fills the rest,
// each slot sized `stack_size`. The helpers below derive offsets and sizes from
// those constants and the runtime configuration.
uint16_t get_stack_area_offset()
{
	return check_cast<uint16_t>(TableOffset + stacks.stack_count * EntrySize);
}

uint16_t get_stack_top(const uint8_t index)
{
	return check_cast<uint16_t>(get_stack_area_offset() +
	                           (index + 1) * stacks.stack_size - 2);
}

uint16_t get_required_bytes()
{
	return check_cast<uint16_t>(get_stack_area_offset() +
	                           stacks.stack_count * stacks.stack_size);
}

uint16_t get_required_paragraphs()
{
	return ceil_udivide(get_required_bytes(), uint16_t{16});
}

void write_callback_instruction(PhysPt& address, const callback_number_t callback)
{
	phys_writeb(address++, 0xfe); // GRP 4
	phys_writeb(address++, 0x38); // DOSBox callback instruction
	phys_writew(address, callback);
	address += 2;
}

void write_far_call(PhysPt& address, const RealPt target)
{
	phys_writeb(address++, 0x9a); // call far ptr16:16
	phys_writew(address, RealOffset(target));
	address += 2;
	phys_writew(address, RealSegment(target));
	address += 2;
}

// Lay down the IRQ0 wrapper at the start of the segment. The wrapper runs on
// the interrupted program's stack on entry; the enter callback then switches
// SS:SP to a private slot. We push flags and far-call the previous IRQ0
// handler so its iret unwinds correctly back to the instruction after the
// call. The leave callback then restores the original SS:SP before our own
// iret returns to the interrupted program.
void write_irq0_wrapper()
{
	auto address = RealToPhysical(stacks.irq0_wrapper);

	phys_writeb(address++, 0x55); // push bp
	write_callback_instruction(address, stacks.enter_callback.Get_callback());
	phys_writeb(address++, 0x9c); // pushf
	write_far_call(address, stacks.old_irq0_vector);
	write_callback_instruction(address, stacks.leave_callback.Get_callback());
	phys_writeb(address++, 0x5d); // pop bp
	phys_writeb(address++, 0xcf); // iret
}

// Zero the segment and populate the per-slot table. Each table entry holds the
// alloc flag and (filled in at runtime) the saved SS:SP; each stack slot has
// its table-entry offset written into the top word. The latter mirrors the
// MS-DOS layout and helps both DOSBox-side debugging and any DOS code that
// inspects the stack area directly.
void initialise_stack_pool()
{
	const auto base = PhysicalMake(stacks.segment, 0);

	for (uint16_t offset = 0; offset < get_required_bytes(); ++offset) {
		mem_writeb(base + offset, 0);
	}

	stacks.entries.assign(stacks.stack_count, {});
	stacks.active_entries.assign(MaxNestedInterrupts, -1);

	for (uint8_t i = 0; i < stacks.stack_count; ++i) {
		const auto entry_offset = check_cast<uint16_t>(TableOffset +
		                                               i * EntrySize);
		const auto stack_top    = get_stack_top(i);

		stacks.entries[i]        = {};
		stacks.entries[i].new_sp = stack_top;

		// MS-DOS leaves the table-entry offset in the word at the top
		// of each interrupt stack. Keep the same layout for
		// debuggability and for code that inspects the stack area
		// directly.
		real_writew(stacks.segment, stack_top, entry_offset);
		real_writeb(stacks.segment, entry_offset + 0, 0); // free
		real_writew(stacks.segment, entry_offset + 6, stack_top);
	}

	stacks.active_depth    = 0;
	stacks.untracked_depth = 0;
	stacks.next_entry      = check_cast<uint8_t>(stacks.stack_count - 1);
}

void reset_runtime_state()
{
	stacks.installed                = false;
	stacks.exhausted_warning_logged = false;
	stacks.segment                  = 0;
	stacks.old_irq0_vector          = 0;
	stacks.irq0_wrapper             = 0;
	stacks.entries.clear();
	stacks.active_entries.clear();
	stacks.stack_count     = DefaultStackCount;
	stacks.stack_size      = DefaultStackSize;
	stacks.active_depth    = 0;
	stacks.untracked_depth = 0;
	stacks.next_entry      = DefaultStackCount - 1;
}

// Pick the next free slot, or return -1 if the pool is exhausted. We start
// at `next_entry` and walk downwards (wrapping at zero), which matches the
// MS-DOS "hi-mem first" allocation order: the deepest-nested interrupt gets
// the lowest-addressed slot. The intent is that if a slot does overflow, it
// clobbers another pool slot rather than the interrupted program's stack.
int16_t allocate_stack_entry()
{
	for (uint8_t i = 0; i < stacks.stack_count; ++i) {
		const auto index = check_cast<uint8_t>(
		        (stacks.next_entry + stacks.stack_count - i) %
		        stacks.stack_count);

		if (stacks.entries[index].allocated) {
			continue;
		}

		stacks.entries[index].allocated = true;
		stacks.next_entry = (index == 0) ? check_cast<uint8_t>(stacks.stack_count - 1)
		                                 : index - 1;
		real_writeb(stacks.segment, TableOffset + index * EntrySize, 1);
		return check_cast<int16_t>(index);
	}

	if (!stacks.exhausted_warning_logged) {
		LOG_WARNING("DOS: Interrupt stack pool exhausted; using interrupted stack");
		stacks.exhausted_warning_logged = true;
	}
	return -1;
}

// Callback fired by the wrapper before chaining to the previous IRQ0 handler.
// Allocates a slot, saves the interrupted SS:SP into both the runtime entry
// and its mirror in the in-segment table, then switches SS:SP to the top of
// the slot. If allocation fails (pool exhausted, or `active_entries` LIFO
// already full), the previous handler runs on the interrupted stack — the
// same fallback MS-DOS uses. BP is also pointed at the new top, matching the
// real MS-DOS wrapper's behaviour.
Bitu enter_irq_stack()
{
	if (!stacks.installed) {
		return CBRET_NONE;
	}

	if (stacks.active_depth >= stacks.active_entries.size()) {
		++stacks.untracked_depth;
		return CBRET_NONE;
	}

	const auto index                             = allocate_stack_entry();
	stacks.active_entries[stacks.active_depth++] = index;

	if (index < 0) {
		return CBRET_NONE;
	}

	auto& entry    = stacks.entries[static_cast<size_t>(index)];
	entry.saved_ss = SegValue(ss);
	entry.saved_sp = reg_sp;

	const auto entry_offset = check_cast<uint16_t>(
	        TableOffset + static_cast<uint8_t>(index) * EntrySize);
	real_writew(stacks.segment, entry_offset + 2, entry.saved_sp);
	real_writew(stacks.segment, entry_offset + 4, entry.saved_ss);

	SegSet16(ss, stacks.segment);
	reg_sp = entry.new_sp;

	// The real MS-DOS stack wrapper uses BP while selecting and switching
	// stacks. This keeps the interrupted program's BP out of BIOS INT 1Ch.
	reg_bp = entry.new_sp;
	return CBRET_NONE;
}

// Counterpart to enter_irq_stack: pop the most recent slot off the LIFO,
// restore SS:SP from the saved entry, free the slot, and clear the in-segment
// mirror. The `untracked_depth` branch unwinds the pathological case where
// the LIFO overflowed on entry; in that case there was no slot to switch to,
// so there is nothing to restore here either.
Bitu leave_irq_stack()
{
	if (!stacks.installed) {
		return CBRET_NONE;
	}

	if (stacks.untracked_depth > 0) {
		--stacks.untracked_depth;
		return CBRET_NONE;
	}

	if (stacks.active_depth == 0) {
		return CBRET_NONE;
	}

	const auto index = stacks.active_entries[--stacks.active_depth];
	stacks.active_entries[stacks.active_depth] = -1;

	if (index < 0) {
		return CBRET_NONE;
	}

	auto& entry = stacks.entries[static_cast<size_t>(index)];

	SegSet16(ss, entry.saved_ss);
	reg_sp = entry.saved_sp;

	entry.allocated   = false;
	stacks.next_entry = static_cast<uint8_t>(index);

	const auto entry_offset = check_cast<uint16_t>(
	        TableOffset + static_cast<uint8_t>(index) * EntrySize);
	real_writeb(stacks.segment, entry_offset + 0, 0);
	real_writew(stacks.segment, entry_offset + 2, 0);
	real_writew(stacks.segment, entry_offset + 4, 0);

	return CBRET_NONE;
}

} // namespace

// Decide whether to install the wrapper based on the `stacks` setting. `auto`
// is treated as a proxy for "modern enough to plausibly run software that
// needs this": EGA-or-better machines roughly correlate with the MS-DOS 3.2+
// era where games started to assume STACKS= was available, and we explicitly
// match MS-DOS in not installing on PCjr-class hardware.
bool DOS_ShouldUseInterruptStacks(const SectionProp& section)
{
	const std::string setting = section.GetString("stacks");

	if (setting == "off") {
		return false;
	}
	if (setting == "on") {
		return true;
	}

	assert(setting == "auto");
	return is_machine_ega_or_better();
}

void DOS_InstallInterruptStacks(const SectionProp& section)
{
	if (stacks.installed) {
		return;
	}

	stacks.stack_count = check_cast<uint8_t>(section.GetInt("stacks_count"));
	stacks.stack_size  = check_cast<uint16_t>(section.GetInt("stacks_size"));

	// Allocate the wrapper, table and stack pool from conventional memory
	// via the DOS MCB chain, matching MS-DOS's STACKS layout. Mark the
	// owning PSP as MCB_DOS so the block is treated as kernel-owned and is
	// not freed when programs exit.
	uint16_t segment            = 0;
	const uint16_t requested    = get_required_paragraphs();
	uint16_t blocks             = requested;
	const uint16_t saved_psp    = dos.psp();
	dos.psp(MCB_DOS);
	const bool allocated = DOS_AllocateMemory(&segment, &blocks);
	dos.psp(saved_psp);

	if (!allocated) {
		LOG_WARNING("DOS: Could not allocate %u paragraphs for interrupt stacks "
		            "(largest free block: %u paragraphs); feature disabled",
		            requested, blocks);
		return;
	}

	DOS_MCB(check_cast<uint16_t>(segment - 1)).SetFileName("SD      ");

	stacks.segment         = segment;
	stacks.irq0_wrapper    = RealMake(stacks.segment, 0);
	stacks.old_irq0_vector = RealGetVec(Irq0Vector);

	stacks.enter_callback.Allocate(enter_irq_stack, "DOS IRQ stack enter");
	stacks.leave_callback.Allocate(leave_irq_stack, "DOS IRQ stack leave");

	initialise_stack_pool();
	write_irq0_wrapper();

	RealSetVec(Irq0Vector, stacks.irq0_wrapper);
	stacks.installed = true;
}

// Reverse the install sequence at shutdown. We restore the IVT only if we
// still own the vector; if a TSR has hooked over us, unhooking would corrupt
// its chain, so we leave the vector alone and log a warning. The conventional-
// memory segment itself is not freed here — it is owned by MCB_DOS and will
// be reclaimed when the DOS kernel tears down.
void DOS_UninstallInterruptStacks()
{
	if (!stacks.installed) {
		return;
	}

	if (RealGetVec(Irq0Vector) == stacks.irq0_wrapper) {
		RealSetVec(Irq0Vector, stacks.old_irq0_vector);
	} else {
		LOG_WARNING("DOS: IRQ0 vector changed while interrupt stacks were installed");
	}

	stacks.enter_callback.Uninstall();
	stacks.leave_callback.Uninstall();

	reset_runtime_state();
}
