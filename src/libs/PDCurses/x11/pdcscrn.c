/* PDCurses */

#include "pdcx11.h"

#include <xpm.h>

#include <stdlib.h>
#include <string.h>

/* Default icons for XCurses applications.  */

#include "../common/icon64.xpm"
#include "../common/icon32.xpm"

#ifdef PDC_WIDE
# define DEFNFONT "-misc-fixed-medium-r-normal--20-200-75-75-c-100-iso10646-1"
# define DEFIFONT "-misc-fixed-medium-o-normal--20-200-75-75-c-100-iso10646-1"
# define DEFBFONT "-misc-fixed-bold-r-normal--20-200-75-75-c-100-iso10646-1"
#else
# define DEFNFONT "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1"
# define DEFIFONT "-misc-fixed-medium-o-normal--13-120-75-75-c-70-iso8859-1"
# define DEFBFONT "-misc-fixed-bold-r-normal--13-120-75-75-c-70-iso8859-1"
#endif

#ifndef MAX_PATH
# define MAX_PATH 256
#endif

/* Macros just for app_resources */

#define APPDATAOFF(n) XtOffsetOf(XCursesAppData, n)

#define RINT(name1, name2, value) { \
                #name1, #name2, XtRInt, \
                sizeof(int), APPDATAOFF(name1), XtRImmediate, \
                (XtPointer)value \
        }

#define RPIXEL(name1, name2, value) { \
                #name1, #name2, XtRPixel, \
                sizeof(Pixel), APPDATAOFF(name1), XtRString, \
                (XtPointer)#value \
        }

#define RCOLOR(name, value) RPIXEL(color##name, Color##name, value)


#define RSTRINGP(name1, name2, param) { \
                #name1, #name2, XtRString, \
                MAX_PATH, APPDATAOFF(name1), XtRString, (XtPointer)param \
        }

#define RSTRING(name1, name2) RSTRINGP(name1, name2, "")

#define RFONT(name1, name2, value) { \
                #name1, #name2, XtRFontStruct, \
                sizeof(XFontStruct), APPDATAOFF(name1), XtRString, \
                (XtPointer)value \
        }

#define RCURSOR(name1, name2, value) { \
                #name1, #name2, XtRCursor, \
                sizeof(Cursor), APPDATAOFF(name1), XtRString, \
                (XtPointer)#value \
        }

static XtResource app_resources[] =
{
    RINT(lines, Lines, -1),
    RINT(cols, Cols, -1),

    RCOLOR(Black, Black),
    RCOLOR(Red, red3),
    RCOLOR(Green, green3),
    RCOLOR(Yellow, yellow3),
    RCOLOR(Blue, blue3),
    RCOLOR(Magenta, magenta3),
    RCOLOR(Cyan, cyan3),
    RCOLOR(White, Grey),

    RCOLOR(BoldBlack, grey40),
    RCOLOR(BoldRed, red1),
    RCOLOR(BoldGreen, green1),
    RCOLOR(BoldYellow, yellow1),
    RCOLOR(BoldBlue, blue1),
    RCOLOR(BoldMagenta, magenta1),
    RCOLOR(BoldCyan, cyan1),
    RCOLOR(BoldWhite, White),

    RFONT(normalFont, NormalFont, DEFNFONT),
    RFONT(italicFont, ItalicFont, DEFIFONT),
    RFONT(boldFont, BoldFont, DEFBFONT),

    RSTRING(bitmap, Bitmap),
    RSTRING(pixmap, Pixmap),
    RCURSOR(pointer, Pointer, xterm),

    RPIXEL(pointerForeColor, PointerForeColor, Black),
    RPIXEL(pointerBackColor, PointerBackColor, White),

    RINT(doubleClickPeriod, DoubleClickPeriod, (PDC_CLICK_PERIOD * 2)),
    RINT(clickPeriod, ClickPeriod, PDC_CLICK_PERIOD),
    RINT(scrollbarWidth, ScrollbarWidth, 15),
    RINT(cursorBlinkRate, CursorBlinkRate, 0),

    RSTRING(textCursor, TextCursor),
    RINT(textBlinkRate, TextBlinkRate, 500)
};

