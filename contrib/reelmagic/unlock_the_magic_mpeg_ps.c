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



/*


http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html

http://www.mpucoder.com/DVD/mpeg-1_pes-hdr.html


*/



#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>








struct pes_substream_buffer;
struct mpeg1_pes_header;
struct gop_time_code;
struct substream;
struct run_state;


typedef int (*obj_reader_t)(struct run_state * const /* rs */, const unsigned char /* stream_id */);
typedef int (*psubstr_ingester_t)(struct substream * const /* substream, */, const unsigned char /* stream_id */);

struct pes_substream_buffer {
  unsigned char buf[64 * 1024];
  unsigned char *ptr;
  unsigned len;
};

struct mpeg1_pes_header {
  unsigned have_pstdbuf, have_pts, have_dts;
  unsigned pstdBufferScale;
  unsigned pstdBufferSize;
  uint64_t pts;
  uint64_t dts;
};

struct gop_time_code {
  unsigned char hour;
  unsigned char minute;
  unsigned char second;
  unsigned char frame;
};


struct substream {
  unsigned char                stream_id; /* 0 for unknown */
  struct mpeg1_pes_header      pes_header;
  struct pes_substream_buffer  data;
  psubstr_ingester_t           ingesters[256];

  union {
    struct {
      struct gop_time_code last_gop_time_code;
      unsigned gop_count;
      unsigned total_picture_count;
      unsigned picture_count_since_last_pictype_i;
      unsigned pictype_i_count;
      unsigned pictype_p_count;
      unsigned pictype_b_count;
      unsigned pictype_d_count;

      uint64_t last_picture_i_pts;
      uint64_t last_picture_p_pts;
      uint64_t last_picture_b_pts;
      uint64_t last_picture_d_pts;

    } video_stats;
    struct {

    } audio_stats;
  };
};


struct run_state {
  FILE                 *fp;
  const char           *filename;

  obj_reader_t          top_level_obj_readers[256];
  unsigned              top_level_stream_id_counters[256];


  /* video-specific stuff */
  struct substream      video_stream;

  /* audio-specific stuff */
  struct substream      audio_stream;
};

static int
mpgstream_readall(const struct run_state * rs, void * const buf, const unsigned size) {
  return fread(buf, size, 1, rs->fp);
}




/* hacky globals just to quickly make this work */
static FILE *_fpout = NULL;
static unsigned _user_specified_magical_f_f_code = 0;
static unsigned _user_specified_magical_b_f_code = 0;
static unsigned _apply_magical_correction = 0;

static void
outwrite(const void * const buf, const unsigned size) {
  fwrite(buf, size, 1, _fpout); /* XXX retval !? */
}

static void
outwrite_startcode(const unsigned char stream_id) {
  const unsigned char buf[] = {0,0,1,stream_id};
  outwrite(buf, sizeof(buf));
}

static void
outwrite_object(const unsigned char stream_id, const void * const buf, const unsigned size) {
  outwrite_startcode(stream_id);
  outwrite(buf, size);
}

static void
outwrite_packet(const unsigned char stream_id, const unsigned pes_len, const void * const content_buf) {
  unsigned char len_buf[2];
  len_buf[0] = (unsigned char)(pes_len >> 8);
  len_buf[1] = (unsigned char)(pes_len);
  outwrite_startcode(stream_id);
  outwrite(len_buf, sizeof(len_buf));
  outwrite(content_buf, pes_len);
}



















#define RET_FAIL(MSG) {fprintf(stderr, "%s\n", MSG); return -1;}
#define RET_FAILF(FMT, ...) {fprintf(stderr, FMT "\n", __VA_ARGS__); return -1;}





/*
  general PES stuff
*/

static inline void
increment_pes_substream_buffer(struct pes_substream_buffer * const subbuf, const unsigned length) {
  subbuf->ptr += length;
  subbuf->len -= length;
}
static void
shift_pes_substream_buffer_remainder(struct pes_substream_buffer * const subbuf) {
  if (subbuf->ptr == subbuf->buf) return;
  if (subbuf->len == 0) {
    subbuf->ptr = subbuf->buf;
    return;
  }
  memmove(subbuf->buf, subbuf->ptr, subbuf->len);
  subbuf->ptr = subbuf->buf;
}

