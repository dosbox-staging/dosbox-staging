#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "pixelscale.h"

typedef unsigned char uchar;

typedef struct line_info
{	int size;
	int left;
	int weight_l;
	int weight_r;
	int main_start;
	int main_end;
	int main_width;
} line_info;

enum NEIGHBOR
{	N_TOP, N_BOTTOM, N_LEFT, N_RIGHT  };

typedef struct cell_info
{	signed weights[4];
} cell_info;

typedef struct info
{	ps_size      size_in;
	ps_format    fmt_in;
	ps_format    fmt_out;
	uchar        comp_n;
	char         perfect; /* boolean flag */
	double       scale_x;
	double       scale_y;
	line_info*   rows_info;
	line_info*   cols_info;
	cell_info*   cells;
	uchar       *ipl,*ipr;
	uchar*       bound_cols[4];
	uchar*       rowbuf;
	char         nb;
	char         dw;
	char         dh;
} info;

typedef struct junct_info
{	uchar   low;
	uchar   high;
	double  diff_lo;
	double  diff_hi;
	double  middle;
} junct_info;

static junct_info get_junct_info( double scale, char nb )
{	junct_info ji;
	ji.low     = floor(scale);
	ji.high    = ji.low + 1;
	ji.diff_hi =  (double)ji.high - scale;
	ji.diff_lo = -(double)ji.low  + scale;
	if( nb )
	{	ji.middle = 0.0;  }
	else
	{	ji.middle = 0.0;  }
	return ji;
}

static void li_filldim( line_info *pli, int left )
{	pli->left       = left;
	pli->main_start = left;
	pli->main_end   = left + pli->size - 1;

	if( pli->weight_l < 256 )
	{	pli->main_start += 1;  }
	if( pli->weight_r < 256 )
	{	pli->main_end   -= 1;  }

	pli->main_width = pli->main_end - pli->main_start + 1;
}

static void li_setsize( junct_info ji, line_info* pli, double *diff )
{	if( *diff > ji.middle )
	{	*diff = *diff - ji.diff_hi;
		pli->size = ji.high;
	}
	else
	{	*diff = *diff + ji.diff_lo;
		pli->size = ji.low;
	}
}

static line_info *li_get( unsigned length, double scale, double softness )
{	double diff = 0.0;
	int i;
	int edgeDiff;
	char nb = (softness == 0.0);
	double dweight;
	int*   edgeWeight;
	int left = 0;
	int dummy;
	junct_info ji = get_junct_info( scale, nb );
	line_info* res = (line_info*)calloc( length, sizeof( line_info ) );
	line_info* pli = res;
	for( i = 0; i < length; i++ )
	{	res[i].weight_l = 256; res[i].weight_r = 256;  }
	for( i = 0; i < length; i++ )
	{	diff = scale * i - left;
		li_setsize( ji, pli, &diff );
		if( !nb )
		{	if( diff  > 0.0 )
			{	dweight = 1.0 - diff;
				if( i < length - 1 )
				{	edgeWeight = &(pli + 1)->weight_l;  }
				else
				{	edgeWeight = &dummy;  }
			}
			else
			{	dweight = 1.0 + diff;
				edgeWeight = &pli->weight_r;
			}
			dweight     = pow( dweight, softness );
			*edgeWeight = round( dweight * 256 );
		}

		li_filldim( pli, left );

		left = left + pli->size;
		pli = pli + 1;
	}
	return res;
}

static uchar* alloc_colors( info* si )
{	return (uchar*)calloc( si->size_in.h * si->size_in.w, si->comp_n );  }

static uchar* new_color( info* si )
{	return (uchar*)malloc( si->comp_n );  }

