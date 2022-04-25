/* Copyright 2018-2020          Anton Shepelev          (anton.txt@gmail.com) */
/* Usage of the works is permitted  provided that this instrument is retained */
/* with the works, so that any entity that uses the works is notified of this */
/* instrument.    DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY.                 */

/* ----------------------- Pixel-perfect scaling unit ----------------------- */
/* This unit uses the Horstmann indentation style.                            */

#ifndef PPSCALE_H
#define PPSCALE_H

#ifdef __cplusplus
extern "C" {
#endif

int pp_getscale /* calculate integer scales for pixel-perfect magnification */
(	int    win,  int hin,  /* input dimensions                  */
	double par,            /* input pixel aspect ratio          */
	int    wout, int hout, /* output  dimensions                */
	double parweight,      /* weight of PAR in scale estimation */
	int    *sx,  int *sy   /* horisontal and vertical scales    */
); /* returns -1 on error and 0 on success */


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
); /* return -1 on error and 0 on success */

#ifdef __cplusplus
}
#endif

class PPScale {
public:
	PPScale(const int source_w,
	        const int source_h,
	        const double aspect_ratio,
	        const bool source_is_doubled,
	        const int canvas_w,
	        const int canvas_h)

	{
		assert(source_w > 0);
		assert(source_h > 0);
		assert(canvas_w > 0);
		assert(canvas_h > 0);
		assert(aspect_ratio > 0.1);

		// divide the source if it's been doubled
		effective_source_w = source_w / (source_is_doubled ? 2 : 1);
		effective_source_h = source_h / (source_is_doubled ? 2 : 1);

		// call the pixel-perfect library to get the scale factors
		constexpr double aspect_weight = 1.14; // pixel-perfect constant
		const int err = pp_getscale(effective_source_w, effective_source_h,
		                            aspect_ratio, canvas_w, canvas_h,
		                            aspect_weight, &scale_x, &scale_y);

		// if the scaling failed, ensure the the multipliers are valid
		if (err) {
			scale_x = 1;
			scale_y = 1;
		}
		// calculate the output dimensions
		output_w = effective_source_w * scale_x;
		output_h = effective_source_h * scale_y;
		assert(output_w > 0);
		assert(output_h > 0);
	}
	// default constructor
	PPScale() {}

	int effective_source_w = 0;
	int effective_source_h = 0;
	int output_w = 0;
	int output_h = 0;
private:
	int scale_x = 0;
	int scale_y = 0;
};

#endif /* PPSCALE_H */