static inline int
parse_mpeg1_pts_dts_field(uint64_t * const output, const unsigned char * const buf) {
  /* validate */
  if ((buf[4] & 0x01) != 0x01) return 0; /* failed */
  if ((buf[2] & 0x01) != 0x01) return 0; /* failed */
  if ((buf[0] & 0x01) != 0x01) return 0; /* failed */

  /* extract the 33-bit integer from this ridiculously insane encoding... */
  (*output)   = (buf[0] & 0x0E) >> 1;          /* 3 bits */
  (*output) <<= 8; (*output) |= buf[1];        /* 8 bits */
  (*output) <<= 7; (*output) |= buf[2] >> 1;   /* 7 bits */
  (*output) <<= 8; (*output) |= buf[3];        /* 8 bits */
  (*output) <<= 7; (*output) |= buf[4] >> 1;   /* 7 bits */

  return 1; /* success */
}

static int
read_mpeg1_pes_header(const struct run_state * const rs, struct mpeg1_pes_header * const output, const unsigned max_length, const unsigned char stream_id) {
  unsigned bytes_read;
  unsigned pad_ok;
  unsigned char buf[10];

  bytes_read = 0;
  pad_ok = 1;

  /* clear out only the "have" fields so that the values can be used historically */
  output->have_pstdbuf = output->have_pts = output->have_dts = 0;

  while ((bytes_read < max_length) && (!(output->have_pstdbuf && output->have_pts))) {
    /* read in a byte */
    if (mpgstream_readall(rs, buf, 1) != 1) RET_FAILF("Failed to read in MPEG-1 PES header byte for stream id 0x%02hhX", stream_id);
    outwrite(buf, 1);
    ++bytes_read;
    if (pad_ok && (buf[0] == 0xFF)) continue; /* ignore if padding */
    pad_ok = 0; /* padding not OK after the beginning */

    /* figure out what it means... */
    if ((buf[0] & 0xC0) == 0x40) {
      /* first two bits = '01' which means we have a P-STD buffer size field present */
      if (output->have_pstdbuf) RET_FAILF("PES header contains multiple P-STD buffer size fields for stream ID type 0x%02hhX", stream_id);
      if (((max_length - bytes_read) < 1) || (mpgstream_readall(rs, &buf[1], 1) != 1)) RET_FAILF("Failed to read in MPEG-1 PES header byte for STD buffer extension for stream id 0x%02hhX", stream_id);
      outwrite(&buf[1], 1);
      output->pstdBufferScale = (buf[0] & 0x20) ? 1024 : 128;
      output->pstdBufferSize   = buf[0] & 0x1F;
      output->pstdBufferSize <<= 8;
      output->pstdBufferSize  |= buf[1];
      output->have_pstdbuf = 1;
      bytes_read += 1;
    }
    else if ((buf[0] & 0xF0) == 0x20) {
      /* only PTS field present */
      if (output->have_pts || output->have_dts) RET_FAILF("PES header contains multiple PTS/DTS fields for stream ID type 0x%02hhX", stream_id);
      if (((max_length - bytes_read) < 4) || (mpgstream_readall(rs, &buf[1], 4) != 1)) RET_FAILF("Failed to read in MPEG-1 PES header bytes for PTS-only field for stream id 0x%02hhX", stream_id);
      outwrite(&buf[1], 4);
      if (!parse_mpeg1_pts_dts_field(&output->pts, buf))
        RET_FAILF("PES header PTS-only decode failed for stream ID type 0x%02hhX", stream_id);
      output->have_pts = 1;
      bytes_read += 4;
    }
    else if ((buf[0] & 0xF0) == 0x30) {
      /* PTS and DTS field present */
      if (output->have_pts || output->have_dts) RET_FAILF("PES header contains multiple PTS/DTS fields for stream ID type 0x%02hhX", stream_id);
      if (((max_length - bytes_read) < 9) || (mpgstream_readall(rs, &buf[1], 9) != 1)) RET_FAILF("Failed to read in MPEG-1 PES header bytes for PTS+DTS field for stream id 0x%02hhX", stream_id);
      outwrite(&buf[1], 9);
      if (!parse_mpeg1_pts_dts_field(&output->pts, buf))
        RET_FAILF("PES header PTS decode failed for stream ID type 0x%02hhX", stream_id);
      if (!parse_mpeg1_pts_dts_field(&output->dts, &buf[5]))
        RET_FAILF("PES header DTS decode failed for stream ID type 0x%02hhX", stream_id);
      output->have_pts = output->have_dts = 1;
      bytes_read += 9;
    }
    else if (buf[0] == 0x0f) {
      /* done with header */
      break;
    }
    else {
      RET_FAILF("Invalid MPEG-1 PES header: Unknown byte 0x%02hhX for stream ID type 0x%02hhX", buf[0], stream_id);
    }
  }

  return bytes_read;
}

