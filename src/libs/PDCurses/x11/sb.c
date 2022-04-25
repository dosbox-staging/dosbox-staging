/* PDCurses */

#include "pdcx11.h"

/*man-start**************************************************************

sb
--

### Synopsis

    int sb_init(void)
    int sb_set_horz(int total, int viewport, int cur)
    int sb_set_vert(int total, int viewport, int cur)
    int sb_get_horz(int *total, int *viewport, int *cur)
    int sb_get_vert(int *total, int *viewport, int *cur)
    int sb_refresh(void);

### Description

   These functions manipulate the scrollbar.

### Return Value

   All functions return OK on success and ERR on error.

### Portability
                             X/Open  ncurses  NetBSD
    sb_init                     -       -       -
    sb_set_horz                 -       -       -
    sb_set_vert                 -       -       -
    sb_get_horz                 -       -       -
    sb_get_vert                 -       -       -
    sb_refresh                  -       -       -

**man-end****************************************************************/

#ifdef USE_XAW3D
# include <Xaw3d/Box.h>
# include <Xaw3d/Scrollbar.h>
#elif defined(USE_NEXTAW)
# include <neXtaw/Box.h>
# include <neXtaw/Scrollbar.h>
#else
# include <Xaw/Box.h>
# include <Xaw/Scrollbar.h>
#endif

#include "scrlbox.h"

bool sb_started = FALSE;

#if NeedWidePrototypes
# define PDC_SCROLLBAR_TYPE double
#else
# define PDC_SCROLLBAR_TYPE float
#endif

static Widget scrollBox, scrollVert, scrollHoriz;
static int sb_viewport_y, sb_viewport_x;
static int sb_total_y, sb_total_x, sb_cur_y, sb_cur_x;

static void _scroll_up_down(Widget w, XtPointer client_data,
    XtPointer call_data)
{
    int pixels = (long) call_data;
    int total_y = sb_total_y * pdc_fheight;
    int viewport_y = sb_viewport_y * pdc_fheight;
    int cur_y = sb_cur_y * pdc_fheight;

    /* When pixels is negative, right button pressed, move data down,
       thumb moves up.  Otherwise, left button pressed, pixels positive,
       move data up, thumb down. */

    cur_y += pixels;

    /* limit panning to size of overall */

    if (cur_y < 0)
        cur_y = 0;
    else
        if (cur_y > (total_y - viewport_y))
            cur_y = total_y - viewport_y;

    sb_cur_y = cur_y / pdc_fheight;

    XawScrollbarSetThumb(w, (double)((double)cur_y / (double)total_y),
                         (double)((double)viewport_y / (double)total_y));
}

static void _scroll_left_right(Widget w, XtPointer client_data,
    XtPointer call_data)
{
    int pixels = (long) call_data;
    int total_x = sb_total_x * pdc_fwidth;
    int viewport_x = sb_viewport_x * pdc_fwidth;
    int cur_x = sb_cur_x * pdc_fwidth;

    cur_x += pixels;

    /* limit panning to size of overall */

    if (cur_x < 0)
        cur_x = 0;
    else
        if (cur_x > (total_x - viewport_x))
            cur_x = total_x - viewport_x;

    sb_cur_x = cur_x / pdc_fwidth;

    XawScrollbarSetThumb(w, (double)((double)cur_x / (double)total_x),
                         (double)((double)viewport_x / (double)total_x));
}

static void _thumb_up_down(Widget w, XtPointer client_data,
    XtPointer call_data)
{
    double percent = *(double *) call_data;
    double total_y = (double) sb_total_y;
    double viewport_y = (double) sb_viewport_y;
    int cur_y = sb_cur_y;

    /* If the size of the viewport is > overall area simply return,
       as no scrolling is permitted. */

    if (sb_viewport_y >= sb_total_y)
        return;

    if ((sb_cur_y = (int)((double)total_y * percent)) >=
        (total_y - viewport_y))
        sb_cur_y = total_y - viewport_y;

    XawScrollbarSetThumb(w, (double)(cur_y / total_y),
                         (double)(viewport_y / total_y));
}

static void _thumb_left_right(Widget w, XtPointer client_data,
    XtPointer call_data)
{
    double percent = *(double *) call_data;
    double total_x = (double) sb_total_x;
    double viewport_x = (double) sb_viewport_x;
    int cur_x = sb_cur_x;

    if (sb_viewport_x >= sb_total_x)
        return;

    if ((sb_cur_x = (int)((float)total_x * percent)) >=
        (total_x - viewport_x))
        sb_cur_x = total_x - viewport_x;

    XawScrollbarSetThumb(w, (double)(cur_x / total_x),
                         (double)(viewport_x / total_x));
}

