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
  unsigned result;
  unsigned temporal_seqnum;
  unsigned picture_type;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s INPUT_FILE\n", argv[0]);
    return 1;
  }

  plm_t *plm = plm_create_with_filename(argv[1]);
  if (!plm) {
    printf("Couldn't open file %s\n", argv[1]);
    return 1;
  }

  plm_set_audio_enabled(plm, FALSE);
  plm_set_loop(plm, FALSE);
  if (!plm->video_decoder) {
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
  /* XXX read rate code here !!! */
  if (plm_video_get_framerate(plm->video_decoder) != 0.000) {
    printf("Error not a magical asset\n");
    plm_destroy(plm);
    return 1;
  }

  /* this is some mighty fine half-assery... */
  result = 0;
  do {
    if (plm_buffer_find_start_code(plm->video_decoder->buffer, PLM_START_PICTURE) == -1) {
      break;
    }
    temporal_seqnum = plm_buffer_read(plm->video_decoder->buffer, 10);
    picture_type = plm_buffer_read(plm->video_decoder->buffer, 3);
    if ((picture_type == PLM_VIDEO_PICTURE_TYPE_PREDICTIVE) || (picture_type == PLM_VIDEO_PICTURE_TYPE_B)) {
      plm_buffer_skip(plm->video_decoder->buffer, 16); // skip vbv_delay
      plm_buffer_skip(plm->video_decoder->buffer, 1); //skip full_px
      result = plm_buffer_read(plm->video_decoder->buffer, 3); //forward f_code
      if (plm_buffer_next_start_code(plm->video_decoder->buffer) == PLM_START_USER_DATA) {
        /* The Horde */
        if (temporal_seqnum != 4) result = 0;
      }
      else {
        /* Return to Zork and Lord of the Rings */
        if ((temporal_seqnum != 3) && (temporal_seqnum != 8)) result = 0;
      }
    }
  } while (result == 0);
  plm_rewind(plm);
  plm_destroy(plm);


  if (result) {
    printf("Found f_code: %u\n", result);
    return 0;
  }

  printf("Not Found\n");
  return 1;
}