static int
read_pes_append_substream(const struct run_state * const rs, struct substream * const substream) {
  unsigned char buf[2];
  unsigned pes_len;
  int header_length;

  /* init substream data buffer ptr to beginning of buffer if not previously done */
  if (substream->data.ptr == NULL) substream->data.ptr = substream->data.buf;

  /* read in the length of this sequence */
  if (mpgstream_readall(rs, buf, sizeof(buf)) != 1)
    RET_FAILF("Failed to read substream pes len for 0x%02hhX", substream->stream_id);
  pes_len   = buf[0];
  pes_len <<= 8;
  pes_len  |= buf[1];

  outwrite_startcode(substream->stream_id);
  outwrite(buf, sizeof(buf));

  /* read in the header */
  header_length = read_mpeg1_pes_header(rs, &substream->pes_header, pes_len, substream->stream_id);
  if (header_length == -1) return -1;
  pes_len -= header_length;

  /* read in / append any remaining bytes for this sequence */
  if (pes_len > 0) {
    if ((pes_len + substream->data.len + (substream->data.ptr - substream->data.buf)) > sizeof(substream->data.buf))
      RET_FAILF("Not enough space to buffer continuing ES data for stream id type 0x%02hhx", substream->stream_id);
    if (mpgstream_readall(rs, &substream->data.ptr[substream->data.len], pes_len) != 1)
      RET_FAILF("Failed to read substream ES data for 0x%02hhX", substream->stream_id);
    substream->data.len += pes_len;
  }

  return pes_len; /* success */
}

static int
dispatch_pes_substream_objects(struct substream * const substream, const void * const outbuf, const unsigned outbufsize) {
  psubstr_ingester_t ingester_func;
  int                ingester_func_result;
  unsigned char      substream_id;


  while (substream->data.len >= 4) {
    if ((substream->data.ptr[0] != 0x00) || (substream->data.ptr[1] != 0x00) || (substream->data.ptr[2] != 0x01))
      RET_FAILF("Bad substream object start code prefix for stream id: 0x%02hhX\n", substream->stream_id);
    substream_id = substream->data.ptr[3];
    if ((ingester_func = substream->ingesters[substream_id]) == NULL)
      RET_FAILF("No ingester function for stream id: 0x%02hhX:0x%02hhX\n", substream->stream_id, substream_id);
    increment_pes_substream_buffer(&substream->data, 4);

    ingester_func_result = (*ingester_func)(substream, substream_id);
    if (ingester_func_result == -2) {
      /* not enough data for ingester function */
      /* rewind back to start code and try again later */
      substream->data.ptr -= 4;
      substream->data.len += 4;
      break;
    }
    else if (ingester_func_result < 0) {
      return -1; /* something failed... bailout... */
    }
    increment_pes_substream_buffer(&substream->data, ingester_func_result);
  }
  
  outwrite(outbuf, outbufsize);
  shift_pes_substream_buffer_remainder(&substream->data);
  return 1;
}