#undef RCURSOR
#undef RFONT
#undef RSTRING
#undef RCOLOR
#undef RPIXEL
#undef RINT
#undef APPDATAOFF
#undef DEFBFONT
#undef DEFIFONT
#undef DEFNFONT

/* Macros for options */

#define COPT(name) {"-" #name, "*" #name, XrmoptionSepArg, NULL}
#define CCOLOR(name) COPT(color##name)

static XrmOptionDescRec options[] =
{
    COPT(lines), COPT(cols), COPT(normalFont), COPT(italicFont),
    COPT(boldFont), COPT(bitmap), COPT(pixmap), COPT(pointer),
    COPT(clickPeriod), COPT(doubleClickPeriod), COPT(scrollbarWidth),
    COPT(pointerForeColor), COPT(pointerBackColor),
    COPT(cursorBlinkRate), COPT(textCursor), COPT(textBlinkRate),

    CCOLOR(Black), CCOLOR(Red), CCOLOR(Green), CCOLOR(Yellow),
    CCOLOR(Blue), CCOLOR(Magenta), CCOLOR(Cyan), CCOLOR(White),

    CCOLOR(BoldBlack), CCOLOR(BoldRed), CCOLOR(BoldGreen),
    CCOLOR(BoldYellow), CCOLOR(BoldBlue), CCOLOR(BoldMagenta),
    CCOLOR(BoldCyan), CCOLOR(BoldWhite)
};

#undef CCOLOR
#undef COPT

Pixel pdc_color[PDC_MAXCOL];

XCursesAppData pdc_app_data;
XtAppContext pdc_app_context;
Widget pdc_toplevel, pdc_drawing;

GC pdc_normal_gc, pdc_cursor_gc, pdc_italic_gc, pdc_bold_gc;
int pdc_fheight, pdc_fwidth, pdc_fascent, pdc_fdescent;
int pdc_wwidth, pdc_wheight;
bool pdc_window_entered = TRUE, pdc_resize_now = FALSE;

static Atom wm_atom[2];
static String class_name = "XCurses";
static int resize_window_width = 0, resize_window_height = 0;
static int received_map_notify = 0;
static bool exposed = FALSE;

static Pixmap icon_pixmap, icon_pixmap_mask;

static char *prog_name[] = {"PDCurses", NULL};
static char **argv = prog_name;
static int argc = 1;

/* close the physical screen */

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
}

void PDC_scr_free(void)
{
    if (icon_pixmap)
        XFreePixmap(XCURSESDISPLAY, icon_pixmap);
    if (icon_pixmap_mask)
        XFreePixmap(XCURSESDISPLAY, icon_pixmap_mask);

    XFreeGC(XCURSESDISPLAY, pdc_normal_gc);
    XFreeGC(XCURSESDISPLAY, pdc_italic_gc);
    XFreeGC(XCURSESDISPLAY, pdc_bold_gc);
    XFreeGC(XCURSESDISPLAY, pdc_cursor_gc);
    XDestroyIC(pdc_xic);
}

void XCursesExit(void)
{
    PDC_scr_free();
}