static void init_cells( info* si )
{	int y, x;
	line_info ri, ci;
	ps_size size_in = si->size_in;
	si->cells    = (cell_info*)calloc( sizeof( cell_info ), size_in.w * size_in.h );
	cell_info* c = si->cells;
	for( y = 0; y < size_in.h; y++ )
	{	ri = si->rows_info[y];
		for( x = 0; x < size_in.w; x++ )
		{	ci = si->cols_info[x];

			c->weights[N_TOP]    = ri.weight_l;
			c->weights[N_BOTTOM] = ri.weight_r;
			c->weights[N_LEFT]   = ci.weight_l;
			c->weights[N_RIGHT]  = ci.weight_r;

			c = c + 1;
		}
	}
}

static double rounddl( double f )
{	double f0 = floor(f);
	double f1 = f0 + 1.0;
	if( f/f0 > f1/f  )
	{	return f1; }
	else
	{	return f0;  }
}

static void get_perfect_scale_asp
( double ar_in, double a_in, double a_out, double ratio, int *scalex, int *scaley )
{	const double ASPECT_IMPORTANCE = 1.14; // Heruistic parameter
	int sx, sy, sx_best, sy_best;
	int sx_max = floor(ratio);
	int sy_max = floor( ratio * a_out / ar_in + 0.000000000000005 );
	if( sy_max == 0 )
	{	sy_max = 1;  } // HACK for small output sizes
	double bestfit = -99.0;

	double fit;
	double err_aspect;
	for( sx = sx_max; sx > 0; sx-- )
	{	sy = rounddl( (double)sx * a_in );
		if( sy == 0 )
		{	sy = 1;  } // HACK for small output sizes
		if( sy > sy_max )
		{	sy = sy - 1;  }
		err_aspect = (double)sy/(double)sx / a_in;
		if( err_aspect < 1.0 )
		{	err_aspect = 1.0 / err_aspect;  }
		err_aspect = powf( err_aspect, ASPECT_IMPORTANCE );
		double xr = (double)sx/sx_max;
		double yr = (double)sy/sy_max;
		double sz;
		if( xr > yr )
		{	sz = xr;  }
		else
		{	sz = yr;  }
		fit = (double)(sz + 0.2) / err_aspect;
		if( fit > bestfit )
		{	bestfit = fit;
			*scalex = sx;
			*scaley = sy;
		}
	}
}

/* TODO: To prevent rounding errors I have switched from float to double,    */
/*       but it is a brute rather than intelligent solution.  It will be     */
/*       better to operate with pixel measurements directly instead of       */
/*       converting them into normalized real ratios, as done currently.     */
static void get_perfect_scale
( ps_size size_in, float a_in, ps_size size_out, int* scalex, int* scaley )
{	double a_x, a_y;
	double ca_in, ca_out;
	double cratio;
	double ar;
	int *sw, *sh;
	if( a_in > 1.0 )
	{	a_y = a_in;     a_x = 1.0;  }
	else
	{	a_x = 1.0/a_in; a_y = 1.0;  }
	if( size_out.w / (size_in.w * a_x) < size_out.h / (size_in.h * a_y) )
	{	ca_in  = a_in;
		ca_out = (double)size_out.h/size_out.w;
		cratio = (double)size_out.w/size_in.w;
		sw     = scalex; sh = scaley;
		ar     = (double)size_in.h/size_in.w;
	}
	else
	{	ca_in  = 1.0/a_in;
		ca_out = (double)size_out.w/size_out.h;
		cratio = (double)size_out.h/size_in.h;
		sw     = scaley; sh = scalex;
		ar     = (double)size_in.w/size_in.h;
	}
	get_perfect_scale_asp( ar, ca_in, ca_out, cratio, sw, sh );
}

static void free_if_not_null( void *ptr )
{	if( ptr != NULL )
	{	free( ptr );  }
}

static void init_bound_cols( info* si )
{	uchar i;
	for( i = 0; i < 4; i++ )
	{	si->bound_cols[i] = new_color( si );  }
}

static void free_bound_cols( info* si )
{	uchar i;
	for( i = 0; i < 4; i++ )
	{	free_if_not_null( si->bound_cols[i] );  }
}