/*
 * "video PES" processing functions...
*/
static int
ingest_pes_sequence_header(struct substream * const substream, const unsigned char stream_id) {
  const unsigned expected_content_size = 8;

  if (substream->data.len >= 4) {
    /* need to catch this even if we don't have a complete header */
    if ((substream->data.ptr[3] & 0xF) >= 0x9) {
      fprintf(stderr, "Magical stream detected. Applying static f_code.\n");
      _apply_magical_correction = 1;
      substream->data.ptr[3] &= 0xF7; /* XXX is this correct !? or should it be hardcoded to 0x5 */
    }
  }

  if (substream->data.len < expected_content_size) return 0; /* need more bytes */
  fprintf(stderr, "Sequence:\n");
  fprintf(stderr, "  - Picture Size:      %ux%u\n",
    (substream->data.ptr[0] << 4) | (substream->data.ptr[1] >> 4),
    (substream->data.ptr[1] << 8) | substream->data.ptr[2]);
  fprintf(stderr, "  - Aspect Radio Code: 0x%02X\n", substream->data.ptr[3] >> 4);
  fprintf(stderr, "  - Frame Rate Code:   0x%02X\n", substream->data.ptr[3] & 0xF);
  return (int)expected_content_size;
}
static int
ingest_gop(struct substream * const substream, const unsigned char stream_id) {
  const unsigned expected_content_size = 4;

  if (substream->data.len < expected_content_size) return -2; /* need more bytes */
  ++substream->video_stats.gop_count;

  substream->video_stats.last_gop_time_code.hour     = (substream->data.ptr[0] >> 2) & 0x1f;
  substream->video_stats.last_gop_time_code.minute   = substream->data.ptr[0] & 0x03;
  substream->video_stats.last_gop_time_code.minute <<= 4;
  substream->video_stats.last_gop_time_code.minute  |= (substream->data.ptr[1] >> 4)  & 0x0f;
  substream->video_stats.last_gop_time_code.second   = substream->data.ptr[1] & 0x07;
  substream->video_stats.last_gop_time_code.second <<= 3;
  substream->video_stats.last_gop_time_code.second  |= (substream->data.ptr[2] >> 5) & 0x07;
  substream->video_stats.last_gop_time_code.frame    = substream->data.ptr[2] & 0x1f;
  substream->video_stats.last_gop_time_code.frame  <<= 1;
  substream->video_stats.last_gop_time_code.frame   |= (substream->data.ptr[3] >> 7)  & 0x01;
//fprintf(stderr, "GOP Time Code:   %02hhu:%02hhu:%02hhu.%02hhu\n", substream->video_stats.last_gop_time_code.hour, substream->video_stats.last_gop_time_code.minute, substream->video_stats.last_gop_time_code.second, substream->video_stats.last_gop_time_code.frame);
  return (int)expected_content_size;
}
static int
ingest_unknown(struct substream * const substream, const unsigned char stream_id) {
  unsigned char *next_substream_object;

  const unsigned char startcode[] = {0x00, 0x00, 0x01};

  next_substream_object = memmem(substream->data.ptr, substream->data.len, startcode, sizeof(startcode));
  if (next_substream_object == NULL) return -2;
  return next_substream_object - substream->data.ptr;
}
static int
ingest_picture(struct substream * const substream, const unsigned char stream_id) {
  unsigned expected_content_size;
  unsigned char picture_type;
  unsigned char *next_substream_object;

  const unsigned char startcode[] = {0x00, 0x00, 0x01};

  expected_content_size = 4;
  if (substream->data.len < expected_content_size) return -2; /* need more bytes */

  picture_type = (substream->data.ptr[1] >> 3) & 0x07;
  if ((picture_type == 2) || (picture_type == 3)) {
    if (_apply_magical_correction) {
      /* need to catch this even if we don't have a complete header */
      substream->data.ptr[3] &= 0xFC;
      substream->data.ptr[3] |= (unsigned char)((_user_specified_magical_f_f_code >> 1) & 0x3);
    }
    if (substream->data.len < ++expected_content_size) return -2; /* need more bytes */
    if (_apply_magical_correction) {
      substream->data.ptr[4] &= 0x47;
      substream->data.ptr[4] |= (unsigned char)((_user_specified_magical_f_f_code & 0x1) << 7);
      substream->data.ptr[4] |= (unsigned char)((_user_specified_magical_b_f_code & 0x7) << 3);
    }
  }

  next_substream_object = memmem(substream->data.ptr, substream->data.len, startcode, sizeof(startcode));
  if (next_substream_object == NULL) return -2; /* need more bytes */
  if (expected_content_size > (next_substream_object - substream->data.ptr)) {
    fprintf(stderr, "Picture header parse error\n");
    return -1; /* fatal error */
  }
  expected_content_size = next_substream_object - substream->data.ptr;

  switch(picture_type) {
  case 1: /* I picture */
    substream->video_stats.picture_count_since_last_pictype_i = 0;
    substream->video_stats.last_picture_i_pts = substream->pes_header.pts;
    ++substream->video_stats.pictype_i_count;
    break;
  case 2: /* P picture */
    if (_apply_magical_correction) 
      fprintf(stderr, "Patched P Picture #%u in GOP %u at PTS %lu\n", substream->video_stats.total_picture_count, (unsigned)(substream->video_stats.gop_count - 1), (unsigned long)substream->pes_header.pts);
    ++substream->video_stats.picture_count_since_last_pictype_i;
    ++substream->video_stats.pictype_p_count;
    substream->video_stats.last_picture_p_pts = substream->pes_header.pts;
    break;
  case 3: /* B picture */
    if (_apply_magical_correction)
      fprintf(stderr, "Patched B Picture #%u in GOP %u at PTS %lu\n", substream->video_stats.total_picture_count, (unsigned)(substream->video_stats.gop_count - 1), (unsigned long)substream->pes_header.pts);
    ++substream->video_stats.picture_count_since_last_pictype_i;
    ++substream->video_stats.pictype_b_count;
    substream->video_stats.last_picture_b_pts = substream->pes_header.pts;
    break;
  case 4: /* D picture */
    ++substream->video_stats.picture_count_since_last_pictype_i;
    ++substream->video_stats.pictype_d_count;
    substream->video_stats.last_picture_d_pts = substream->pes_header.pts;
    break;
  default:
    RET_FAILF("Unknown picture type #%u\n", substream->video_stats.total_picture_count);
  }

  ++substream->video_stats.total_picture_count;

  return (int)expected_content_size;
}
static int
ingest_slice(struct substream * const substream, const unsigned char stream_id) {
  return ingest_unknown(substream, stream_id); /* TBD */
}
static int
ingest_end(struct substream * const substream, const unsigned char stream_id) {
  return 0;
}
/*
 * end of "video PES" processing functions...
*/