static void _initialize_colors(void)
{
    int i, r, g, b;

    pdc_color[COLOR_BLACK]   = pdc_app_data.colorBlack;
    pdc_color[COLOR_RED]     = pdc_app_data.colorRed;
    pdc_color[COLOR_GREEN]   = pdc_app_data.colorGreen;
    pdc_color[COLOR_YELLOW]  = pdc_app_data.colorYellow;
    pdc_color[COLOR_BLUE]    = pdc_app_data.colorBlue;
    pdc_color[COLOR_MAGENTA] = pdc_app_data.colorMagenta;
    pdc_color[COLOR_CYAN]    = pdc_app_data.colorCyan;
    pdc_color[COLOR_WHITE]   = pdc_app_data.colorWhite;

    pdc_color[COLOR_BLACK + 8]   = pdc_app_data.colorBoldBlack;
    pdc_color[COLOR_RED + 8]     = pdc_app_data.colorBoldRed;
    pdc_color[COLOR_GREEN + 8]   = pdc_app_data.colorBoldGreen;
    pdc_color[COLOR_YELLOW + 8]  = pdc_app_data.colorBoldYellow;
    pdc_color[COLOR_BLUE + 8]    = pdc_app_data.colorBoldBlue;
    pdc_color[COLOR_MAGENTA + 8] = pdc_app_data.colorBoldMagenta;
    pdc_color[COLOR_CYAN + 8]    = pdc_app_data.colorBoldCyan;
    pdc_color[COLOR_WHITE + 8]   = pdc_app_data.colorBoldWhite;

#define RGB(R, G, B) ( ((unsigned long)(R) << 16) | \
                       ((unsigned long)(G) << 8) | \
                       ((unsigned long)(B)) )

    /* 256-color xterm extended palette: 216 colors in a 6x6x6 color
       cube, plus 24 shades of gray */

    for (i = 16, r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++)
                pdc_color[i++] = RGB(r ? r * 40 + 55 : 0,
                                     g ? g * 40 + 55 : 0,
                                     b ? b * 40 + 55 : 0);
    for (i = 0; i < 24; i++)
        pdc_color[i + 232] = RGB(i * 10 + 8, i * 10 + 8, i * 10 + 8);

#undef RGB
}

static void _get_icon(void)
{
    Status rc;

    PDC_LOG(("_get_icon() - called\n"));

    if (pdc_app_data.pixmap && pdc_app_data.pixmap[0]) /* supplied pixmap */
    {
        XpmReadFileToPixmap(XtDisplay(pdc_toplevel),
                            RootWindowOfScreen(XtScreen(pdc_toplevel)),
                            (char *)pdc_app_data.pixmap,
                            &icon_pixmap, &icon_pixmap_mask, NULL);
    }
    else if (pdc_app_data.bitmap && pdc_app_data.bitmap[0]) /* bitmap */
    {
        unsigned file_bitmap_width = 0, file_bitmap_height = 0;
        int x_hot = 0, y_hot = 0;

        rc = XReadBitmapFile(XtDisplay(pdc_toplevel),
                             RootWindowOfScreen(XtScreen(pdc_toplevel)),
                             (char *)pdc_app_data.bitmap,
                             &file_bitmap_width, &file_bitmap_height,
                             &icon_pixmap, &x_hot, &y_hot);

        if (BitmapOpenFailed == rc)
            fprintf(stderr, "bitmap file %s: not found\n",
                    pdc_app_data.bitmap);
        else if (BitmapFileInvalid == rc)
            fprintf(stderr, "bitmap file %s: contents invalid\n",
                    pdc_app_data.bitmap);
    }
    else
    {
        XIconSize *icon_size;
        int size_count = 0, max_height = 0, max_width = 0;

        icon_size = XAllocIconSize();

        rc = XGetIconSizes(XtDisplay(pdc_toplevel),
                           RootWindowOfScreen(XtScreen(pdc_toplevel)),
                           &icon_size, &size_count);

        /* if the WM can advise on icon sizes... */

        if (rc && size_count)
        {
            int i;

            PDC_LOG(("size_count: %d rc: %d\n", size_count, rc));

            for (i = 0; i < size_count; i++)
            {
                if (icon_size[i].max_width > max_width)
                    max_width = icon_size[i].max_width;
                if (icon_size[i].max_height > max_height)
                    max_height = icon_size[i].max_height;

                PDC_LOG(("min: %d %d\n",
                         icon_size[i].min_width, icon_size[i].min_height));

                PDC_LOG(("max: %d %d\n",
                         icon_size[i].max_width, icon_size[i].max_height));

                PDC_LOG(("inc: %d %d\n",
                         icon_size[i].width_inc, icon_size[i].height_inc));
            }
        }

        XFree(icon_size);

        XpmCreatePixmapFromData(XtDisplay(pdc_toplevel),
              RootWindowOfScreen(XtScreen(pdc_toplevel)),
              (max_width >= 64 && max_height >= 64) ? icon64 : icon32,
              &icon_pixmap, &icon_pixmap_mask, NULL);
    }
}

/* Redraw the entire screen */

