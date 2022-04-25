/* PDCurses */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <curspriv.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>

#include <sys/types.h>

#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>

#include <Xatom.h>

#define XCURSESDISPLAY (XtDisplay(pdc_drawing))
#define XCURSESWIN     (XtWindow(pdc_drawing))

typedef struct
{
    int lines;
    int cols;
    Pixel colorBlack;
    Pixel colorRed;
    Pixel colorGreen;
    Pixel colorYellow;
    Pixel colorBlue;
    Pixel colorMagenta;
    Pixel colorCyan;
    Pixel colorWhite;
    Pixel colorBoldBlack;
    Pixel colorBoldRed;
    Pixel colorBoldGreen;
    Pixel colorBoldYellow;
    Pixel colorBoldBlue;
    Pixel colorBoldMagenta;
    Pixel colorBoldCyan;
    Pixel colorBoldWhite;
    Pixel pointerForeColor;
    Pixel pointerBackColor;
    XFontStruct *normalFont;
    XFontStruct *italicFont;
    XFontStruct *boldFont;
    char *bitmap;
    char *pixmap;
    Cursor pointer;
    int clickPeriod;
    int doubleClickPeriod;
    int scrollbarWidth;
    int cursorBlinkRate;
    char *textCursor;
    int textBlinkRate;
} XCursesAppData;

extern Pixel pdc_color[PDC_MAXCOL];
extern XIC pdc_xic;

extern XCursesAppData pdc_app_data;
extern XtAppContext pdc_app_context;
extern Widget pdc_toplevel, pdc_drawing;

extern GC pdc_normal_gc, pdc_cursor_gc, pdc_italic_gc, pdc_bold_gc;
extern int pdc_fheight, pdc_fwidth, pdc_fascent, pdc_fdescent;
extern int pdc_wwidth, pdc_wheight;

extern bool pdc_blinked_off, pdc_window_entered, pdc_resize_now;
extern bool pdc_vertical_cursor, pdc_visible_cursor;

int PDC_display_cursor(int, int, int, int, int);

void PDC_blink_cursor(XtPointer, XtIntervalId *);
void PDC_blink_text(XtPointer, XtIntervalId *);
int PDC_kb_setup(void);
void PDC_redraw_cursor(void);
bool PDC_scrollbar_init(const char *);