/*
 * "top level" processing functions...
*/

static int
read_program_end_object(struct run_state * const rs, const unsigned char stream_id) {
  outwrite_startcode(stream_id);
  return 0; /* success; this is the end of the stream to return 0 indicating so */
}

static int
read_pack_header_object(struct run_state * const rs, const unsigned char stream_id) {
  unsigned char header_content[8];

  if (mpgstream_readall(rs, header_content, sizeof(header_content)) != 1)
    RET_FAIL("Failed to read pack header content");

  outwrite_object(stream_id, header_content, sizeof(header_content));

  static int do_once = 1;
  if (do_once) {
    fprintf(stderr, "incomplete parsing of PACK header(s)!\n");
    do_once = 0;
  }
  return 1; /* success */
}

static int
read_system_header_object(struct run_state * const rs, const unsigned char stream_id) {
  unsigned char header_content[14];

  if (mpgstream_readall(rs, header_content, sizeof(header_content)) != 1)
    RET_FAIL("Failed to read system header content");

  outwrite_object(stream_id, header_content, sizeof(header_content));

  static int do_once = 1;
  if (do_once) {
    fprintf(stderr, "incomplete parsing of SYSTEM header(s)!\n");
    do_once = 0;
  }
  return 1; /* success */
}

static int
discard_pes_object_content(struct run_state * const rs, const unsigned char stream_id) {
  unsigned char buf[64*1024];
  unsigned pes_len;

  if (mpgstream_readall(rs, buf, 2) != 1)
    RET_FAILF("Failed to read padding stream pes len for 0x%02hhX", stream_id);
  pes_len   = buf[0];
  pes_len <<= 8;
  pes_len  |= buf[1];

  if (mpgstream_readall(rs, buf, pes_len) != 1)
    RET_FAILF("Failed to slurp discarded PES packet content for 0x%02hhX", stream_id);

  outwrite_packet(stream_id, pes_len, buf);

  return 1; /* success */
}