static void _display_screen(void)
{
    int row;

    PDC_LOG(("_display_screen() - called\n"));

    if (!curscr)
        return;

    for (row = 0; row < SP->lines; row++)
        PDC_transform_line(row, 0, COLS, curscr->_y[row]);

    PDC_redraw_cursor();
}

static void _handle_expose(Widget w, XtPointer client_data, XEvent *event,
                           Boolean *unused)
{
    PDC_LOG(("_handle_expose() - called\n"));

    /* ignore all Exposes except last */

    if (event->xexpose.count)
        return;

    exposed = TRUE;

    if (received_map_notify)
        _display_screen();
}

static void _handle_nonmaskable(Widget w, XtPointer client_data, XEvent *event,
                                Boolean *unused)
{
    XClientMessageEvent *client_event = (XClientMessageEvent *)event;

    PDC_LOG(("_handle_nonmaskable called: event %d\n", event->type));

    if (event->type == ClientMessage)
    {
        PDC_LOG(("ClientMessage received\n"));

        /* This code used to include handling of WM_SAVE_YOURSELF, but
           it resulted in continual failure of THE on my Toshiba laptop.
           Removed on 3-3-2001. Now only exits on WM_DELETE_WINDOW. */

        if ((Atom)client_event->data.s[0] == wm_atom[0])
            exit(0);
    }
}

static void _handle_enter_leave(Widget w, XtPointer client_data,
                                XEvent *event, Boolean *unused)
{
    PDC_LOG(("_handle_enter_leave called\n"));

    switch(event->type)
    {
    case EnterNotify:
        PDC_LOG(("EnterNotify received\n"));

        pdc_window_entered = TRUE;
        break;

    case LeaveNotify:
        PDC_LOG(("LeaveNotify received\n"));

        pdc_window_entered = FALSE;

        /* Display the cursor so it stays on while the window is
           not current */

        PDC_redraw_cursor();
        break;

    default:
        PDC_LOG(("_handle_enter_leave - unknown event %d\n", event->type));
    }
}

static void _handle_structure_notify(Widget w, XtPointer client_data,
                                     XEvent *event, Boolean *unused)
{
    PDC_LOG(("_handle_structure_notify() - called\n"));

    switch(event->type)
    {
    case ConfigureNotify:
        PDC_LOG(("ConfigureNotify received\n"));

        /* Window has been resized, change width and height to send to
           place_text and place_graphics in next Expose. */

        resize_window_width = event->xconfigure.width;
        resize_window_height = event->xconfigure.height;

        SP->resized = TRUE;
        pdc_resize_now = TRUE;
        break;

    case MapNotify:
        PDC_LOG(("MapNotify received\n"));

        received_map_notify = 1;

        break;

    default:
        PDC_LOG(("_handle_structure_notify - unknown event %d\n",
                 event->type));
    }
}

static void _get_gc(GC *gc, XFontStruct *font_info, int fore, int back)
{
    XGCValues values;

    /* Create default Graphics Context */

    *gc = XCreateGC(XCURSESDISPLAY, XCURSESWIN, 0L, &values);

    /* specify font */

    XSetFont(XCURSESDISPLAY, *gc, font_info->fid);

    XSetForeground(XCURSESDISPLAY, *gc, pdc_color[fore]);
    XSetBackground(XCURSESDISPLAY, *gc, pdc_color[back]);
}

static void _pointer_setup(void)
{
    XColor pointerforecolor, pointerbackcolor;
    XrmValue rmfrom, rmto;

    XDefineCursor(XCURSESDISPLAY, XCURSESWIN, pdc_app_data.pointer);
    rmfrom.size = sizeof(Pixel);
    rmto.size = sizeof(XColor);

    rmto.addr = (XPointer)&pointerforecolor;
    rmfrom.addr = (XPointer)&(pdc_app_data.pointerForeColor);
    XtConvertAndStore(pdc_drawing, XtRPixel, &rmfrom, XtRColor, &rmto);

    rmfrom.size = sizeof(Pixel);
    rmto.size = sizeof(XColor);

    rmfrom.addr = (XPointer)&(pdc_app_data.pointerBackColor);
    rmto.addr = (XPointer)&pointerbackcolor;
    XtConvertAndStore(pdc_drawing, XtRPixel, &rmfrom, XtRColor, &rmto);

    XRecolorCursor(XCURSESDISPLAY, pdc_app_data.pointer,
                   &pointerforecolor, &pointerbackcolor);
}

