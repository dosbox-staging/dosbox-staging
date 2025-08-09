// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2018-2021 kcgen <kcgen@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mp3_seek_table.h"

// Local headers
#include "util/math_utils.h"

// How many compressed MP3 frames should we skip between each recorded
// time point.  The trade-off is as follows:
//   - a large number means slower in-game seeking but a smaller in-memory seek table.
//   - a smaller numbers (below 10) results in fast seeks on slow hardware.
constexpr uint32_t FRAMES_PER_SEEK_POINT = 7;

// This function generates a new seek-table for a given mp3 stream and writes
// the data to the fast-seek file.
//
static uint64_t generate_new_seek_points(drmp3* const p_dr,
                                         std::vector<drmp3_seek_point>& seek_points_vector) {

    // Initialize our frame counters with zeros.
    drmp3_uint64 mp3_frame_count(0);
    drmp3_uint64 pcm_frame_count(0);

    // Get the number of compressed MP3 frames and the number of uncompressed PCM frames.
    drmp3_bool32 result = drmp3_get_mp3_and_pcm_frame_count(p_dr,
                                                            &mp3_frame_count,
                                                            &pcm_frame_count);

    if (   result != DRMP3_TRUE
        || mp3_frame_count < FRAMES_PER_SEEK_POINT
        || pcm_frame_count < FRAMES_PER_SEEK_POINT) {
        // LOG_MSG("MP3: failed to determine or find sufficient mp3 and pcm frames");
        return 0;
    }

    // Based on the number of frames found in the file, we size our seek-point
    // vector accordingly. We then pass our sized vector into dr_mp3 which populates
    // the decoded PCM times.
    // We also take into account the desired number of "FRAMES_PER_SEEK_POINT",
    // which is defined above.
    auto num_seek_points = static_cast<drmp3_uint32>
        (ceil_udivide(mp3_frame_count, FRAMES_PER_SEEK_POINT));

    seek_points_vector.resize(num_seek_points);
    result = drmp3_calculate_seek_points(p_dr,
                                         &num_seek_points,
                                         seek_points_vector.data());

    if (result != DRMP3_TRUE || num_seek_points == 0) {
        // LOG_MSG("MP3: failed to calculate sufficient seek points for stream");
        return 0;
    }

    // The calculate function provides us with the actual number of generated seek
    // points in the num_seek_points variable; so if this differs from expected then we
    // need to resize (ie: shrink) our vector.
    if (num_seek_points != seek_points_vector.size()) {
        seek_points_vector.resize(num_seek_points);
    }

    // Finally, we return the number of decoded PCM frames for this given file, which
    // doubles as a success-code.
    return pcm_frame_count;
}

uint64_t populate_seek_points(mp3_t* p_mp3, bool &result)
{
    // assume failure until proven otherwise
    result = false;

    const auto pcm_frame_count = generate_new_seek_points(p_mp3->p_dr,
                                                          p_mp3->seek_points_vector);
    if (pcm_frame_count == 0) {
        return 0;
    }

    // We bind our seek points to the dr_mp3 object which will be used for fast seeking.
    if(drmp3_bind_seek_table(p_mp3->p_dr,
                             static_cast<uint32_t>(p_mp3->seek_points_vector.size()),
                             p_mp3->seek_points_vector.data())
                             != DRMP3_TRUE) {
        return 0;
    }

    result = true;
    return pcm_frame_count;
}
