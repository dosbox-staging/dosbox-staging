/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2018-2021  kcgen <kcgen@users.noreply.github.com>
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

/*
 *  DOSBox MP3 Seek Table Handler
 *  -----------------------------
 *
 * Problem:
 *          Seeking within an MP3 file to an exact time-offset, such as is expected
 *          within DOS games, is extremely difficult because the MP3 format doesn't
 *          provide a defined relationship between the compressed data stream positions
 *          versus decompressed PCM times.
 *
 * Solution:
 *          To solve this, we step through each compressed MP3 frames in
 *          the MP3 file (without decoding the actual audio) and keep a record of the
 *          decompressed "PCM" times for each frame.  We save this relationship to
 *          to a local fie, called a fast-seek look-up table, which we can quickly
 *          reuse every subsequent time we need to seek within the MP3 file.  This allows
 *          seeks to be performed extremely fast while being PCM-exact.
 *
 *          This "fast-seek" file can hold data for multiple MP3s to avoid
 *          creating an excessive number of files in the local working directory.
 *
 * Challenges:
 *       1. What happens if an MP3 file is changed but the MP3's filename remains the same?
 *
 *          The lookup table is indexed based on a checksum instead of filename.
 *          The checksum is calculated based on a subset of the MP3's content in
 *          addition to being seeded based on the MP3's size in bytes.
 *          This makes it very sensitive to changes in MP3 content; if a change
 *          is detected a new lookup table is generated.
 *
 *       2. Checksums can be weak, what if a collision happens?
 *
 *          To avoid the risk of collision, we use the current best-of-breed
 *          xxHash algorithm that has a quality-score of 10, the highest rating
 *          from the SMHasher test set.  See https://github.com/Cyan4973/xxHash
 *          for more details.
 *
 *       3. What happens if fast-seek file is brought from a little-endian
 *          machine to a big-endian machine (x86 or ARM to a PowerPC or Sun
 *          Sparc machine)?
 *
 *          The lookup table is serialized and multi-byte types are byte-swapped
 *          at runtime according to the architecture. This makes fast-seek files
 *          cross-compatible regardless of where they were written to or read from.
 *
 *       4. What happens if this code is updated to use a new fast-seek file
 *          format, but an old fast-seek file exists?
 *
 *          The seek-table file is versioned (see SEEK_TABLE_IDENTIFIER befow),
 *          therefore, if the format and version is updated, then the seek-table
 *          will be regenerated.
 */

#include "mp3_seek_table.h"

// System headers
#include <algorithm>
#include <climits>
#include <fstream>
#include <map>
#include <string>
#include <sys/stat.h>

// Local headers
#include "math_utils.h"

#define XXH_INLINE_ALL 1
#define XXH_NO_INLINE_HINTS 1
#define XXH_STATIC_LINKING_ONLY 1
#if !defined(NDEBUG)
    #define XXH_DEBUGLEVEL 1
#endif
#include "xxhash.h"

// C++ scope modifiers
using std::map;
using std::vector;
using std::string;
using std::ios_base;
using std::ifstream;
using std::ofstream;

// container types
using seek_points_table_t = typename std::map<uint64_t, std::vector<drmp3_seek_point_serial> >;
using frame_count_table_t = typename std::map<uint64_t, uint64_t>;

// Identifies a valid versioned seek-table
#define SEEK_TABLE_IDENTIFIER "st-v6"

// How many compressed MP3 frames should we skip between each recorded
// time point.  The trade-off is as follows:
//   - a large number means slower in-game seeking but a smaller fast-seek file.
//   - a smaller numbers (below 10) results in fast seeks on slow hardware.
constexpr uint32_t FRAMES_PER_SEEK_POINT = 7;

// Returns the size of a file in bytes (if valid), otherwise -1
off_t get_file_size(const char* filename) {
    struct stat stat_buf;
    const auto rc = stat(filename, &stat_buf);
    return (rc >= 0) ? stat_buf.st_size : -1;
}