static ps_info new_info
(	ps_format fmt_in,
	ps_size   size_in,
	ps_format fmt_out,
	ps_size   size_out,
	uchar     comp_n,
	double    softness,
	char      dw,
	char      dh
)
{	int i;
	ps_info res = (ps_info)malloc(sizeof(info));
	info* si = (info*)res;
	si->dw        = dw;
	si->dh        = dh;
	si->comp_n    = comp_n;
	si->fmt_in    = fmt_in;
	si->size_in   = size_in;
	si->fmt_out   = fmt_out;
	si->scale_x   = ( double )size_out.w / size_in.w;
	si->scale_y   = ( double )size_out.h / size_in.h;
	si->perfect   = si->scale_x == (int)si->scale_x &&
	                si->scale_y == (int)si->scale_y;
	si->nb        = softness == 0.0 || si->perfect;
	si->rows_info = li_get( size_in.h, si->scale_y, softness );
	si->cols_info = li_get( size_in.w, si->scale_x, softness );
	si->ipl       = new_color( si );
	si->ipr       = new_color( si );
	// TODO: In case of allocation errors, call ps_free and return NULL
	// TODO: Because the exact pitch is not known at init time:
	si->rowbuf    = (uchar*)malloc( size_out.w * fmt_out.step );
	for( i = 0; i < size_out.w * fmt_out.step; i++ )
	{	si->rowbuf[i] = 0;  }
	init_cells( si );
	init_bound_cols( si );
	return res;
}

static void handle_dwh_new
( char dw, char dh, ps_format *fmt_in, ps_size* size, double* par )
{	if( dh )
	{	size->h /= 2;
		*par    *= 2;
	}
	if( dw )
	{	size->w      /= 2;
		*par         /= 2;
		fmt_in->step *= 2;
	}
}

static void handle_dwh_scale
( char dw, char dh, ps_pixels *pix_in, ps_rect* rect )
{	if( dh )
	{	pix_in->pitch *= 2;
		rect->h       /= 2;
		rect->y       /= 2;
	}
	if( dw )
	{	rect->w /= 2;
		rect->x /= 2;
	}
}

ps_info ps_new_soft
(	ps_format     const fmt_in,
	ps_size       const size_in,
	ps_format     const fmt_out,
	ps_size       const size_out,
	char          const dw,
	char          const dh,
	unsigned char const comp_n,
	double        const softness
)
{	double par_dummy;
	ps_size size_in_calc = size_in;
	ps_format fmt_in_calc = fmt_in;
	handle_dwh_new( dw, dh, &fmt_in_calc, &size_in_calc, &par_dummy );
	return new_info( fmt_in_calc, size_in_calc, fmt_out, size_out, comp_n, softness, dw, dh );
}

ps_info ps_new_nn
(	ps_format     const fmt_in,
	ps_size       const size_in,
	ps_format     const fmt_out,
	ps_size       const size_out,
	char          const dw,
	char          const dh,
	unsigned char const comp_n
)
{	double par_dummy;
	ps_size   size_in_calc = size_in;
	ps_format fmt_in_calc  = fmt_in;
	handle_dwh_new( dw, dh, &fmt_in_calc, &size_in_calc, &par_dummy );
	return new_info( fmt_in_calc, size_in_calc, fmt_out, size_out, comp_n,
		0.0, dw, dh );
}

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
)
{	int sx, sy;
	double aspect = aspect_in;
	ps_size   size_in_calc = size_in;
	ps_format fmt_in_calc  = fmt_in;
	handle_dwh_new( dw, dh, &fmt_in_calc, &size_in_calc, &aspect );
	get_perfect_scale( size_in_calc, aspect, size_out, &sx, &sy );
	size_res->w = sx * size_in_calc.w;
	size_res->h = sy * size_in_calc.h;
	return new_info( fmt_in_calc, size_in_calc, fmt_out, *size_res, comp_n,
		0.0, dw, dh );
}

