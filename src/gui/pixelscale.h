/* This unit currently supports only graphical modes
   with one byte per component, therefore:
	TODO: Consider using color masks to support arbitrary
	      color modes.
*/

#ifndef PIXEL_SCALE
#define PIXEL_SCALE

typedef struct ps_rect
{	int x, y, w, h;  } ps_rect;

typedef struct ps_format
{	unsigned char offs;
	unsigned char step;
} ps_format;

typedef struct ps_size
{	unsigned w;
	unsigned h;
} ps_size;

typedef struct ps_pixels
{	unsigned char* pixels;
	unsigned       pitch;
} ps_pixels;

        struct ps_info_internal;
typedef struct ps_info_internal* ps_info;

ps_info ps_new_soft
(	ps_format     const fmt_in,
	ps_size       const size_in,
	ps_format     const fmt_out,
	ps_size       const size_out,
	char          const dw,
	char          const dh,
	unsigned char const comp_n,
	double        const softness
);

ps_info ps_new_nn
(	ps_format     const fmt_in,
	ps_size       const size_in,
	ps_format     const fmt_out,
	ps_size       const size_out,
	char          const dw,
	char          const dh,
	unsigned char const comp_n
);

ps_info ps_new_perfect
(	ps_format     const fmt_in,
	ps_size       const size_in,
	ps_format     const fmt_out,
	ps_size       const size_out,
	char          const dw,
	char          const dh,
	unsigned char const comp_n,
	double        const aspect_in,
	ps_size*      const size_res
);

void ps_scale( ps_info const si, ps_pixels const pix_in, ps_pixels const pix_out, ps_rect* area );
void ps_free ( ps_info const si );
#endif
