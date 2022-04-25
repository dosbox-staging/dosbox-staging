/* PDCurses */

#include "pdcx11.h"

#include <keysym.h>

#ifdef HAVE_DECKEYSYM_H
# include <DECkeysym.h>
#endif

#ifdef HAVE_SUNKEYSYM_H
# include <Sunkeysym.h>
#endif

static struct
{
    KeySym keycode;
    bool numkeypad;
    unsigned short normal;
    unsigned short shifted;
    unsigned short control;
    unsigned short alt;
} key_table[] =
{
/* keycode      keypad  normal       shifted       control      alt*/
 {XK_Left,      FALSE,  KEY_LEFT,    KEY_SLEFT,    CTL_LEFT,    ALT_LEFT},
 {XK_Right,     FALSE,  KEY_RIGHT,   KEY_SRIGHT,   CTL_RIGHT,   ALT_RIGHT},
 {XK_Up,        FALSE,  KEY_UP,      KEY_SUP,      CTL_UP,      ALT_UP},
 {XK_Down,      FALSE,  KEY_DOWN,    KEY_SDOWN,    CTL_DOWN,    ALT_DOWN},
 {XK_Home,      FALSE,  KEY_HOME,    KEY_SHOME,    CTL_HOME,    ALT_HOME},
/* Sun Type 4 keyboard */
 {XK_R7,        FALSE,  KEY_HOME,    KEY_SHOME,    CTL_HOME,    ALT_HOME},
 {XK_End,       FALSE,  KEY_END,     KEY_SEND,     CTL_END,     ALT_END},
/* Sun Type 4 keyboard */
 {XK_R13,       FALSE,  KEY_END,     KEY_SEND,     CTL_END,     ALT_END},
 {XK_Prior,     FALSE,  KEY_PPAGE,   KEY_SPREVIOUS,CTL_PGUP,    ALT_PGUP},
/* Sun Type 4 keyboard */
 {XK_R9,        FALSE,  KEY_PPAGE,   KEY_SPREVIOUS,CTL_PGUP,    ALT_PGUP},
 {XK_Next,      FALSE,  KEY_NPAGE,   KEY_SNEXT,    CTL_PGDN,    ALT_PGDN},
/* Sun Type 4 keyboard */
 {XK_R15,       FALSE,  KEY_NPAGE,   KEY_SNEXT,    CTL_PGDN,    ALT_PGDN},
 {XK_Insert,    FALSE,  KEY_IC,      KEY_SIC,      CTL_INS,     ALT_INS},
 {XK_Delete,    FALSE,  KEY_DC,      KEY_SDC,      CTL_DEL,     ALT_DEL},
 {XK_F1,        FALSE,  KEY_F(1),    KEY_F(13),    KEY_F(25),   KEY_F(37)},
 {XK_F2,        FALSE,  KEY_F(2),    KEY_F(14),    KEY_F(26),   KEY_F(38)},
 {XK_F3,        FALSE,  KEY_F(3),    KEY_F(15),    KEY_F(27),   KEY_F(39)},
 {XK_F4,        FALSE,  KEY_F(4),    KEY_F(16),    KEY_F(28),   KEY_F(40)},
 {XK_F5,        FALSE,  KEY_F(5),    KEY_F(17),    KEY_F(29),   KEY_F(41)},
 {XK_F6,        FALSE,  KEY_F(6),    KEY_F(18),    KEY_F(30),   KEY_F(42)},
 {XK_F7,        FALSE,  KEY_F(7),    KEY_F(19),    KEY_F(31),   KEY_F(43)},
 {XK_F8,        FALSE,  KEY_F(8),    KEY_F(20),    KEY_F(32),   KEY_F(44)},
 {XK_F9,        FALSE,  KEY_F(9),    KEY_F(21),    KEY_F(33),   KEY_F(45)},
 {XK_F10,       FALSE,  KEY_F(10),   KEY_F(22),    KEY_F(34),   KEY_F(46)},
 {XK_F11,       FALSE,  KEY_F(11),   KEY_F(23),    KEY_F(35),   KEY_F(47)},
 {XK_F12,       FALSE,  KEY_F(12),   KEY_F(24),    KEY_F(36),   KEY_F(48)},
 {XK_F13,       FALSE,  KEY_F(13),   KEY_F(25),    KEY_F(37),   KEY_F(49)},
 {XK_F14,       FALSE,  KEY_F(14),   KEY_F(26),    KEY_F(38),   KEY_F(50)},
 {XK_F15,       FALSE,  KEY_F(15),   KEY_F(27),    KEY_F(39),   KEY_F(51)},
 {XK_F16,       FALSE,  KEY_F(16),   KEY_F(28),    KEY_F(40),   KEY_F(52)},
 {XK_F17,       FALSE,  KEY_F(17),   KEY_F(29),    KEY_F(41),   KEY_F(53)},
 {XK_F18,       FALSE,  KEY_F(18),   KEY_F(30),    KEY_F(42),   KEY_F(54)},
 {XK_F19,       FALSE,  KEY_F(19),   KEY_F(31),    KEY_F(43),   KEY_F(55)},
 {XK_F20,       FALSE,  KEY_F(20),   KEY_F(32),    KEY_F(44),   KEY_F(56)},
 {XK_BackSpace, FALSE,  0x08,        0x08,         CTL_BKSP,    ALT_BKSP},
 {XK_Tab,       FALSE,  0x09,        KEY_BTAB,     CTL_TAB,     ALT_TAB},
#if defined(XK_ISO_Left_Tab)
 {XK_ISO_Left_Tab, FALSE, 0x09,      KEY_BTAB,     CTL_TAB,     ALT_TAB},
#endif
 {XK_Select,    FALSE,  KEY_SELECT,  KEY_SELECT,   KEY_SELECT,  KEY_SELECT},
 {XK_Print,     FALSE,  KEY_PRINT,   KEY_SPRINT,   KEY_PRINT,   KEY_PRINT},
 {XK_Find,      FALSE,  KEY_FIND,    KEY_SFIND,    KEY_FIND,    KEY_FIND},
 {XK_Pause,     FALSE,  KEY_SUSPEND, KEY_SSUSPEND, KEY_SUSPEND, KEY_SUSPEND},
 {XK_Clear,     FALSE,  KEY_CLEAR,   KEY_CLEAR,    KEY_CLEAR,   KEY_CLEAR},
 {XK_Cancel,    FALSE,  KEY_CANCEL,  KEY_SCANCEL,  KEY_CANCEL,  KEY_CANCEL},
 {XK_Break,     FALSE,  KEY_BREAK,   KEY_BREAK,    KEY_BREAK,   KEY_BREAK},
 {XK_Help,      FALSE,  KEY_HELP,    KEY_SHELP,    KEY_LHELP,   KEY_HELP},
 {XK_L4,        FALSE,  KEY_UNDO,    KEY_SUNDO,    KEY_UNDO,    KEY_UNDO},
 {XK_L6,        FALSE,  KEY_COPY,    KEY_SCOPY,    KEY_COPY,    KEY_COPY},
 {XK_L9,        FALSE,  KEY_FIND,    KEY_SFIND,    KEY_FIND,    KEY_FIND},
 {XK_Menu,      FALSE,  KEY_OPTIONS, KEY_SOPTIONS, KEY_OPTIONS, KEY_OPTIONS},
 {XK_Super_R,   FALSE,  KEY_COMMAND, KEY_SCOMMAND, KEY_COMMAND, KEY_COMMAND},
 {XK_Super_L,   FALSE,  KEY_COMMAND, KEY_SCOMMAND, KEY_COMMAND, KEY_COMMAND},
#ifdef HAVE_SUNKEYSYM_H
 {SunXK_F36,    FALSE,  KEY_F(41),   KEY_F(43),    KEY_F(45),   KEY_F(47)},
 {SunXK_F37,    FALSE,  KEY_F(42),   KEY_F(44),    KEY_F(46),   KEY_F(48)},
#endif
#ifdef HAVE_DECKEYSYM_H
 {DXK_Remove,   FALSE,  KEY_DC,      KEY_SDC,      CTL_DEL,     ALT_DEL},
#endif
 {XK_Escape,    FALSE,  0x1B,        0x1B,         0x1B,        ALT_ESC},
 {XK_KP_Enter,  TRUE,   PADENTER,    PADENTER,     CTL_PADENTER,ALT_PADENTER},
 {XK_KP_Add,    TRUE,   PADPLUS,     '+',          CTL_PADPLUS, ALT_PADPLUS},
 {XK_KP_Subtract,TRUE,  PADMINUS,    '-',          CTL_PADMINUS,ALT_PADMINUS},
 {XK_KP_Multiply,TRUE,  PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
/* Sun Type 4 keyboard */
 {XK_R6,        TRUE,   PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
 {XK_KP_Divide, TRUE,   PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
/* Sun Type 4 keyboard */
 {XK_R5,        TRUE,   PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
 {XK_KP_Decimal,TRUE,   PADSTOP,     '.',          CTL_PADSTOP, ALT_PADSTOP},
 {XK_KP_0,      TRUE,   PAD0,        '0',          CTL_PAD0,    ALT_PAD0},
 {XK_KP_1,      TRUE,   KEY_C1,      '1',          CTL_PAD1,    ALT_PAD1},
 {XK_KP_2,      TRUE,   KEY_C2,      '2',          CTL_PAD2,    ALT_PAD2},
 {XK_KP_3,      TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
 {XK_KP_4,      TRUE,   KEY_B1,      '4',          CTL_PAD4,    ALT_PAD4},
 {XK_KP_5,      TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
/* Sun Type 4 keyboard */
 {XK_R11,       TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
 {XK_KP_6,      TRUE,   KEY_B3,      '6',          CTL_PAD6,    ALT_PAD6},
 {XK_KP_7,      TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
 {XK_KP_8,      TRUE,   KEY_A2,      '8',          CTL_PAD8,    ALT_PAD8},
 {XK_KP_9,      TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
/* the following added to support Sun Type 5 keyboards */
 {XK_F21,       FALSE,  KEY_SUSPEND, KEY_SSUSPEND, KEY_SUSPEND, KEY_SUSPEND},
 {XK_F22,       FALSE,  KEY_PRINT,   KEY_SPRINT,   KEY_PRINT,   KEY_PRINT},
 {XK_F24,       TRUE,   PADMINUS,    '-',          CTL_PADMINUS,ALT_PADMINUS},
/* Sun Type 4 keyboard */
 {XK_F25,       TRUE,   PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
/* Sun Type 4 keyboard */
 {XK_F26,       TRUE,   PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
 {XK_F27,       TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
 {XK_F29,       TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
 {XK_F31,       TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
 {XK_F35,       TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
#ifdef XK_KP_Delete
 {XK_KP_Delete, TRUE,   PADSTOP,     '.',          CTL_PADSTOP, ALT_PADSTOP},
#endif
#ifdef XK_KP_Insert
 {XK_KP_Insert, TRUE,   PAD0,        '0',          CTL_PAD0,    ALT_PAD0},
#endif
#ifdef XK_KP_End
 {XK_KP_End,    TRUE,   KEY_C1,      '1',          CTL_PAD1,    ALT_PAD1},
#endif
#ifdef XK_KP_Down
 {XK_KP_Down,   TRUE,   KEY_C2,      '2',          CTL_PAD2,    ALT_PAD2},
#endif
#ifdef XK_KP_Next
 {XK_KP_Next,   TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
#endif
#ifdef XK_KP_Left
 {XK_KP_Left,   TRUE,   KEY_B1,      '4',          CTL_PAD4,    ALT_PAD4},
#endif
#ifdef XK_KP_Begin
 {XK_KP_Begin,  TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
#endif
#ifdef XK_KP_Right
 {XK_KP_Right,  TRUE,   KEY_B3,      '6',          CTL_PAD6,    ALT_PAD6},
#endif
#ifdef XK_KP_Home
 {XK_KP_Home,   TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
#endif
#ifdef XK_KP_Up
 {XK_KP_Up,     TRUE,   KEY_A2,      '8',          CTL_PAD8,    ALT_PAD8},
#endif
#ifdef XK_KP_Prior
 {XK_KP_Prior,  TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
#endif
 {0,            0,      0,           0,            0,           0}
};

static KeySym keysym = 0;
static XIM Xim = NULL;

XIC pdc_xic = NULL;

#ifdef MOUSE_DEBUG
# define MOUSE_LOG(x) printf x
#else
# define MOUSE_LOG(x)
#endif

static unsigned long _process_key_event(XEvent *event)
{
    Status status;
    wchar_t buffer[120];
    unsigned long key = 0;
    int buflen = 40;
    int i, count;
    unsigned long modifier = 0;
    bool key_code = FALSE;

    PDC_LOG(("_process_key_event() - called\n"));

    /* In compose -- ignore elements */

    if (XFilterEvent(event, XCURSESWIN))
        return -1;

    /* Handle modifier keys first; ignore other KeyReleases */

    if (event->type == KeyRelease)
    {
        /* The keysym value was set by a previous call to this function
           with a KeyPress event (or reset by the mouse event handler) */

        if (SP->return_key_modifiers &&
            IsModifierKey(keysym))
        {
            switch (keysym) {
            case XK_Shift_L:
                key = KEY_SHIFT_L;
                break;
            case XK_Shift_R:
                key = KEY_SHIFT_R;
                break;
            case XK_Control_L:
                key = KEY_CONTROL_L;
                break;
            case XK_Control_R:
                key = KEY_CONTROL_R;
                break;
            case XK_Alt_L:
                key = KEY_ALT_L;
                break;
            case XK_Alt_R:
                key = KEY_ALT_R;
            }

            if (key)
            {
                SP->key_code = TRUE;
                return key;
            }
        }

        return -1;
    }

    buffer[0] = '\0';

    count = XwcLookupString(pdc_xic, &(event->xkey), buffer, buflen,
                            &keysym, &status);

    /* translate keysym into curses key code */

    PDC_LOG(("Key mask: %x\n", event->xkey.state));

    /* 0x10: usually, numlock modifier */

    if (event->xkey.state & Mod2Mask)
        modifier |= PDC_KEY_MODIFIER_NUMLOCK;

    /* 0x01: shift modifier */

    if (event->xkey.state & ShiftMask)
        modifier |= PDC_KEY_MODIFIER_SHIFT;

    /* 0x04: control modifier */

    if (event->xkey.state & ControlMask)
        modifier |= PDC_KEY_MODIFIER_CONTROL;

    /* 0x08: usually, alt modifier */

    if (event->xkey.state & Mod1Mask)
        modifier |= PDC_KEY_MODIFIER_ALT;

    for (i = 0; key_table[i].keycode; i++)
    {
        if (key_table[i].keycode == keysym)
        {
            PDC_LOG(("State %x\n", event->xkey.state));

            /* ControlMask: 0x04: control modifier
               Mod1Mask: 0x08: usually, alt modifier
               Mod2Mask: 0x10: usually, numlock modifier
               ShiftMask: 0x01: shift modifier */

            if ((event->xkey.state & ShiftMask) ||
                (key_table[i].numkeypad &&
                (event->xkey.state & Mod2Mask)))
            {
                key = key_table[i].shifted;
            }
            else if (event->xkey.state & ControlMask)
            {
                key = key_table[i].control;
            }
            else if (event->xkey.state & Mod1Mask)
            {
                key = key_table[i].alt;
            }

            /* To get here, we ignore all other modifiers */

            else
                key = key_table[i].normal;

            key_code = (key > 0x100);
            break;
        }
    }

    if (!key && buffer[0] && count == 1)
        key = buffer[0];

    PDC_LOG(("Key: %s pressed - %x Mod: %x\n",
             XKeysymToString(keysym), key, event->xkey.state));

    /* Handle ALT letters and numbers */

    if (event->xkey.state & Mod1Mask)
    {
        if (key >= 'A' && key <= 'Z')
        {
            key += ALT_A - 'A';
            key_code = TRUE;
        }

        if (key >= 'a' && key <= 'z')
        {
            key += ALT_A - 'a';
            key_code = TRUE;
        }

        if (key >= '0' && key <= '9')
        {
            key += ALT_0 - '0';
            key_code = TRUE;
        }
    }

    /* After all that, send the key back to the application if is
       NOT zero. */

    if (key)
    {
        SP->key_modifiers = modifier;

        SP->key_code = key_code;
        return key;
    }

    return -1;
}

static unsigned long _process_mouse_event(XEvent *event)
{
    int button_no;
    static int last_button_no = 0;

    PDC_LOG(("_process_mouse_event() - called\n"));

    keysym = 0; /* suppress any modifier key return */

    button_no = event->xbutton.button;

    /* It appears that under X11R6 (at least on Linux), that an
       event_type of ButtonMotion does not include the mouse button in
       the event. The following code is designed to cater for this
       situation. */

    SP->mouse_status.changes = 0;

    SP->mouse_status.x = event->xbutton.x / pdc_fwidth;
    SP->mouse_status.y = event->xbutton.y / pdc_fheight;

    switch(event->type)
    {
    case ButtonPress:
        /* Handle button 4 and 5, which are normally mapped to the wheel
           mouse scroll up and down, and button 6 and 7, which are
           normally mapped to the wheel mouse scroll left and right */

        last_button_no = button_no;

        if (button_no >= 4 && button_no <= 7)
        {
            /* Send the KEY_MOUSE to curses program */

            memset(&SP->mouse_status, 0, sizeof(SP->mouse_status));

            switch(button_no)
            {
               case 4:
                  SP->mouse_status.changes = PDC_MOUSE_WHEEL_UP;
                  break;
               case 5:
                  SP->mouse_status.changes = PDC_MOUSE_WHEEL_DOWN;
                  break;
               case 6:
                  SP->mouse_status.changes = PDC_MOUSE_WHEEL_LEFT;
                  break;
               case 7:
                  SP->mouse_status.changes = PDC_MOUSE_WHEEL_RIGHT;
            }

            SP->mouse_status.x = SP->mouse_status.y = -1;

            SP->key_code = TRUE;
            return KEY_MOUSE;
        }

        MOUSE_LOG(("\nButtonPress\n"));

        SP->mouse_status.button[button_no - 1] = BUTTON_PRESSED;

        napms(SP->mouse_wait);
        while (XtAppPending(pdc_app_context))
        {
            XEvent rel;
            XtAppNextEvent(pdc_app_context, &rel);

            if (rel.type == ButtonRelease && rel.xbutton.button == button_no)
                SP->mouse_status.button[button_no - 1] = BUTTON_CLICKED;
            else
                XSendEvent(XtDisplay(pdc_toplevel),
                           RootWindowOfScreen(XtScreen(pdc_toplevel)),
                           True, 0, &rel);
        }

        break;

    case MotionNotify:
        MOUSE_LOG(("\nMotionNotify: y: %d x: %d Width: %d "
                   "Height: %d\n", event->xbutton.y, event->xbutton.x,
                   pdc_fwidth, pdc_fheight));

        button_no = last_button_no;

        SP->mouse_status.changes |= PDC_MOUSE_MOVED;
        break;

    case ButtonRelease:
        MOUSE_LOG(("\nButtonRelease\n"));

        /* ignore "releases" of scroll buttons */

        if (button_no >= 4 && button_no <= 7)
            return -1;

        SP->mouse_status.button[button_no - 1] = BUTTON_RELEASED;
    }

    /* Set up the mouse status fields in preparation for sending */

    SP->mouse_status.changes |= 1 << (button_no - 1);

    if (SP->mouse_status.changes & PDC_MOUSE_MOVED &&
        (SP->mouse_status.button[button_no - 1] &
         BUTTON_ACTION_MASK) == BUTTON_PRESSED)
        SP->mouse_status.button[button_no - 1] = BUTTON_MOVED;

    if (event->xbutton.state & ShiftMask)
        SP->mouse_status.button[button_no - 1] |= BUTTON_SHIFT;
    if (event->xbutton.state & ControlMask)
        SP->mouse_status.button[button_no - 1] |= BUTTON_CONTROL;
    if (event->xbutton.state & Mod1Mask)
        SP->mouse_status.button[button_no - 1] |= BUTTON_ALT;

    /* If we are ignoring the event, or the mouse position is outside
       the bounds of the screen, return here */

    if (SP->mouse_status.x < 0 || SP->mouse_status.x >= SP->cols ||
        SP->mouse_status.y < 0 || SP->mouse_status.y >= SP->lines)
        return -1;

    /* Send the KEY_MOUSE to curses program */

    SP->key_code = TRUE;
    return KEY_MOUSE;
}

/* check if a key or mouse event is waiting */

bool PDC_check_key(void)
{
    XtInputMask s = XtAppPending(pdc_app_context);

    PDC_LOG(("PDC_check_key() - returning %s\n", s ? "TRUE" : "FALSE"));

    return pdc_resize_now || !!s;
}

/* return the next available key or mouse event */

int PDC_get_key(void)
{
    XEvent event;
    unsigned long newkey = 0;
    int key = 0;

    if (pdc_resize_now)
    {
        pdc_resize_now = FALSE;
        SP->key_code = TRUE;
        return KEY_RESIZE;
    }

    XtAppNextEvent(pdc_app_context, &event);

    switch (event.type)
    {
    case KeyPress:
    case KeyRelease:
        newkey = _process_key_event(&event);
        break;
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
        newkey = _process_mouse_event(&event);
        break;
    default:
        XtDispatchEvent(&event);
        return -1;
    }

    if ((unsigned long)(-1) == newkey)
        return -1;

    key = (int)newkey;

    PDC_LOG(("PDC_get_key() - key %d returned\n", key));

    return key;
}

void PDC_set_keyboard_binary(bool on)
{
    PDC_LOG(("PDC_set_keyboard_binary() - called\n"));
}

/* discard any pending keyboard or mouse input -- this is the core
   routine for flushinp() */

void PDC_flushinp(void)
{
    PDC_LOG(("PDC_flushinp() - called\n"));

    while (PDC_check_key())
        PDC_get_key();
}

bool PDC_has_mouse(void)
{
    return TRUE;
}

int PDC_mouse_set(void)
{
    return OK;
}

int PDC_modifiers_set(void)
{
    return OK;
}

static void _dummy_handler(Widget w, XtPointer client_data,
                           XEvent *event, Boolean *unused)
{
}

int PDC_kb_setup(void)
{
    Xim = XOpenIM(XCURSESDISPLAY, NULL, NULL, NULL);

    if (Xim)
    {
        pdc_xic = XCreateIC(Xim, XNInputStyle,
                            XIMPreeditNothing | XIMStatusNothing,
                            XNClientWindow, XCURSESWIN, NULL);
    }

    if (pdc_xic)
    {
        long im_event_mask;

        XGetICValues(pdc_xic, XNFilterEvents, &im_event_mask, NULL);

        /* Add in the mouse events */

        im_event_mask |= ButtonPressMask | ButtonReleaseMask |
                         ButtonMotionMask;

        XtAddEventHandler(pdc_drawing, im_event_mask, False,
                          _dummy_handler, NULL);
        XSetICFocus(pdc_xic);
    }
    else
    {
        perror("ERROR: Cannot create input context");
        return ERR;
    }

    return OK;
}