void ps_free ( ps_info const osi )
{	info* si = (info*)osi;
	free_if_not_null( si->ipl );
	free_if_not_null( si->ipr );
	free_if_not_null( si->cols_info );
	free_if_not_null( si->rows_info );
	free_if_not_null( si->cells );
	free_if_not_null( si->rowbuf );
	free_bound_cols( si );
	free( si );
}

// TODO: Maybe precalculate the difference 256 - weight also?
static inline uchar interpolate_comp( unsigned weight, uchar current, uchar that )
{	return ( (int)current * weight + (int)that * ( 256 - weight ) ) >> 8;  }

static inline void interpolate_pixel( unsigned weight, uchar* current, uchar* that, int count, uchar* res )
{	int c;
	for( c = 0; c < count; c++ )
	{	*res = interpolate_comp( weight, *current, *that );
		res++; current++; that++;
	}
}

static uchar* get_nb_pix( ps_pixels image, uchar* pixel, int step, int nb )
{	int offset;
	switch (nb)
	{	case N_TOP:    offset = -image.pitch; break;
		case N_BOTTOM: offset =  image.pitch; break;
		case N_LEFT:   offset = -step; break;
		case N_RIGHT:  offset =  step; break;
	}
	return pixel + offset;
}

static inline void get_boundary_colors
	( cell_info *cell, ps_pixels pix_in, uchar* pixel, info* si  )
{	uchar i;
	signed weight;
	uchar* color;
	/* TODO: unroll this loop: */
	for( i = 0; i < 4; i++ )
	{	weight = cell->weights[i];
		if( weight < 256 )
		{	color = get_nb_pix( pix_in, pixel, si->fmt_in.step, i);
			interpolate_pixel
				( weight, pixel, color, si->comp_n, si->bound_cols[i] );
		}
	}
}

static inline uchar* addr( uchar* start, int size, int x, int y, int pitch )
{	return start + y * pitch + x * size;  }

// TODO: restructure the internal types so as to make this easier:
static inline uchar* addr_img( uchar step, ps_pixels pixels, int x, int y )
{	return addr( pixels.pixels, step, x, y, pixels.pitch );  }

static inline void put_and_shift
	( ps_format fmt, uchar comp_n, uchar** row, uchar* pixel )
{	memcpy( *row, pixel, comp_n );
	*row = *row + fmt.step;
}

static inline void fill_subrow
( info* si, uchar** row, line_info *ci, uchar* left, uchar* right, uchar* current )
{	uchar i;
	ps_format fmt_out;
	uchar comp_n;
	fmt_out = si->fmt_out;
	comp_n  = si->comp_n;
	if( ci->weight_l < 256 )
	{	put_and_shift( fmt_out, comp_n, row, left );  }
	for( i = 0; i < ci->main_width; i++ )
	{	put_and_shift( fmt_out, comp_n, row, current );  }
	if( ci->weight_r < 256 )
	{	put_and_shift( fmt_out, comp_n, row, right );  }
}

static inline uchar color_diff( uchar* a, uchar* b, uchar count )
{	unsigned full = 0;
	int comp_diff;
	uchar i;
	for( i = 0; i < count; i++ )
	{	comp_diff = *a - *b;
		if( comp_diff < 0 )
		{	comp_diff = - comp_diff;  }
		full = full + comp_diff;
		a = a + 1;
		b = b + 1;
	}
	return (uchar)(full / count);
}

static inline void interpolate_corner
( uchar comp_n, uchar* current, uchar cur_diff, uchar add_diff, uchar* other, uchar* res )
{	double dweight = (double)cur_diff / ( (double)add_diff + (double)cur_diff ) * 255;
	uchar   weight = ( uchar )round( dweight );
	interpolate_pixel( weight, current, other, comp_n, res );
}