// Calculates a unique 64-bit hash (integer) from the provided file.
// This function should not cause side-effects; ie, the current
// read-position within the file should not be altered.
//
// This function tries to files as-close to the middle of the MP3 file as possible,
// and use that feed the hash function in hopes of the most uniqueness.
// We're trying to avoid content that might be duplicated across MP3s, like:
// 1. ID3 tag filler content, which might be boiler plate or all empty
// 2. Trailing silence or similar zero-PCM content
//
uint64_t calculate_stream_hash(struct SDL_RWops* const context) {
    // Save the current stream position, so we can restore it at the end of the function.
    const auto original_pos = SDL_RWtell(context);

    // Seek to the end of the file so we can calculate the stream size.
    SDL_RWseek(context, 0, RW_SEEK_END);
    const auto end_pos = SDL_RWtell(context);
    SDL_RWseek(context, original_pos, RW_SEEK_SET); // restore position

    // Check if we can trust the end position as the stream size
    if (end_pos <= 0)
        return 0;
    const auto stream_size = static_cast<size_t>(end_pos);

    // create a hash state
    const auto state = XXH64_createState();
    if (!state)
        return 0;

    // Initialize state seeded with our total stream size
    const auto seed = static_cast<XXH64_hash_t>(stream_size);
    if (XXH64_reset(state, seed) == XXH_ERROR) {
        XXH64_freeState(state);
        return 0;
    }

    // Setup counters and read buffer
    XXH64_hash_t hash = 0;
    size_t current_bytes_read = 0;
    size_t total_bytes_read = 0;
    auto hash_state = XXH_OK;
    vector<char> buffer(1024, 0);

    // Seek prior to the last 32 KiB (or less) in the file, which is what we'll hash
    const auto bytes_to_hash = std::min(static_cast<size_t>(32768u), stream_size);
    const auto pos_to_hash_from = static_cast<Sint64>(stream_size - bytes_to_hash);
    SDL_RWseek(context, pos_to_hash_from, RW_SEEK_SET);
    assert(SDL_RWtell(context) == pos_to_hash_from);

    // Feed the state with input data, any size, any number of times
    do {
        current_bytes_read = SDL_RWread(context, buffer.data(), 1, buffer.size());
        total_bytes_read += current_bytes_read;
        hash_state = XXH64_update(state, buffer.data(), current_bytes_read);
    }
    while (current_bytes_read
           && total_bytes_read < bytes_to_hash
           && hash_state == XXH_OK);

    // Finalize the hash and clean it up
    hash = XXH64_digest(state);
    XXH64_freeState(state);

    // restore the stream position and cleanup the hash
    SDL_RWseek(context, original_pos, RW_SEEK_SET);
    assert(SDL_RWtell(context) == original_pos);

    return hash;
}

// This function generates a new seek-table for a given mp3 stream and writes
// the data to the fast-seek file.
//
uint64_t generate_new_seek_points(const char* filename,
                                const uint64_t& stream_hash,
                                drmp3* const p_dr,
                                seek_points_table_t& seek_points_table,
                                frame_count_table_t& pcm_frame_count_table,
                                vector<drmp3_seek_point_serial>& seek_points_vector) {

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
                                         reinterpret_cast<drmp3_seek_point*>(seek_points_vector.data()));

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

    // Update our lookup table file with the new seek points and pcm_frame_count.
    // Note: the serializer elegantly handles C++ STL objects and is endian-safe.
    seek_points_table[stream_hash] = seek_points_vector;
    pcm_frame_count_table[stream_hash] = pcm_frame_count;
    ofstream outfile(filename, ios_base::trunc | ios_base::binary);

    // Caching our seek table to file is optional.  If the user is blocked due to
    // security or write-access issues, then this write-phase is skipped. In this
    // scenario the seek table will be generated on-the-fly on every start of DOSBox.
    if (outfile.is_open()) {
        Archive<ofstream> serialize(outfile);
        serialize << SEEK_TABLE_IDENTIFIER << seek_points_table << pcm_frame_count_table;
        outfile.close();
    }

    // Finally, we return the number of decoded PCM frames for this given file, which
    // doubles as a success-code.
    return pcm_frame_count;
}