void PDC_set_args(int c, char **v)
{
    argc = c;
    argv = v;
}

/* open the physical screen -- miscellaneous initialization */

int PDC_scr_open(void)
{
    bool italic_font_valid, bold_font_valid;
    int minwidth, minheight;

    PDC_LOG(("PDC_scr_open() - called\n"));

    /* Start defining X Toolkit things */

#if XtSpecificationRelease > 4
    XtSetLanguageProc(NULL, (XtLanguageProc)NULL, NULL);
#endif

    /* Exit if no DISPLAY variable set */

    if (!getenv("DISPLAY"))
    {
        fprintf(stderr, "Error: no DISPLAY variable set\n");
        return ERR;
    }

    /* Initialise the top level widget */

    pdc_toplevel = XtVaAppInitialize(&pdc_app_context, class_name, options,
                                 XtNumber(options), &argc, argv, NULL, NULL);

    XtVaGetApplicationResources(pdc_toplevel, &pdc_app_data, app_resources,
                                XtNumber(app_resources), NULL);

    /* Check application resource values here */

    pdc_fwidth = pdc_app_data.normalFont->max_bounds.width;

    pdc_fascent = pdc_app_data.normalFont->ascent;
    pdc_fdescent = pdc_app_data.normalFont->descent;
    pdc_fheight = pdc_fascent + pdc_fdescent;

    /* Check that the italic font and normal fonts are the same size */

    italic_font_valid =
        pdc_fwidth == pdc_app_data.italicFont->max_bounds.width;

    bold_font_valid =
        pdc_fwidth == pdc_app_data.boldFont->max_bounds.width;

    /* Calculate size of display window */

    COLS = pdc_app_data.cols;
    LINES = pdc_app_data.lines;

    if (-1 == COLS)
    {
        const char *env = getenv("PDC_COLS");
        if (env)
            COLS = atoi(env);

        if (COLS <= 0)
            COLS = 80;
    }

    if (-1 == LINES)
    {
        const char *env = getenv("PDC_LINES");
        if (env)
            LINES = atoi(env);

        if (LINES <= 0)
            LINES = 24;
    }

    pdc_wwidth = pdc_fwidth * COLS;
    pdc_wheight = pdc_fheight * LINES;

    minwidth = pdc_fwidth * 2;
    minheight = pdc_fheight * 2;

    /* Set up the icon for the application; the default is an internal
       one for PDCurses. Then set various application level resources. */

    _get_icon();

    XtVaSetValues(pdc_toplevel, XtNminWidth, minwidth, XtNminHeight,
                  minheight, XtNbaseWidth, 0, XtNbaseHeight, 0,
                  XtNbackground, 0, XtNiconPixmap, icon_pixmap,
                  XtNiconMask, icon_pixmap_mask, NULL);

    /* Create a widget in which to draw */

    if (!PDC_scrollbar_init(argv[0]))
    {
        pdc_drawing = pdc_toplevel;

        XtVaSetValues(pdc_toplevel, XtNwidth, pdc_wwidth, XtNheight,
            pdc_wheight, XtNwidthInc, pdc_fwidth, XtNheightInc,
            pdc_fheight, NULL);
    }

    /* Determine text cursor alignment from resources */

    if (!strcmp(pdc_app_data.textCursor, "vertical"))
        pdc_vertical_cursor = TRUE;

    SP->lines = LINES;
    SP->cols = COLS;

    SP->mouse_wait = pdc_app_data.clickPeriod;
    SP->audible = TRUE;

    SP->termattrs = A_COLOR | A_ITALIC | A_UNDERLINE | A_LEFT | A_RIGHT |
                    A_REVERSE;

    /* Add Event handlers to the drawing widget */

    XtAddEventHandler(pdc_drawing, ExposureMask, False, _handle_expose, NULL);
    XtAddEventHandler(pdc_drawing, StructureNotifyMask, False,
                      _handle_structure_notify, NULL);
    XtAddEventHandler(pdc_drawing, EnterWindowMask | LeaveWindowMask, False,
                      _handle_enter_leave, NULL);
    XtAddEventHandler(pdc_toplevel, 0, True, _handle_nonmaskable, NULL);

    /* If there is a cursorBlink resource, start the Timeout event */

    if (pdc_app_data.cursorBlinkRate)
        XtAppAddTimeOut(pdc_app_context, pdc_app_data.cursorBlinkRate,
                        PDC_blink_cursor, NULL);

    XtRealizeWidget(pdc_toplevel);

    /* Handle trapping of the WM_DELETE_WINDOW property */

    wm_atom[0] = XInternAtom(XtDisplay(pdc_toplevel), "WM_DELETE_WINDOW",
                             False);

    XSetWMProtocols(XtDisplay(pdc_toplevel), XtWindow(pdc_toplevel),
                    wm_atom, 1);

    /* Create the Graphics Context for drawing. This MUST be done AFTER
       the associated widget has been realized. */

    PDC_LOG(("before _get_gc\n"));

    _get_gc(&pdc_normal_gc, pdc_app_data.normalFont, COLOR_WHITE, COLOR_BLACK);

    _get_gc(&pdc_italic_gc, italic_font_valid ? pdc_app_data.italicFont :
            pdc_app_data.normalFont, COLOR_WHITE, COLOR_BLACK);

    _get_gc(&pdc_bold_gc, bold_font_valid ? pdc_app_data.boldFont :
            pdc_app_data.normalFont, COLOR_WHITE, COLOR_BLACK);

    _get_gc(&pdc_cursor_gc, pdc_app_data.normalFont,
            COLOR_WHITE, COLOR_BLACK);

    XSetLineAttributes(XCURSESDISPLAY, pdc_cursor_gc, 2,
                       LineSolid, CapButt, JoinMiter);

    /* Set the pointer for the application */

    _pointer_setup();

    if (ERR == PDC_kb_setup())
        return ERR;

    while (!exposed)
    {
        XEvent event;

        XtAppNextEvent(pdc_app_context, &event);
        XtDispatchEvent(&event);
    }

    _initialize_colors();

    SP->orig_attr = FALSE;

    atexit(PDC_scr_free);

    XSync(XtDisplay(pdc_toplevel), True);
    SP->resized = pdc_resize_now = FALSE;

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
    PDC_LOG(("PDC_resize_screen() - called. Lines: %d Cols: %d\n",
             nlines, ncols));

    if (nlines || ncols || !SP->resized)
        return ERR;

    pdc_wwidth = resize_window_width;
    pdc_wheight = resize_window_height;
    pdc_visible_cursor = TRUE;

    return OK;
}