static inline void fill_boundary
(	uchar** row, info* si, line_info *ci, ps_pixels pix_in,
	NEIGHBOR n, cell_info *cell, uchar* pixel )
{	uchar cur_diff, add_diff;
	uchar weight;
	uchar    **bounds    = si->bound_cols;
	uchar* color;
	signed *weights;
	uchar  step;

	/* TODO: Store in each cell: four fields of each wieghts and four fields of
	         each offset (for speed) */
	step = si->fmt_in.step;
	weights = cell->weights;
	color = get_nb_pix( pix_in, pixel, step, n );
	cur_diff = color_diff( color, pixel, si->comp_n );
	if( weights[N_LEFT] < 256 )
	{	color = get_nb_pix( pix_in, pixel, step, N_LEFT );
		add_diff = color_diff( color, pixel, si->comp_n );
		interpolate_corner( si->comp_n, bounds[n], cur_diff, add_diff, bounds[N_LEFT], si->ipl );
	}
	if( weights[N_RIGHT] < 256 )
	{	color = get_nb_pix( pix_in, pixel, step, N_RIGHT );
		add_diff = color_diff( color, pixel, si->comp_n );
		interpolate_corner( si->comp_n, bounds[n], cur_diff, add_diff, bounds[N_RIGHT], si->ipr );
	}
	fill_subrow( si, row, ci, si->ipl, si->ipr, bounds[n] );
}

static void inc_rect( ps_rect* rect, ps_size size, char l, char r, char t, char b )
{	if( ( rect->x + rect->w < size.w ) && r )
	{	rect->w++;  }
	if( rect->y + rect->h < size.h && b )
	{	rect->h++;  }
	if( rect->x > 0 && l )
	{	rect->x--; rect->w++;  }
	if( rect->y > 0 && t )
	{	rect->y--; rect->h++;  }
}

static ps_rect get_color_rect( info* si, ps_rect rect, ps_size size )
{	ps_rect colRect = rect;
	inc_rect
	(	&colRect, size,
		si->cols_info[rect.x].weight_l < 256,
		si->cols_info[rect.x + rect.w - 1].weight_r < 256,
		si->rows_info[rect.y].weight_l < 256,
		si->rows_info[rect.y + rect.h - 1].weight_r < 256
	);
	return colRect;
}

static void pass_through
(	info* si,
	uchar* src,
	line_info* ci,
	uchar** mid_cur,
	cell_info *cell
)
{	int mc;
	int step;
	uchar comp_n;
	comp_n = si->comp_n;
	step = si->fmt_out.step;
	for( mc = 0; mc < ci->size; mc++ )
	{	memcpy( *mid_cur, src, comp_n );
		*mid_cur = *mid_cur + step;
	}
}

static void set_area( info* si, ps_rect* area )
{	area->x = round( si->scale_x * area->x );
	area->w = round( si->scale_x * area->w );
	area->y = round( si->scale_y * area->y );
	area->h = round( si->scale_y * area->h );
}

typedef struct rowstart
{	uchar*     out;
	uchar*     in;
	cell_info* cell;
} rowstart;

static void rs_init( info* si, ps_rect* area,
	ps_pixels pix_in, ps_pixels pix_out, rowstart* rs )
{	rs->cell = si->cells + area->y * si->size_in.w + area->x;
	rs->in   = addr_img( si->fmt_in.step, pix_in, area->x, area->y );
	rs->in   = rs->in + si->fmt_in.offs;
	rs->out  = addr_img( si->fmt_out.step, pix_out,
		si->cols_info[area->x].left, si->rows_info[area->y].left );
	rs->out  = rs->out + si->fmt_out.offs;
}

static void rs_next( rowstart* rs, info* si, ps_pixels pix_in, ps_pixels pix_out, line_info ri )
{	rs->out  = rs->out  + ri.size * pix_out.pitch;
	rs->cell = rs->cell + si->size_in.w;
	rs->in   = rs->in   + pix_in.pitch;
}