static int
read_video_stream_object(struct run_state * const rs, const unsigned char stream_id) {
  const void *preappend_ptr;
  int bytes_appended;

  /* currently only one video ES supported */
  if (rs->video_stream.stream_id != stream_id) {
    if (rs->video_stream.stream_id != 0) {
      /* ignore other video streams */
      return discard_pes_object_content(rs, stream_id);
    }
    /* first video ES found will be the "primary" */
    rs->video_stream.stream_id = stream_id;
    fprintf(stderr, "Discovered Video ES @ 0x%02hhX\n", rs->video_stream.stream_id);
  }

  /* read the PES header and append some PES data */
  if (rs->video_stream.data.ptr == NULL) rs->video_stream.data.ptr = rs->video_stream.data.buf;
  preappend_ptr = &rs->video_stream.data.ptr[rs->video_stream.data.len];
  bytes_appended = read_pes_append_substream(rs, &rs->video_stream);
  if (bytes_appended == -1) return -1;
#if 0
fprintf(stderr, "Have PSTDBUF: %u\n", rs->video_stream.pes_header.have_pstdbuf);
fprintf(stderr, "Have PTS: %u\n",     rs->video_stream.pes_header.have_pts);
fprintf(stderr, "Have DTS: %u\n",     rs->video_stream.pes_header.have_dts);
fprintf(stderr, "pstdBufferScale: %u\n",     rs->video_stream.pes_header.pstdBufferScale);
fprintf(stderr, "pstdBufferSize: %u\n",     rs->video_stream.pes_header.pstdBufferSize);
fprintf(stderr, "pts: %lu\n",     (unsigned long)rs->video_stream.pes_header.pts);
fprintf(stderr, "dts: %lu\n",     (unsigned long)rs->video_stream.pes_header.dts);
#endif

  /* process any complete sub-items within this elementary stream */
  return dispatch_pes_substream_objects(&rs->video_stream, preappend_ptr, bytes_appended);
}

static int
read_audio_stream_object(struct run_state * const rs, const unsigned char stream_id) {
  /* currently only one audio ES supported */
  if (rs->audio_stream.stream_id != stream_id) {
    if (rs->audio_stream.stream_id != 0) {
      /* ignore other audio streams */
      return discard_pes_object_content(rs, stream_id);
    }
    /* first audio ES found will be the "primary" */
    rs->audio_stream.stream_id = stream_id;
    fprintf(stderr, "Discovered Audio ES @ 0x%02hhX\n", rs->audio_stream.stream_id);
  }

  /* XXX do nothing for now */
  return discard_pes_object_content(rs, stream_id);

  /* read the PES header and append some PES data */
  if (read_pes_append_substream(rs, &rs->audio_stream) == -1) return -1;

  return 1; /* success */
}

static int
read_top_level_object(struct run_state * const rs) {
  unsigned char header[4];
  obj_reader_t  reader_func;

  switch(mpgstream_readall(rs, header, sizeof(header))) {
  case 0:  return 0;   /* end of stream */
  case -1: return -1;  /* failure */
  }

  if ((header[0] != 0x00) || (header[1] != 0x00) || (header[2] != 0x01))
    RET_FAIL("Bad object start code prefix");

  if ((reader_func = rs->top_level_obj_readers[header[3]]) == NULL)
    RET_FAILF("No reader function for stream ID type 0x%02hhX", header[3]);

  ++rs->top_level_stream_id_counters[header[3]];
  return (*reader_func)(rs, header[3]);
}
/*
 * end of "top level" processing functions...
*/







static void
populate_stream_id_handlers(struct run_state * const rs) {
  unsigned i;

  /* "top level" stuff */
  rs->top_level_obj_readers[0xB9] = &read_program_end_object;
  rs->top_level_obj_readers[0xBA] = &read_pack_header_object;
  rs->top_level_obj_readers[0xBB] = &read_system_header_object;
  rs->top_level_obj_readers[0xBE] = &discard_pes_object_content;
  for (i = 0xc0; i <= 0xdf; ++i)
    rs->top_level_obj_readers[i] = &read_audio_stream_object;
  for (i = 0xe0; i <= 0xef; ++i)
    rs->top_level_obj_readers[i] = &read_video_stream_object;



  /* video stuff */
  rs->video_stream.ingesters[0x00] = &ingest_picture;
  rs->video_stream.ingesters[0xB2] = &ingest_unknown;
  rs->video_stream.ingesters[0xB3] = &ingest_pes_sequence_header;
  rs->video_stream.ingesters[0xB7] = &ingest_end;
  rs->video_stream.ingesters[0xB8] = &ingest_gop;
  rs->video_stream.ingesters[0xB9] = &ingest_gop;
  for (i = 0x01; i <= 0xaf; ++i)
    rs->video_stream.ingesters[i] = &ingest_slice;
}

