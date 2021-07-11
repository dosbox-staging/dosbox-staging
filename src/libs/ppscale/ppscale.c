/* Copyright 2018-2020          Anton Shepelev          (anton.txt@gmail.com) */
/* Usage of the works is permitted  provided that this instrument is retained */
/* with the works, so that any entity that uses the works is notified of this */
/* instrument.    DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY.                 */

/* ----------------------- Pixel-perfect scaling unit ----------------------- */
/* This unit uses the Horstmann indentation style.                            */

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>

static double min( double a, double b )
{	double res;
	if( a < b ) res = a;
	else        res = b;
	return res;	
}

int pp_getscale /* calculate integer scales for pixel-perfect magnification */
(	int    win,  int hin,  /* input dimensions                  */
	double par,            /* input pixel aspect ratio          */
	int    wout, int hout, /* output  dimensions                */
	double parweight,      /* weight of PAR in scale estimation */
	int    *sx,  int *sy   /* horisontal and vertical scales    */
) /* returns -1 on error and 0 on success */
{
	/* check for invalid inputs: */
	if( win <= 0    || hin <= 0    ||
	    win >  wout || hin >  hout ||
	    par <= 0.0  || parweight <= 0 ||
	    sx == NULL  || sy == NULL )
		return -1;

	/* enforce aspect ratio priority for 1:n and 1:n pixel proportions: */
	const double parnorm = ( par > 1.0 ) ? par : 1.0 / par; /* Ensure PAR is normalized to exceed 1.0  */

	/* if our PAR is an integer ratio, then this will enforce exact aspect ratio */
	const int exactpar = parnorm - floor( parnorm ) < 0.01;
	
	/* maximum x and y scales */
	const int sxm = (int)floor( ( double )wout / win );
	const int sym = (int)floor( ( double )hout / hin );
	assert(sxm && sym);

	/* current x and y scales; incrementally adjusted */
	int sxc = sxm;
	int syc = sym;

	double parrat = 0;               /* ratio of current PAR and target PAR    */
	double errpar = 0, errsize = 0, err = 0; /* PAR error, size error, and total error */
	double errmin = 0;               /* minimal error so far                   */ 
	double srat = 0;                 /* ratio of maximum size to current       */

	errmin = -1; /* this value marks the first iteration */
	while( 1 )
	{
		/* Handle unstable calculation: parrat = (double)syc / sxc / par  */
		if( syc == 0 ) /* numerator is zero, so result will be zero */
			parrat = 0;
		else if( sxc == 0 ) /* numerator is not zero but denominator is zero, so will result in 'inf' */
			parrat = (double)INFINITY;
		else /* otherwise attempt the calculation */
			parrat = (double)syc / sxc / par;

		/* calculate aspect-ratio error: */
		if( parrat > 1.0 )
			errpar = parrat;
		else if( fabs(parrat) > DBL_EPSILON ) /* denominator is valid, so allow the division */
			errpar = 1.0 / parrat;
		else /* otherwise parrat is near zero so will result in 'inf' */
			errpar = (double)INFINITY;

		/* Handle unstable calculation: srat = min( (double)sym/syc, (double)sxm/sxc ) */
		if( syc == 0 && sxc == 0 ) /* denominators are both zero so will result in 'inf' */
			srat = (double)INFINITY; /* numerator always positive, so negative 'inf' is not possible */
		else if( syc == 0 ) /* left-hand-size 'inf', so use right-hand-side */
			srat = (double)sxm / sxc;
		else if( sxc == 0 ) /* right-hand-size 'inf', so use left-hand-side */
			srat = (double)sym / syc;
		else /* if none of the above, attempt the calculation */
			srat = min( (double)sym/syc, (double)sxm/sxc );

		/* calculate size error: */
		/* if PAR is exact, exclude size error from the fitness function: */
		if( exactpar ) errsize = 1.0;
		else           errsize = pow( srat, parweight );

		err = errpar * errsize; /* total error */

		/* check for a new optimum or if errmin is -1: */
		if( err < errmin || fabs(errmin + 1) < DBL_EPSILON )
		{
			*sx    = sxc;
			*sy    = syc;
			errmin = err;
		}

		/* try a smaller magnification: */
		if( parrat < 1.0 ) sxc--;
		else               syc--;		

		/* do not explore magnifications smaller than half the screen: */
		if( srat >= 2.0 ) break;
	}
	
	return 0;
}

/* RAT: the many scalar arguments are not unifined into one of more stucts */
/*      because doing so somehow thwarts GCC's O3 optimisations, and the   */
/*      alrorithm works up to 30% slower.                                  */
int pp_scale /* magnify an image in a pixel-perfect manner */
(	char* simg, int spitch, /* source image and pitch           */
	int   *rx,  int *ry,    /* location and                     */
	int   *rw,  int *rh,    /* size of the rectangle to process */
	char* dimg, int dpitch, /* destination image and pitch      */
	int   bypp,             /* bytes per pixel                  */
	int   sx,   int sy      /* horisontal and vertical scales   */
) /* return -1 on error and 0 on success */
{	int    x,  y,  /* coordinate of a source pixel             */
	      ix, iy,  /* horisontal and vertical subpixel indices */
	      b,       /* index of byte in a source pixel          */
	      drowsz;  /* destination row size in bytes            */

	char *srow, *spix,  *spos, /* source row, current pixel, and position */
	     *drow, *drow0, *dpos; /* dest. row, base row, and position */

	if /* minimal sanity checks: */
	(	bypp < 1 || sx < 1 || sy < 1 || *rw < 1 || *rh < 1 ||
		simg == NULL || dimg == NULL ||
		spitch < *rw * bypp || dpitch < *rw * sx * bypp
	)
	return -1;

	drowsz = bypp * *rw * sx;
	srow   = simg +      *ry * spitch +      *rx * bypp;
	drow   = dimg + sy * *ry * dpitch + sx * *rx * bypp;
	for( y = 0; y < *rh; y += 1 )
	{	drow0 = drow;
		dpos  = drow;
		spos  = srow;
		spix  = srow;

		for( x = 0; x < *rw; x += 1 )
		{	for( ix = 0; ix < sx; ix += 1 )
			{	for( b = 0; b < bypp; b += 1 )
				{	*dpos = *spos;
					dpos += 1;
					spos += 1;
				}
				spos = spix;
			}
			spix += bypp;
		}

		iy = 1;
		while( 1 )
		{	drow = drow + dpitch;          /* next destination row           */
			if( iy == sy ) break;          /* terminate if source row scaled */
			memcpy( drow, drow0, (size_t)drowsz ); /* duplicate base row below       */
			iy += 1;
		}

		srow = srow + spitch; /* next source row */
	}

	/* return the output rectagle: */
	*rx *= sx; * ry *= sy; *rw *= sx; *rh *= sy;

	return 0;
}