// This function attempts to fetch a seek-table for a given mp3 stream from the fast-seek file.
// If anything is amiss then this function fails.
//
uint64_t load_existing_seek_points(const char* filename,
                                 const uint64_t& stream_hash,
                                 seek_points_table_t& seek_points_table,
                                 frame_count_table_t& pcm_frame_count_table,
                                 vector<drmp3_seek_point_serial>& seek_points) {

    // The below sentinals sanity check and read the incoming
    // file one-by-one until all the data can be trusted.

    // Sentinal 1: bail if we got a zero-byte file.
    struct stat buffer;
    if (stat(filename, &buffer) != 0) {
        return 0;
    }

    // Sentinal 2: Bail if the file isn't big enough to hold our identifier.
    if (get_file_size(filename) < static_cast<int64_t>(sizeof(SEEK_TABLE_IDENTIFIER))) {
        return 0;
    }

    // Sentinal 3: Bail if we don't get a matching identifier.
    string fetched_identifier;
    ifstream infile(filename, ios_base::binary);
    Archive<ifstream> deserialize(infile);
    deserialize >> fetched_identifier;
    if (fetched_identifier != SEEK_TABLE_IDENTIFIER) {
        infile.close();
        return 0;
    }

    // De-serialize the seek point and pcm_count tables.
    deserialize >> seek_points_table >> pcm_frame_count_table;
    infile.close();

    // Sentinal 4: does the seek_points table have our stream's hash?
    const auto p_seek_points = seek_points_table.find(stream_hash);
    if (p_seek_points == seek_points_table.end()) {
        return 0;
    }

    // Sentinal 5: does the pcm_frame_count table have our stream's hash?
    const auto p_pcm_frame_count = pcm_frame_count_table.find(stream_hash);
    if (p_pcm_frame_count == pcm_frame_count_table.end()) {
        return 0;
    }

    // If we made it here, the file was valid and has lookup-data for our
    // our desired stream
    seek_points = p_seek_points->second;
    return p_pcm_frame_count->second;
}

// This function attempts to populate our seek table for the given mp3 stream, first
// attempting to read it from the fast-seek file and (if it can't be read for any reason), it
// calculates new data.  It makes use of the above two functions.
//
uint64_t populate_seek_points(struct SDL_RWops* const context,
                              mp3_t* p_mp3,
                              const char* seektable_filename,
                              bool &result) {

    // assume failure until proven otherwise
    result = false;

    // Calculate the stream's xxHash value.
    const auto stream_hash = calculate_stream_hash(context);
    if (stream_hash == 0) {
        // LOG_MSG("MP3: could not compute the hash of the stream");
        return 0;
    }

    // Attempt to fetch the seek points and pcm count from an existing look up table file.
    seek_points_table_t seek_points_table;
    frame_count_table_t pcm_frame_count_table;
    auto pcm_frame_count = load_existing_seek_points(seektable_filename,
                                                     stream_hash,
                                                     seek_points_table,
                                                     pcm_frame_count_table,
                                                     p_mp3->seek_points_vector);

    // Otherwise calculate new seek points and save them to the fast-seek file.
    if (pcm_frame_count == 0) {
        pcm_frame_count = generate_new_seek_points(seektable_filename,
                                                   stream_hash,
                                                   p_mp3->p_dr,
                                                   seek_points_table,
                                                   pcm_frame_count_table,
                                                   p_mp3->seek_points_vector);
        if (pcm_frame_count == 0) {
            // LOG_MSG("MP3: could not load existing or generate new seek points for the stream");
            return 0;
        }
    }

    // Finally, regardless of which scenario succeeded above, we now have our seek points!
    // We bind our seek points to the dr_mp3 object which will be used for fast seeking.
    if(drmp3_bind_seek_table(p_mp3->p_dr,
                             static_cast<uint32_t>(p_mp3->seek_points_vector.size()),
                             reinterpret_cast<drmp3_seek_point*>(p_mp3->seek_points_vector.data()))
                             != DRMP3_TRUE) {
        return 0;
    }

    result = true;
    return pcm_frame_count;
}