int
main( int argc, char *argv[] ) {
  struct run_state rs;
  unsigned i;

  if (argc != 4) {
    fprintf(stderr, "Usage: %s F_CODE INPUT_FILE OUTPUT_FILE\n", argv[0]);
    return 1;
  }

  bzero(&rs, sizeof(rs));
  populate_stream_id_handlers(&rs);

  _user_specified_magical_f_f_code = (unsigned)atoi(argv[1]);
  if ((_user_specified_magical_f_f_code < 1) || (_user_specified_magical_f_f_code > 7)) {
    fprintf(stderr, "Invalid f_code %u. Acceptable range is 1-7\n", _user_specified_magical_f_f_code);
    return 1;
  }
  _user_specified_magical_b_f_code = _user_specified_magical_f_f_code;

  rs.filename = argv[2];
  rs.fp = fopen(rs.filename, "rb");
  if (rs.fp == NULL) {
    fprintf(stderr, "Failed to open '%s' for reading", rs.filename);
    perror("");
    return 1;
  };

  _fpout = fopen(argv[3], "wb");
  if (rs.fp == NULL) {
    fprintf(stderr, "Failed to open '%s' for writing", argv[2]);
    perror("");
    return 1;
  };

  while ( 1 ) switch(read_top_level_object(&rs)) {
  case -1: /* failure occured */
    fclose(rs.fp);
    return 1;
  case 0:  /* analyze success */
    goto done_success;
  }

done_success:
  fclose(_fpout);
  fclose(rs.fp);
  fprintf(stderr, "Top-Level Stream ID Counters:\n");
  for (i = 0; i <= 0xFF; ++i) {
    if (rs.top_level_stream_id_counters[i] == 0) continue;
    fprintf(stderr, "  . 0x%02X: %u\n", i, rs.top_level_stream_id_counters[i]);
  }
  if (!rs.top_level_stream_id_counters[0xB9]) {
    fprintf(stderr, "WARNING: Stream terminated prematuraly; no 'program end' found!\n");
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "Video Elementary Stream Statistics:\n");
  fprintf(stderr, "  . Video Stream ID: 0x%02hhX\n", rs.video_stream.stream_id);
//  fprintf(stderr, "  . Last GOP Time:   %02hhu:%02hhu:%02hhu.%02hhu\n", rs.video_stream.video_stats.last_gop_time_code.hour, rs.video_stream.video_stats.last_gop_time_code.minute, rs.video_stream.video_stats.last_gop_time_code.second, rs.video_stream.video_stats.last_gop_time_code.frame);
  fprintf(stderr, "  . GOP Count:       %u\n",       rs.video_stream.video_stats.gop_count);
  fprintf(stderr, "  . Picture Count:   %u\n",       rs.video_stream.video_stats.total_picture_count);
  fprintf(stderr, "  . I-Picture Count: %u\n",       rs.video_stream.video_stats.pictype_i_count);
  fprintf(stderr, "  . P-Picture Count: %u\n",       rs.video_stream.video_stats.pictype_p_count);
  fprintf(stderr, "  . B-Picture Count: %u\n",       rs.video_stream.video_stats.pictype_b_count);
  fprintf(stderr, "  . D-Picture Count: %u\n",       rs.video_stream.video_stats.pictype_d_count);
  fprintf(stderr, "\n");
  fprintf(stderr, "Audio Elementary Stream Statistics:\n");
  fprintf(stderr, "  . Audio Stream ID: 0x%02hhX\n", rs.audio_stream.stream_id);
  fprintf(stderr, "\n");
  fprintf(stderr, "Successfully completed analysis!\n");
  return 0;
}