void PDC_reset_prog_mode(void)
{
    PDC_LOG(("PDC_reset_prog_mode() - called.\n"));
}

void PDC_reset_shell_mode(void)
{
    PDC_LOG(("PDC_reset_shell_mode() - called.\n"));
}

void PDC_restore_screen_mode(int i)
{
}

void PDC_save_screen_mode(int i)
{
}

bool PDC_can_change_color(void)
{
    return TRUE;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    XColor tmp;
    Colormap cmap = DefaultColormap(XCURSESDISPLAY,
                                    DefaultScreen(XCURSESDISPLAY));

    tmp.pixel = pdc_color[color];
    XQueryColor(XCURSESDISPLAY, cmap, &tmp);

    *red = ((double)(tmp.red) * 1000 / 65535) + 0.5;
    *green = ((double)(tmp.green) * 1000 / 65535) + 0.5;
    *blue = ((double)(tmp.blue) * 1000 / 65535) + 0.5;

    return OK;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    XColor tmp;

    tmp.red = ((double)red * 65535 / 1000) + 0.5;
    tmp.green = ((double)green * 65535 / 1000) + 0.5;
    tmp.blue = ((double)blue * 65535 / 1000) + 0.5;

    Colormap cmap = DefaultColormap(XCURSESDISPLAY,
                                    DefaultScreen(XCURSESDISPLAY));

    if (XAllocColor(XCURSESDISPLAY, cmap, &tmp))
        pdc_color[color] = tmp.pixel;

    return OK;
}