bool PDC_scrollbar_init(const char *program_name)
{
    if (pdc_app_data.scrollbarWidth && sb_started)
    {
        scrollBox = XtVaCreateManagedWidget(program_name,
            scrollBoxWidgetClass, pdc_toplevel, XtNwidth,
            pdc_wwidth + pdc_app_data.scrollbarWidth,
            XtNheight, pdc_wheight + pdc_app_data.scrollbarWidth,
            XtNwidthInc, pdc_fwidth, XtNheightInc, pdc_fheight, NULL);

        pdc_drawing = XtVaCreateManagedWidget(program_name,
            boxWidgetClass, scrollBox, XtNwidth,
            pdc_wwidth, XtNheight, pdc_wheight, XtNwidthInc,
            pdc_fwidth, XtNheightInc, pdc_fheight, NULL);

        scrollVert = XtVaCreateManagedWidget("scrollVert",
            scrollbarWidgetClass, scrollBox, XtNorientation,
            XtorientVertical, XtNheight, pdc_wheight, XtNwidth,
            pdc_app_data.scrollbarWidth, NULL);

        XtAddCallback(scrollVert, XtNscrollProc, _scroll_up_down, pdc_drawing);
        XtAddCallback(scrollVert, XtNjumpProc, _thumb_up_down, pdc_drawing);

        scrollHoriz = XtVaCreateManagedWidget("scrollHoriz",
            scrollbarWidgetClass, scrollBox, XtNorientation,
            XtorientHorizontal, XtNwidth, pdc_wwidth, XtNheight,
            pdc_app_data.scrollbarWidth, NULL);

        XtAddCallback(scrollHoriz, XtNscrollProc, _scroll_left_right,
                      pdc_drawing);
        XtAddCallback(scrollHoriz, XtNjumpProc, _thumb_left_right,
                      pdc_drawing);

        return TRUE;
    }

    return FALSE;
}

/* sb_init() is the sb initialization routine.
   This must be called before initscr(). */

int sb_init(void)
{
    PDC_LOG(("sb_init() - called\n"));

    if (SP)
        return ERR;

    sb_started = TRUE;
    sb_viewport_y = sb_viewport_x = 0;
    sb_total_y = sb_total_x = sb_cur_y = sb_cur_x = 0;

    return OK;
}

/* sb_set_horz() - Used to set horizontal scrollbar.

   total = total number of columns
   viewport = size of viewport in columns
   cur = current column in total */

int sb_set_horz(int total, int viewport, int cur)
{
    PDC_LOG(("sb_set_horz() - called: total %d viewport %d cur %d\n",
             total, viewport, cur));

    if (!SP)
        return ERR;

    sb_total_x = total;
    sb_viewport_x = viewport;
    sb_cur_x = cur;

    return OK;
}

/* sb_set_vert() - Used to set vertical scrollbar.

   total = total number of columns on line
   viewport = size of viewport in columns
   cur = current column in total */

int sb_set_vert(int total, int viewport, int cur)
{
    PDC_LOG(("sb_set_vert() - called: total %d viewport %d cur %d\n",
             total, viewport, cur));

    if (!SP)
        return ERR;

    sb_total_y = total;
    sb_viewport_y = viewport;
    sb_cur_y = cur;

    return OK;
}

/* sb_get_horz() - Used to get horizontal scrollbar.

   total = total number of lines
   viewport = size of viewport in lines
   cur = current line in total */

int sb_get_horz(int *total, int *viewport, int *cur)
{
    PDC_LOG(("sb_get_horz() - called\n"));

    if (!SP)
        return ERR;

    if (total)
        *total = sb_total_x;
    if (viewport)
        *viewport = sb_viewport_x;
    if (cur)
        *cur = sb_cur_x;

    return OK;
}

/* sb_get_vert() - Used to get vertical scrollbar.

   total = total number of lines
   viewport = size of viewport in lines
   cur = current line in total */

int sb_get_vert(int *total, int *viewport, int *cur)
{
    PDC_LOG(("sb_get_vert() - called\n"));

    if (!SP)
        return ERR;

    if (total)
        *total = sb_total_y;
    if (viewport)
        *viewport = sb_viewport_y;
    if (cur)
        *cur = sb_cur_y;

    return OK;
}

/* sb_refresh() - Used to draw the scrollbars. */

int sb_refresh(void)
{
    PDC_LOG(("sb_refresh() - called\n"));

    if (!SP)
        return ERR;

    if (sb_started)
    {
        PDC_SCROLLBAR_TYPE total_y = sb_total_y;
        PDC_SCROLLBAR_TYPE total_x = sb_total_x;

        if (total_y)
            XawScrollbarSetThumb(scrollVert,
                (PDC_SCROLLBAR_TYPE)(sb_cur_y) / total_y,
                (PDC_SCROLLBAR_TYPE)(sb_viewport_y) / total_y);

        if (total_x)
            XawScrollbarSetThumb(scrollHoriz,
                (PDC_SCROLLBAR_TYPE)(sb_cur_x) / total_x,
                (PDC_SCROLLBAR_TYPE)(sb_viewport_x) / total_x);
    }

    return OK;
}
