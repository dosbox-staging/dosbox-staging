/*
 *  Copyright (C) 2022       The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mouse.h"
#include "mouse_core.h"

#include <algorithm>
#include <array>

#include "callback.h"
#include "checks.h"
#include "cpu.h"
#include "keyboard.h"
#include "mem.h"
#include "pic.h"
#include "regs.h"
#include "video.h"

CHECK_NARROWING();

MouseShared mouse_shared;
MouseVideo mouse_video;

bool mouse_seamless_driver = false;
bool mouse_suggest_show    = false;

static MouseButtons12 buttons_12 = 0;   // host side buttons 1 (left), 2 (right)
static MouseButtons345 buttons_345 = 0; // host side buttons 3 (middle), 4, and 5

static bool raw_input = true; // true = relative input is raw data, without
                              // host OS mouse acceleration applied

static uintptr_t int74_ret_callback = 0;

// ***************************************************************************
// Debug code, normally not enabled
// ***************************************************************************

// #define DEBUG_QUEUE_ENABLE

#ifndef DEBUG_QUEUE_ENABLE
#    define DEBUG_QUEUE(...) ;
#else
// TODO: after migrating to C++20, allow to skip the 2nd argument by using
// '__VA_OPT__(,) __VA_ARGS__' instead of ', __VA_ARGS__'
#    define DEBUG_QUEUE(fmt, ...) \
        LOG_INFO("(queue) %04d: " fmt, DEBUG_GetDiffTicks(), __VA_ARGS__);

static uint32_t DEBUG_GetDiffTicks()
{
    static uint32_t previous_ticks = 0;
    uint32_t diff_ticks = 0;

    if (previous_ticks)
        diff_ticks = PIC_Ticks - previous_ticks;

    previous_ticks = PIC_Ticks;
    return diff_ticks;
}

#endif

// ***************************************************************************
// Mouse button helper functions
// ***************************************************************************

static MouseButtonsAll GetButtonsJoined()
{
    MouseButtonsAll buttons_all;
    buttons_all.data = buttons_12.data | buttons_345.data;

    return buttons_all;
}

static MouseButtons12S GetButtonsSquished()
{
    MouseButtons12S buttons_12S;

    // Squish buttons 3/4/5 into single virtual middle button
    buttons_12S.data = buttons_12.data;
    if (buttons_345.data)
        buttons_12S.middle = 1;

    return buttons_12S;
}

// ***************************************************************************
// Mouse event queue implementation
// ***************************************************************************

typedef struct MouseEvent {
    bool req_ps2 = false; // if PS/2 mouse emulation needs an event
    bool req_vmm = false; // if virtual machine mouse needs an event
    bool req_dos = false; // if DOS mouse driver needs an event

    MouseEventId id             = MouseEventId::NotDosEvent;
    uint8_t mask                = 0;
    MouseButtons12S buttons_12S = 0;

    uint8_t aggr_mask = 0; // mask to check if button events can be aggregated

    MouseEvent() {}
    MouseEvent(MouseEventId id) : id(id), mask(static_cast<uint8_t>(id)) {}

} MouseEvent;

static class MouseQueue final {
public:
    MouseQueue();

    void AddEvent(MouseEvent &event);
    void FetchEvent(MouseEvent &event);
    void ClearEventsDOS();
    void StartTimerIfNeeded();
    void Tick();

    uint8_t start_delay_ps2     = 5; // for PS/2 events, in milliseconds
    uint8_t start_delay_dos_btn = 1; // for DOS button events
    uint8_t start_delay_dos_mov = 5; // for DOS move/wheel events

private:
    MouseQueue(const MouseQueue &)            = delete;
    MouseQueue &operator=(const MouseQueue &) = delete;

    void AggregateEventsDOS(MouseEvent &event);
    MouseEvent &PopEventBtn();
    void UpdateDelayCounters();

    std::array<MouseEvent, 8> events = {}; // a modulo queue of events
    uint8_t idx_first                = 0;  // index of the first event
    uint8_t num_events               = 0;  // number of events in the queue

    MouseButtons12S last_buttons_12S = 0; // last buttons reported

    bool queue_overflow    = false;
    bool timer_in_progress = false;

    // Time in milliseconds which has to elapse before event can take place
    uint8_t delay_ps2     = 0;
    uint8_t delay_dos_btn = 0; // for DOS button events
    uint8_t delay_dos_mov = 0; // for DOS move/wheel events

    // Events for which a flag is enough to store them
    bool event_ps2       = false;
    bool event_dos_moved = false;
    bool event_dos_wheel = false;

    bool prefer_ps2 = false; // true = next time prefer PS/2 event

    uint32_t ticks_start = 0; // PIC_Ticks value when timer starts

    // Helper masks for aggregating DOS button events
    static constexpr uint8_t aggr_mask_pressed =
            static_cast<uint8_t>(MouseEventId::PressedLeft)  |
            static_cast<uint8_t>(MouseEventId::PressedRight) |
            static_cast<uint8_t>(MouseEventId::PressedMiddle);
    static constexpr uint8_t aggr_mask_released =
            static_cast<uint8_t>(MouseEventId::ReleasedLeft)  |
            static_cast<uint8_t>(MouseEventId::ReleasedRight) |
            static_cast<uint8_t>(MouseEventId::ReleasedMiddle);

    // Helpers to check if there are events in the queue
    inline bool HasEventDOSmov() const
    {
        return event_dos_moved || event_dos_wheel;
    }
    inline bool HasEventDOSbtn() const
    {
        return (num_events != 0);
    }
    inline bool HasEventDOSany() const
    {
        return HasEventDOSmov() || HasEventDOSbtn();
    }
    inline bool HasEventPS2() const
    {
        return event_ps2;
    }
    inline bool HasEventAny() const
    {
        return HasEventDOSany() || HasEventPS2();
    }

    // Helpers to check if there are events ready to be handled
    inline bool HasReadyEventPS2() const
    {
        return HasEventPS2() && !delay_ps2;
    }
    inline bool HasReadyEventDOSmov() const
    {
        return HasEventDOSmov() && !delay_dos_mov &&
               !mouse_shared.dos_cb_running; // callback busy = no new
                                             // event
    }
    inline bool HasReadyEventDOSbtn() const
    {
        return HasEventDOSbtn() && !delay_dos_btn &&
               !mouse_shared.dos_cb_running; // callback busy = no new
                                             // event
    }
    inline bool HasReadyEventAny() const
    {
        return HasReadyEventPS2() || HasReadyEventDOSmov() ||
               HasReadyEventDOSbtn();
    }

} queue;

static void MouseQueueTick(uint32_t)
{
    queue.Tick();
}

MouseQueue::MouseQueue() {}

void MouseQueue::AddEvent(MouseEvent &event)
{
    DEBUG_QUEUE("AddEvent:   %s %s %s",
                event.req_ps2 ? "PS2" : "---",
                event.req_vmm ? "VMM" : "---",
                event.req_dos ? "DOS" : "---");

    // From now on, do not distinguish between VMM and PS/2
    // events - virtual machine managers only
    // need to trigger PS/2 packet updates
    event.req_ps2 |= event.req_vmm;

    // If events are being fetched, clear the DOS overflow flag
    if (mouse_shared.active_dos && !mouse_shared.dos_cb_running)
        queue_overflow = false;

    // If queue got overflowed due to DOS not taking events,
    // don't accept any more DOS events other than mouse move,
    // as it might lead to strange effects in DOS application
    if (GCC_UNLIKELY(queue_overflow) && event.req_dos &&
        event.id != MouseEventId::MouseHasMoved) {
        event.req_dos = false;
        // Normal mechanism for updating button state is not working now
        last_buttons_12S = event.buttons_12S;
    }

    // Mouse movements should be aggregated, no point in handling
    // excessive amount of events
    if (event.req_dos)
        AggregateEventsDOS(event);

    // Prevent unnecessary further processing
    if (!event.req_dos && !event.req_ps2)
        return; // event not relevant any more

    bool restart_timer = false;
    if (event.req_ps2) {
        if (!HasEventPS2() && timer_in_progress && !delay_ps2) {
            DEBUG_QUEUE("AddEvent: restart timer for %s", "PS2");
            // We do not want the timer to start only then DOS event
            // gets processed - for minimum latency it is better to
            // restart the timer
            restart_timer = true;
        }

        // Events for PS/2 interfaces (or virtualizer compatible
        // drivers) do not carry any information - they are only
        // notifications that new data is available for fetching
        event_ps2 = true;
    }

    if (event.req_dos) {
        if (!HasEventDOSany() && timer_in_progress && !delay_dos_btn &&
            !delay_dos_mov) {
            DEBUG_QUEUE("AddEvent: restart timer for %s", "DOS");
            // We do not want the timer to start only then PS/2
            // event gets processed - for minimum latency it is
            // better to restart the timer
            restart_timer = true;
        }

        if (event.id == MouseEventId::MouseHasMoved) {
            // Mouse has moved - put in priority place
            event_dos_moved = true;
        } else if (event.id == MouseEventId::WheelHasMoved) {
            // Wheel has moved - put in priority place
            event_dos_wheel = true;
        } else if (GCC_UNLIKELY(num_events >= events.size())) {
            // No space left, queue overflow. Clear it (leave only
            // movement notifications) and don't accept any more
            // button/wheel events until application starts to react
            num_events      = 0;
            event_dos_wheel = false;
            queue_overflow  = true;
        } else {
            // Button press/release - put into the queue
            const auto idx = (idx_first + num_events++) % events.size();
            event.buttons_12S.data = GetButtonsSquished().data;
            events[idx]            = event;
        }
    }

    if (restart_timer) {
        timer_in_progress = false;
        PIC_RemoveEvents(MouseQueueTick);
        UpdateDelayCounters();
        StartTimerIfNeeded();
    } else if (!timer_in_progress) {
        DEBUG_QUEUE("ActivateIRQ, in %s", __FUNCTION__);
        // If no timer in progress, handle the event now
        PIC_ActivateIRQ(12);
    }
}

void MouseQueue::AggregateEventsDOS(MouseEvent &event)
{
    // Try to aggregate move / wheel events
    if ((event_dos_moved && event.id == MouseEventId::MouseHasMoved) ||
        (event_dos_wheel && event.id == MouseEventId::WheelHasMoved)) {
        event.req_dos = false; // DOS queue already has such an event
        return;
    }

    // Non-button events can't be aggregated with button events
    // at this moment, this is only possible once they are being
    // passed to the interrupt
    if (event.id == MouseEventId::MouseHasMoved ||
        event.id == MouseEventId::WheelHasMoved)
        return;

    // Generate masks to detect whether two button events can be
    // aggregated (might be needed later even if we have no more
    // events now)
    if (event.mask & aggr_mask_pressed)
        // Set 'pressed+released' for every 'pressed' bit
        event.aggr_mask = static_cast<uint8_t>(event.mask | (event.mask << 1));
    else if (event.mask & aggr_mask_released)
        // Set 'pressed+released' for every 'released' bit
        event.aggr_mask = static_cast<uint8_t>(event.mask | (event.mask >> 1));

    // If no button events in the queue, skip further processing
    if (!num_events)
        return;

    // Try to aggregate with the last queue event
    auto &last_event = events[(idx_first + num_events) % events.size()];
    if (GCC_UNLIKELY(last_event.aggr_mask & event.aggr_mask))
        return; // no aggregation possible

    // Both events can be aggregated into a single one
    last_event.mask |= event.mask;
    last_event.aggr_mask |= event.aggr_mask;
    last_event.buttons_12S = event.buttons_12S;

    // Event aggregated, DOS does not need it any more
    event.req_dos = false;
}

MouseEvent &MouseQueue::PopEventBtn()
{
    assert(num_events > 0);

    auto &event = events[idx_first];

    idx_first++;
    idx_first = static_cast<uint8_t>(idx_first % events.size());
    num_events--;

    return event;
}

void MouseQueue::FetchEvent(MouseEvent &event)
{
    // First try prioritized (move/wheel) DOS events
    if (HasReadyEventDOSmov()) {
        // Set delay before next DOS events
        delay_dos_btn = start_delay_dos_btn;
        delay_dos_mov = start_delay_dos_mov;
        // Fill in common event information
        event.req_dos     = true;
        event.buttons_12S = last_buttons_12S;
        // Mark which events to handle
        if (event_dos_moved) {
            event.mask |= static_cast<uint8_t>(MouseEventId::MouseHasMoved);
            event_dos_moved = false;
        }
        if (event_dos_wheel) {
            event.mask |= static_cast<uint8_t>(MouseEventId::WheelHasMoved);
            event_dos_wheel = false;
        }
        // If possible, aggregate button events
        if (!HasReadyEventDOSbtn())
            return;
        const auto &event_btn = PopEventBtn();
        event.mask |= event_btn.mask;
        last_buttons_12S  = event_btn.buttons_12S;
        event.buttons_12S = last_buttons_12S;
        return;
    }

    // We should prefer PS/2 events now (as the last was DOS one),
    // but we can't if there is no PS/2 event ready to be handled
    if (!HasReadyEventPS2())
        prefer_ps2 = false;

    // Try DOS button events
    if (HasReadyEventDOSbtn() && !prefer_ps2) {
        // Next time prefer PS/2 events over buttons for DOS driver
        prefer_ps2 = true;
        // Set delay before next DOS events
        delay_dos_btn = start_delay_dos_btn;
        delay_dos_mov = std::max(delay_dos_mov, delay_dos_btn);
        // Take event from the queue
        event            = PopEventBtn();
        last_buttons_12S = event.buttons_12S;
        return;
    }

    // Now try PS/2 event
    if (HasReadyEventPS2()) {
        // Next time prefer DOS event
        prefer_ps2 = false;
        // Set delay before next PS/2 events
        delay_ps2 = start_delay_ps2;
        // PS/2 events are really dummy - merely a notification
        // that something has happened and driver has to react
        event.req_ps2 = true;
        event_ps2     = false;
        return;
    }

    // Nothing to provide to interrupt handler,
    // event will stay empty
}

void MouseQueue::ClearEventsDOS()
{
    // Clear DOS relevant part of the queue
    num_events      = 0;
    event_dos_moved = false;
    event_dos_wheel = false;
    delay_dos_mov   = 0;
    delay_dos_btn   = 0;

    // The overflow reason is most likely gone
    queue_overflow = false;

    if (!HasEventAny()) {
        timer_in_progress = false;
        PIC_RemoveEvents(MouseQueueTick);
    }
}

void MouseQueue::StartTimerIfNeeded()
{
    // Do nothing if timer is already in progress
    if (timer_in_progress)
        return;

    bool timer_needed = false;
    uint8_t delay     = UINT8_MAX;

    if (delay_ps2) {
        timer_needed = true;
        delay        = std::min(delay, delay_ps2);
    }
    if (delay_dos_mov) {
        timer_needed = true;
        delay        = std::min(delay, delay_dos_mov);
    } else if (delay_dos_btn) {
        // Do not report button before the movement
        timer_needed = true;
        delay        = std::min(delay, delay_dos_btn);
    }

    // If queue is empty and all expired, we need no timer
    if (!timer_needed) {
        return;
    }

    // Enforce some non-zero delay between events; needed
    // for example if DOS interrupt handler is busy
    delay = std::max(delay, static_cast<uint8_t>(1));

    // Start the timer
    DEBUG_QUEUE("StartTimer, %d", delay);
    ticks_start       = PIC_Ticks;
    timer_in_progress = true;
    PIC_AddEvent(MouseQueueTick, static_cast<double>(delay));
}

void MouseQueue::UpdateDelayCounters()
{
    const uint32_t tmp = (PIC_Ticks > ticks_start) ? (PIC_Ticks - ticks_start)
                                                   : 1;
    uint8_t elapsed    = static_cast<uint8_t>(
                std::min(tmp, static_cast<uint32_t>(UINT8_MAX)));
    if (!ticks_start)
        elapsed = 1;

    auto calc_new_delay = [](const uint8_t delay, const uint8_t elapsed) {
        return static_cast<uint8_t>((delay > elapsed) ? (delay - elapsed)
                                                      : 0);
    };

    delay_ps2     = calc_new_delay(delay_ps2, elapsed);
    delay_dos_mov = calc_new_delay(delay_dos_mov, elapsed);
    delay_dos_btn = calc_new_delay(delay_dos_btn, elapsed);

    ticks_start = 0;
}

void MouseQueue::Tick()
{
    DEBUG_QUEUE("%s", "Tick");

    timer_in_progress = false;
    UpdateDelayCounters();

    // If we have anything to pass to guest side, activate interrupt;
    // otherwise start the timer again
    if (HasReadyEventAny()) {
        DEBUG_QUEUE("ActivateIRQ, in %s", __FUNCTION__);
        PIC_ActivateIRQ(12);
    } else
        StartTimerIfNeeded();
}

// ***************************************************************************
// Common helper calculations
// ***************************************************************************

float MOUSE_GetBallisticsCoeff(const float speed)
{
    // This routine provides a function for mouse ballistics
    //(cursor acceleration), to be reused by various mouse interfaces.
    // Since this is a DOS emulator, the acceleration model is
    // based on a historic PS/2 mouse specification.

    // Input: mouse speed
    // Output: acceleration coefficient (1.0f for speed >= 6.0f)

    // If we don't have raw mouse input, stay with flat profile;
    // in such case the acceleration is already handled by the host OS,
    // adding our own could lead to hard to predict (most likely
    // undesirable) effects
    if (!raw_input)
        return 1.0f;

    constexpr float a      = 0.017153417f;
    constexpr float b      = 0.382477002f;
    constexpr float lowest = 0.5f;

    // Normal PS/2 mouse 2:1 scaling algorithm is just a substitution:
    // 0 => 0, 1 => 1, 2 => 1, 3 => 3, 4 => 6, 5 => 9, other x => x * 2
    // and the same for negatives. But we want smooth cursor movement,
    // therefore we use approximation model (least square regression,
    // 3rd degree polynomial, on points -6, -5, ..., 0, ... , 5, 6,
    // here scaled to give f(6.0) = 6.0). Polynomial would be:
    //
    // f(x) = a*(x^3) + b*(x^1) = x*(a*(x^2) + b)
    //
    // This C++ function provides not the full polynomial, but rather
    // a coefficient (0.0f ... 1.0f) calculated from supplied speed,
    // by which the relative mouse measurement should be multiplied

    if (speed > -6.0f && speed < 6.0f)
        return std::max((a * speed * speed + b), lowest);
    else
        return 1.0f;

    // Please consider this algorithm as yet another nod to the past,
    // one more small touch of 20th century PC computing history :)
}

float MOUSE_ClampRelMov(const float rel)
{
    // Enforce upper limit of relative mouse movement
    return std::clamp(rel, -2048.0f, 2048.0f);
}

// ***************************************************************************
// Interrupt 74 implementation
// ***************************************************************************

static uintptr_t INT74_Exit()
{
    SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
    reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));

    return CBRET_NONE;
}

static uintptr_t INT74_Handler()
{
    MouseEvent event;
    queue.FetchEvent(event);
    DEBUG_QUEUE("FetchEvent: %s <<< %s",
                event.req_ps2 ? "PS2" : "---",
                event.req_dos ? "DOS" : "---");

    // If DOS driver is active, use it to handle the event
    if (event.req_dos && mouse_shared.active_dos) {
        // Taken from DOSBox X: HERE within the IRQ 12 handler is the
        // appropriate place to redraw the cursor. OSes like Windows 3.1
        // expect real-mode code to do it in response to IRQ 12, not
        // "out of the blue" from the SDL event handler like the
        // original DOSBox code did it. Doing this allows the INT 33h
        // emulation to draw the cursor while not causing Windows 3.1 to
        // crash or behave erratically.
        MOUSEDOS_DrawCursor();

        // If DOS driver's client is not interested in this particular
        // type of event - skip it
        if (!MOUSEDOS_HasCallback(event.mask))
            return INT74_Exit();

        CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
        CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)) + 7);

        DEBUG_QUEUE("INT74: %s", "DOS");
        return MOUSEDOS_DoCallback(event.mask, event.buttons_12S);
    }

    // If BIOS interface is active, use it to handle the event
    if (event.req_ps2 && mouse_shared.active_bios) {
        CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
        CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));

        DEBUG_QUEUE("INT74: %s", "PS2");
        MOUSEPS2_UpdatePacket();
        return MOUSEBIOS_DoCallback();
    }

    // No mouse emulation module is interested in event
    return INT74_Exit();
}

uintptr_t INT74_Ret_Handler()
{
    queue.StartTimerIfNeeded();
    return CBRET_NONE;
}

// ***************************************************************************
// Helper functions
// ***************************************************************************

static MouseEventId SelectIdPressed(const uint8_t idx, const bool changed_12S)
{
    switch (idx) {
    case 0: return MouseEventId::PressedLeft;
    case 1: return MouseEventId::PressedRight;
    case 2: return MouseEventId::PressedMiddle;
    case 3:
    case 4:
        return changed_12S ? MouseEventId::PressedMiddle
                           : MouseEventId::NotDosEvent;
    default: return MouseEventId::NotDosEvent;
    }
}

static MouseEventId SelectIdReleased(const uint8_t idx, const bool changed_12S)
{
    switch (idx) {
    case 0: return MouseEventId::ReleasedLeft;
    case 1: return MouseEventId::ReleasedRight;
    case 2: return MouseEventId::ReleasedMiddle;
    case 3:
    case 4:
        return changed_12S ? MouseEventId::ReleasedMiddle
                           : MouseEventId::NotDosEvent;
    default: return MouseEventId::NotDosEvent;
    }
}

// ***************************************************************************
// External notifications
// ***************************************************************************

void MOUSE_SetConfig(const bool new_raw_input)
{
    raw_input = new_raw_input;
}

void MOUSE_NewScreenParams(const uint16_t clip_x, const uint16_t clip_y,
                           const uint16_t res_x, const uint16_t res_y,
                           const bool fullscreen, const uint16_t x_abs,
                           const uint16_t y_abs)
{
    mouse_video.clip_x = clip_x;
    mouse_video.clip_y = clip_y;

    // Protection against strange window sizes,
    // to prevent division by 0 in some places
    mouse_video.res_x = std::max(res_x, static_cast<uint16_t>(2));
    mouse_video.res_y = std::max(res_y, static_cast<uint16_t>(2));

    mouse_video.fullscreen = fullscreen;

    MOUSEVMM_NewScreenParams(x_abs, y_abs);
    MOUSE_NotifyStateChanged();
}

void MOUSE_NotifyRateDOS(const uint8_t rate_hz)
{
    auto &val_mov = queue.start_delay_dos_mov;
    auto &val_btn = queue.start_delay_dos_btn;

    // Convert rate in Hz to delay in milliseconds
    val_mov = static_cast<uint8_t>(1000 / rate_hz);
    // Cheat a little, do not allow to set more than 10 milliseconds. Some
    // old games might have tried to set lower rate to reduce number of IRQs
    // and save CPU power - this is no longer necessary. Windows 3.1 also
    // sets a suboptimal value in its PS/2 driver.
    // Original DOSBox forces 5 milliseconds all the time, but this is
    // probably an overkill - 10 milliseconds is already twice the typical
    // 50 Hz screen refresh rate.
    val_mov = std::min(val_mov, static_cast<uint8_t>(10));
    // Cheat a little once again - our delay for buttons is separate and
    // much smaller, so that button events can be sent to the DOS
    // application with minimal latency. Due to hardware differences and
    // multiple independent mouse driver implementations in the past, it is
    // unlikely to cause problems. Besides, host OS and hardware have their
    // limitations, too, they also introduce some latency.
    val_btn = val_btn / 5;

    // TODO: make a configuration option(s) for this, same for PS/2 mouse
}

void MOUSE_NotifyRatePS2(const uint8_t rate_hz)
{
    auto &val_ps2 = queue.start_delay_ps2;

    // Convert rate in Hz to delay in milliseconds
    val_ps2 = static_cast<uint8_t>(1000 / rate_hz);
    // Cheat a little, like with DOS driver
    val_ps2 = std::min(val_ps2, static_cast<uint8_t>(10));
}

void MOUSE_NotifyResetDOS()
{
    queue.ClearEventsDOS();
}

void MOUSE_NotifyStateChanged()
{
    const auto old_seamless_driver    = mouse_seamless_driver;
    const auto old_mouse_suggest_show = mouse_suggest_show;

    // Prepare suggestions to the GUI
    mouse_seamless_driver = mouse_shared.active_vmm && !mouse_video.fullscreen;
    mouse_suggest_show = !mouse_shared.active_bios && !mouse_shared.active_dos;

    // TODO: if active_dos, but mouse pointer is outside of defined
    // range, suggest to show mouse pointer

    // Do not suggest to show host pointer if fullscreen
    // or if autoseamless mode is disabled
    mouse_suggest_show &= !mouse_video.fullscreen;
    mouse_suggest_show &= !mouse_video.autoseamless;

    // If state has really changed, update the GUI
    if (mouse_seamless_driver != old_seamless_driver ||
        mouse_suggest_show != old_mouse_suggest_show)
        GFX_UpdateMouseState();
}

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const uint16_t x_abs, const uint16_t y_abs)
{
    // From the GUI we are getting mouse movement data in two
    // distinct formats:
    //
    // - relative; this one has a chance to be raw movements,
    //   it has to be fed to PS/2 mouse emulation, serial port
    //   mouse emulation, etc.; any guest side software accessing
    //   these mouse interfaces will most likely implement it's
    //   own mouse acceleration/smoothing/etc.
    // - absolute; this follows host OS mouse behavior and should
    //   be fed to VMware seamless mouse emulation and similar
    //   interfaces
    //
    // Our DOS mouse driver (INT 33h) is a bit special, as it can
    // act both ways (seamless and non-seamless mouse pointer),
    // so it needs data in both formats.

    // Notify mouse interfaces

    MouseEvent event(MouseEventId::MouseHasMoved);

    if (!mouse_video.autoseamless || mouse_is_captured) {
        MOUSESERIAL_NotifyMoved(x_rel, y_rel);
        event.req_ps2 = MOUSEPS2_NotifyMoved(x_rel, y_rel);
    }
    event.req_vmm = MOUSEVMM_NotifyMoved(x_rel, y_rel, x_abs, y_abs);
    event.req_dos = MOUSEDOS_NotifyMoved(x_rel, y_rel, x_abs, y_abs);

    queue.AddEvent(event);
}

void MOUSE_NotifyMovedFake()
{
    MouseEvent event(MouseEventId::MouseHasMoved);
    event.req_vmm = true;

    queue.AddEvent(event);
}

void MOUSE_EventPressed(uint8_t idx)
{
    const auto buttons_12S_old = GetButtonsSquished();

    switch (idx) {
    case 0: // left button
        if (buttons_12.left)
            return;
        buttons_12.left = 1;
        break;
    case 1: // right button
        if (buttons_12.right)
            return;
        buttons_12.right = 1;
        break;
    case 2: // middle button
        if (buttons_345.middle)
            return;
        buttons_345.middle = 1;
        break;
    case 3: // extra button #1
        if (buttons_345.extra_1)
            return;
        buttons_345.extra_1 = 1;
        break;
    case 4: // extra button #2
        if (buttons_345.extra_2)
            return;
        buttons_345.extra_2 = 1;
        break;
    default: // button not supported
        return;
    }

    const auto buttons_12S = GetButtonsSquished();
    const bool changed_12S = (buttons_12S_old.data != buttons_12S.data);
    const uint8_t idx_12S  = idx < 2 ? idx : 2;

    MouseEvent event(SelectIdPressed(idx, changed_12S));

    if (!mouse_video.autoseamless || mouse_is_captured) {
        if (changed_12S) {
            MOUSESERIAL_NotifyPressed(buttons_12S, idx_12S);
        }
        event.req_ps2 = MOUSEPS2_NotifyPressedReleased(buttons_12S,
                                                       GetButtonsJoined());
    }
    if (changed_12S) {
        event.req_vmm = MOUSEVMM_NotifyPressedReleased(buttons_12S);
        event.req_dos = MOUSEDOS_NotifyPressed(buttons_12S, idx_12S, event.id);
        MOUSESERIAL_NotifyPressed(buttons_12S, idx_12S);
    }

    queue.AddEvent(event);
}

void MOUSE_EventReleased(uint8_t idx)
{
    const auto buttons_12S_old = GetButtonsSquished();

    switch (idx) {
    case 0: // left button
        if (!buttons_12.left)
            return;
        buttons_12.left = 0;
        break;
    case 1: // right button
        if (!buttons_12.right)
            return;
        buttons_12.right = 0;
        break;
    case 2: // middle button
        if (!buttons_345.middle)
            return;
        buttons_345.middle = 0;
        break;
    case 3: // extra button #1
        if (!buttons_345.extra_1)
            return;
        buttons_345.extra_1 = 0;
        break;
    case 4: // extra button #2
        if (!buttons_345.extra_2)
            return;
        buttons_345.extra_2 = 0;
        break;
    default: // button not supported
        return;
    }

    const auto buttons_12S = GetButtonsSquished();
    const bool changed_12S = (buttons_12S_old.data != buttons_12S.data);
    const uint8_t idx_12S  = idx < 2 ? idx : 2;

    MouseEvent event(SelectIdReleased(idx, changed_12S));

    // Pass mouse release to all the mice even if host pointer is not
    // captured, to prevent strange effects when pointer goes back in the
    // window

    event.req_ps2 = MOUSEPS2_NotifyPressedReleased(buttons_12S,
                                                   GetButtonsJoined());
    if (changed_12S) {
        event.req_vmm = MOUSEVMM_NotifyPressedReleased(buttons_12S);
        event.req_dos = MOUSEDOS_NotifyReleased(buttons_12S, idx_12S, event.id);
        MOUSESERIAL_NotifyReleased(buttons_12S, idx_12S);
    }

    queue.AddEvent(event);
}

void MOUSE_EventWheel(const int16_t w_rel)
{
    MouseEvent event(MouseEventId::WheelHasMoved);

    if (!mouse_video.autoseamless || mouse_is_captured) {
        event.req_ps2 = MOUSEPS2_NotifyWheel(w_rel);
        MOUSESERIAL_NotifyWheel(w_rel);
    }

    event.req_vmm = MOUSEVMM_NotifyWheel(w_rel);
    event.req_dos = MOUSEDOS_NotifyWheel(w_rel);

    queue.AddEvent(event);
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void MOUSE_Init(Section * /*sec*/)
{
    // Callback for ps2 irq
    auto call_int74 = CALLBACK_Allocate();
    CALLBACK_Setup(call_int74, &INT74_Handler, CB_IRQ12, "int 74");
    // pseudocode for CB_IRQ12:
    //    sti
    //    push ds
    //    push es
    //    pushad
    //    callback INT74_Handler
    //        ps2 or user callback if requested
    //        otherwise jumps to CB_IRQ12_RET
    //    push ax
    //    mov al, 0x20
    //    out 0xa0, al
    //    out 0x20, al
    //    pop    ax
    //    cld
    //    retf

    int74_ret_callback = CALLBACK_Allocate();
    CALLBACK_Setup(int74_ret_callback, &INT74_Ret_Handler, CB_IRQ12_RET, "int 74 ret");
    // pseudocode for CB_IRQ12_RET:
    //    cli
    //    mov al, 0x20
    //    out 0xa0, al
    //    out 0x20, al
    //    callback INT74_Ret_Handler
    //    popad
    //    pop es
    //    pop ds
    //    iret

    // (MOUSE_IRQ > 7) ? (0x70 + MOUSE_IRQ - 8) : (0x8 + MOUSE_IRQ);
    RealSetVec(0x74, CALLBACK_RealPointer(call_int74));

    MOUSEPS2_Init();
    MOUSEVMM_Init();
    MOUSEDOS_Init();
}
