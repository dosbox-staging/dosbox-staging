/*
 *  Copyright (C) 2022 Jon Dennis
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>

#define PL_MPEG_IMPLEMENTATION
#include "hardware/reelmagic/mpeg_decoder.h"


int
main(int argc, char *argv[]) {
  unsigned frame_rate_code;
  unsigned format_ves;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s INPUT_FILE\n", argv[0]);
    return 1;
  }

  plm_t *plm = plm_create_with_filename(argv[1]);
  if (!plm) {
    printf("Couldn't open file %s\n", argv[1]);
    return 1;
  }

  format_ves = 0;

  plm_set_audio_enabled(plm, FALSE);
  if (!plm->video_decoder) {
    format_ves = 1;
    if (plm->audio_decoder) {
      plm_audio_destroy(plm->audio_decoder);
      plm->audio_decoder = NULL;
    }
    plm_demux_rewind(plm->demux);
    plm->has_decoders = TRUE;
    plm->video_packet_type = PLM_DEMUX_PACKET_VIDEO_1;
    if (plm->video_decoder) plm_video_destroy(plm->video_decoder);
    plm->video_decoder = plm_video_create_with_buffer(plm->demux->buffer, FALSE);
  }
  plm_rewind(plm);

  if (plm_buffer_find_start_code(plm->video_decoder->buffer, PLM_START_SEQUENCE) == -1) {
    printf("Error likely not an MPEG-1 video!\n");
    plm_destroy(plm);
    return 1;
  }

  plm_buffer_skip(plm->video_decoder->buffer, 12); //skip width
  plm_buffer_skip(plm->video_decoder->buffer, 12); //skip height
  plm_buffer_skip(plm->video_decoder->buffer, 4);  //skip PAR
  frame_rate_code = plm_buffer_read(plm->video_decoder->buffer, 4);

  plm_destroy(plm);

  if (frame_rate_code & 0x8) {
    printf("Magical MPEG-1 %s asset detected. Frame rate code=0x%X\n", format_ves ? "ES" : "PS", frame_rate_code);
    return 0;
  }
  else if (frame_rate_code == 0) {
    printf("Bad MPEG-1 %s asset detected. Frame rate code=0x%X\n", format_ves ? "ES" : "PS", frame_rate_code);
  }
  else {
    printf("Normal MPEG-1 %s asset detected. Frame rate code=0x%X\n", format_ves ? "ES" : "PS", frame_rate_code);
  }

  return 1;
}
