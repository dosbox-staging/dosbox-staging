/* PDCurses */

#include "pdcx11.h"

#include <stdlib.h>

/*man-start**************************************************************

clipboard
---------

### Synopsis

    int PDC_getclipboard(char **contents, long *length);
    int PDC_setclipboard(const char *contents, long length);
    int PDC_freeclipboard(char *contents);
    int PDC_clearclipboard(void);

### Description

   PDC_getclipboard() gets the textual contents of the system's
   clipboard. This function returns the contents of the clipboard in the
   contents argument. It is the responsibility of the caller to free the
   memory returned, via PDC_freeclipboard(). The length of the clipboard
   contents is returned in the length argument.

   PDC_setclipboard copies the supplied text into the system's
   clipboard, emptying the clipboard prior to the copy.

   PDC_clearclipboard() clears the internal clipboard.

### Return Values

   indicator of success/failure of call.
   PDC_CLIP_SUCCESS        the call was successful
   PDC_CLIP_MEMORY_ERROR   unable to allocate sufficient memory for
                           the clipboard contents
   PDC_CLIP_EMPTY          the clipboard contains no text
   PDC_CLIP_ACCESS_ERROR   no clipboard support

### Portability
                             X/Open  ncurses  NetBSD
    PDC_getclipboard            -       -       -
    PDC_setclipboard            -       -       -
    PDC_freeclipboard           -       -       -
    PDC_clearclipboard          -       -       -

**man-end****************************************************************/

#include "Xmu/StdSel.h"
#include "Xmu/Atoms.h"

static char *tmpsel = NULL;
static unsigned long tmpsel_length = 0;

static char *xc_selection = NULL;
static long xc_selection_len = 0;

#ifndef X_HAVE_UTF8_STRING
static Atom XA_UTF8_STRING(Display *dpy)
{
    static AtomPtr p = NULL;

    if (!p)
        p = XmuMakeAtom("UTF8_STRING");

    return XmuInternAtom(dpy, p);
}
#endif

static Boolean _convert_proc(Widget w, Atom *selection, Atom *target,
                             Atom *type_return, XtPointer *value_return,
                             unsigned long *length_return, int *format_return)
{
    PDC_LOG(("_convert_proc() - called\n"));

    if (*target == XA_TARGETS(XtDisplay(pdc_toplevel)))
    {
        XSelectionRequestEvent *req = XtGetSelectionRequest(w,
            *selection, (XtRequestId)NULL);

        Atom *targetP;
        XPointer std_targets;
        unsigned long std_length;

        XmuConvertStandardSelection(pdc_toplevel, req->time, selection,
                                    target, type_return, &std_targets,
                                    &std_length, format_return);

        *length_return = std_length + 2;
        *value_return = XtMalloc(sizeof(Atom) * (*length_return));

        targetP = *(Atom**)value_return;
        *targetP++ = XA_STRING;
        *targetP++ = XA_UTF8_STRING(XtDisplay(pdc_toplevel));

        memmove((void *)targetP, (const void *)std_targets,
                sizeof(Atom) * std_length);

        XtFree((char *)std_targets);
        *type_return = XA_ATOM;
        *format_return = 8;

        return True;
    }
    else if (*target == XA_UTF8_STRING(XtDisplay(pdc_toplevel)) ||
             *target == XA_STRING)
    {
        char *data = XtMalloc(tmpsel_length + 1);
        char *tmp = tmpsel;
        int ret_length = 0;

        while (*tmp)
            data[ret_length++] = *tmp++;

        data[ret_length] = '\0';

        *value_return = data;
        *length_return = ret_length;
        *format_return = 8;
        *type_return = *target;

        return True;
    }
    else
        return XmuConvertStandardSelection(pdc_toplevel, CurrentTime,
            selection, target, type_return, (XPointer*)value_return,
            length_return, format_return);
}

static void _lose_ownership(Widget w, Atom *type)
{
    PDC_LOG(("_lose_ownership() - called\n"));

    if (tmpsel)
        free(tmpsel);

    tmpsel = NULL;
    tmpsel_length = 0;
}

static void _get_selection(Widget w, XtPointer data, Atom *selection,
                           Atom *type, XtPointer value,
                           unsigned long *length, int *format)
{
    PDC_LOG(("_get_selection() - called\n"));

    if (value)
    {
        xc_selection = value;
        xc_selection_len = (long)(*length);
    }
    else
        xc_selection_len = 0;
}

int PDC_getclipboard(char **contents, long *length)
{
    XEvent event;

    PDC_LOG(("PDC_getclipboard() - called\n"));

    xc_selection = NULL;
    xc_selection_len = -1;

    XtGetSelectionValue(pdc_toplevel, XA_PRIMARY,
#ifdef PDC_WIDE
                        XA_UTF8_STRING(XtDisplay(pdc_toplevel)),
#else
                        XA_STRING,
#endif
                        _get_selection, (XtPointer)NULL, 0);

    while (-1 == xc_selection_len)
    {
        XtAppNextEvent(pdc_app_context, &event);
        XtDispatchEvent(&event);
    }

    if (xc_selection && xc_selection_len)
    {
        *contents = malloc(xc_selection_len + 1);

        if (!*contents)
            return PDC_CLIP_MEMORY_ERROR;

        memcpy(*contents, xc_selection, xc_selection_len);

        (*contents)[xc_selection_len] = '\0';
        *length = xc_selection_len;

        return PDC_CLIP_SUCCESS;
    }

    return PDC_CLIP_EMPTY;
}

int PDC_setclipboard(const char *contents, long length)
{
    long pos;
    int status;

    PDC_LOG(("PDC_setclipboard() - called\n"));

    if (length > (long)tmpsel_length)
    {
        if (!tmpsel_length)
            tmpsel = malloc(length + 1);
        else
            tmpsel = realloc(tmpsel, length + 1);
    }

    for (pos = 0; pos < length; pos++)
        tmpsel[pos] = contents[pos];

    tmpsel_length = length;
    tmpsel[length] = 0;

    if (XtOwnSelection(pdc_toplevel, XA_PRIMARY, CurrentTime,
                       _convert_proc, _lose_ownership, NULL) == False)
    {
        status = PDC_CLIP_ACCESS_ERROR;
        free(tmpsel);
        tmpsel = NULL;
        tmpsel_length = 0;
    }
    else
        status = PDC_CLIP_SUCCESS;

    return status;
}

int PDC_freeclipboard(char *contents)
{
    PDC_LOG(("PDC_freeclipboard() - called\n"));

    free(contents);
    return PDC_CLIP_SUCCESS;
}

int PDC_clearclipboard(void)
{
    PDC_LOG(("PDC_clearclipboard() - called\n"));

    return PDC_CLIP_SUCCESS;
}