static void scale_init
(	info* si, ps_pixels* pix_inP, ps_pixels pix_out,
	ps_rect* area,
	unsigned* width_bytesP, rowstart* rsP
)
{	ps_rect col_rect;
	// WARN: for some reason the local buffer works faster even though
	//       I have to allocate and initialize it at every call:
	// *mid_rowP = (uchar*)malloc( pix_out.pitch );

	handle_dwh_scale( si->dw, si->dh, pix_inP, area );

	if( !si->nb )
	{	inc_rect( area, si->size_in, 1, 1, 1, 1 );
	}

	*width_bytesP = si->fmt_out.step *
		(	si->cols_info[area->x + area->w - 1].left +
			si->cols_info[area->x+area->w-1].size -
			si->cols_info[area->x].left
		) - si->fmt_out.offs;

	rs_init( si, area, *pix_inP, pix_out, rsP );
}

static void loopx_nb
(	info* si, ps_rect *area, uchar* src, rowstart* rs )
{	int x_in;
	line_info ci;
	uchar* mid_cur;
	cell_info* cell;

	cell = rs->cell;
	mid_cur = si->rowbuf;
	for( x_in = area->x; x_in < area->x + area->w; x_in++ )
	{	ci = si->cols_info[x_in];
		pass_through( si, src, &ci, &mid_cur, cell );
		cell++;
		src = src + si->fmt_in.step;
	}
}

static void loopx_np
(	info* si, ps_rect *area, uchar* src, rowstart* rs,
	line_info ri, ps_pixels pix_in, ps_pixels pix_out
)
{	int x_in;
	line_info ci;
	cell_info* cell;
	uchar *top_cur, *bot_cur;

	cell    = rs->cell;
	top_cur = rs->out;
	bot_cur = rs->out + (ri.size - 1) * pix_out.pitch;
	uchar* mid_cur = si->rowbuf;

	for( x_in = area->x; x_in < area->x + area->w; x_in++ )
	{	ci = si->cols_info[x_in];
		get_boundary_colors( cell, pix_in, src, si );
		if( ri.weight_l  < 256 )
		{	fill_boundary( &top_cur, si, &ci, pix_in, N_TOP,    cell, src );  }
		if( ri.weight_r  < 256 )
		{	fill_boundary( &bot_cur, si, &ci, pix_in, N_BOTTOM, cell, src );  }
		if( ri.main_width == 0 )
		{	goto next_pixel;  }
		fill_subrow( si, &mid_cur, &ci,
			si->bound_cols[N_LEFT ],
			si->bound_cols[N_RIGHT],
			src
		);
	next_pixel:
		cell++;
		src = src + si->fmt_in.step;
	}
}

void ps_scale( ps_info const osi, ps_pixels pix_in, ps_pixels const pix_out, ps_rect* area )
{	info* si = (info*)osi;
	int       y_in;
	uchar     mid_y;
	line_info ri;
	uchar*    mid_row;
	uchar     *src;
	char      nb;
	unsigned  width_bytes;
	uchar* mid_row_out;
	rowstart rs;

	nb     = si->nb;

	scale_init( si, &pix_in, pix_out, area, &width_bytes, &rs );

	for( y_in = area->y; y_in < area->y + area->h; y_in++ )
	{	src  = rs.in;
		ri   = si->rows_info[y_in];

		/* Loop separated into two for optimization: */
		if( nb )
		{	loopx_nb( si, area, src, &rs );  }
		else
		{	loopx_np( si, area, src, &rs, ri, pix_in, pix_out );  }

		/* copy the middle row, if present, to output: */
		if( ri.main_width > 0 )
		{	mid_row_out = rs.out +
			( ri.main_start - ri.left ) * pix_out.pitch;
			for( mid_y = 0; mid_y < ri.main_width; mid_y++ )
			{	memcpy(	mid_row_out, si->rowbuf, width_bytes );
				mid_row_out = mid_row_out + pix_out.pitch;
			}
		}
		rs_next( &rs, si, pix_in, pix_out, ri );
	}
	//free( mid_row );
	set_area( si, area );
}
