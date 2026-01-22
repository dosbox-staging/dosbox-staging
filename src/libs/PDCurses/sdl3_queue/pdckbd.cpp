/* PDCurses */

#include "pdcsdl.h"

#include <cctype>
#include <cstring>

static SDL_Event event;
static SDL_Keycode oldkey;
static MOUSE_STATUS old_mouse_status;

static struct
{
    SDL_Keycode keycode;
    bool numkeypad;
    unsigned short normal;
    unsigned short shifted;
    unsigned short control;
    unsigned short alt;
} key_table[] =
{
/* keycode      keypad  normal       shifted       control      alt*/
 {SDLK_LEFT,    FALSE,  KEY_LEFT,    KEY_SLEFT,    CTL_LEFT,    ALT_LEFT},
 {SDLK_RIGHT,   FALSE,  KEY_RIGHT,   KEY_SRIGHT,   CTL_RIGHT,   ALT_RIGHT},
 {SDLK_UP,      FALSE,  KEY_UP,      KEY_SUP,      CTL_UP,      ALT_UP},
 {SDLK_DOWN,    FALSE,  KEY_DOWN,    KEY_SDOWN,    CTL_DOWN,    ALT_DOWN},
 {SDLK_HOME,    FALSE,  KEY_HOME,    KEY_SHOME,    CTL_HOME,    ALT_HOME},
 {SDLK_END,     FALSE,  KEY_END,     KEY_SEND,     CTL_END,     ALT_END},
 {SDLK_PAGEUP,  FALSE,  KEY_PPAGE,   KEY_SPREVIOUS,CTL_PGUP,    ALT_PGUP},
 {SDLK_PAGEDOWN,FALSE,  KEY_NPAGE,   KEY_SNEXT,    CTL_PGDN,    ALT_PGDN},
 {SDLK_INSERT,  FALSE,  KEY_IC,      KEY_SIC,      CTL_INS,     ALT_INS},
 {SDLK_DELETE,  FALSE,  KEY_DC,      KEY_SDC,      CTL_DEL,     ALT_DEL},
 {SDLK_F1,      FALSE,  KEY_F(1),    KEY_F(13),    KEY_F(25),   KEY_F(37)},
 {SDLK_F2,      FALSE,  KEY_F(2),    KEY_F(14),    KEY_F(26),   KEY_F(38)},
 {SDLK_F3,      FALSE,  KEY_F(3),    KEY_F(15),    KEY_F(27),   KEY_F(39)},
 {SDLK_F4,      FALSE,  KEY_F(4),    KEY_F(16),    KEY_F(28),   KEY_F(40)},
 {SDLK_F5,      FALSE,  KEY_F(5),    KEY_F(17),    KEY_F(29),   KEY_F(41)},
 {SDLK_F6,      FALSE,  KEY_F(6),    KEY_F(18),    KEY_F(30),   KEY_F(42)},
 {SDLK_F7,      FALSE,  KEY_F(7),    KEY_F(19),    KEY_F(31),   KEY_F(43)},
 {SDLK_F8,      FALSE,  KEY_F(8),    KEY_F(20),    KEY_F(32),   KEY_F(44)},
 {SDLK_F9,      FALSE,  KEY_F(9),    KEY_F(21),    KEY_F(33),   KEY_F(45)},
 {SDLK_F10,     FALSE,  KEY_F(10),   KEY_F(22),    KEY_F(34),   KEY_F(46)},
 {SDLK_F11,     FALSE,  KEY_F(11),   KEY_F(23),    KEY_F(35),   KEY_F(47)},
 {SDLK_F12,     FALSE,  KEY_F(12),   KEY_F(24),    KEY_F(36),   KEY_F(48)},
 {SDLK_F13,     FALSE,  KEY_F(13),   KEY_F(25),    KEY_F(37),   KEY_F(49)},
 {SDLK_F14,     FALSE,  KEY_F(14),   KEY_F(26),    KEY_F(38),   KEY_F(50)},
 {SDLK_F15,     FALSE,  KEY_F(15),   KEY_F(27),    KEY_F(39),   KEY_F(51)},
 {SDLK_BACKSPACE,FALSE, 0x08,        0x08,         CTL_BKSP,    ALT_BKSP},
 {SDLK_TAB,     FALSE,  0x09,        KEY_BTAB,     CTL_TAB,     ALT_TAB},
 {SDLK_PRINTSCREEN,FALSE,KEY_PRINT,  KEY_SPRINT,   KEY_PRINT,   KEY_PRINT},
 {SDLK_PAUSE,   FALSE,  KEY_SUSPEND, KEY_SSUSPEND, KEY_SUSPEND, KEY_SUSPEND},
 {SDLK_CLEAR,   FALSE,  KEY_CLEAR,   KEY_CLEAR,    KEY_CLEAR,   KEY_CLEAR},
 {SDLK_HELP,    FALSE,  KEY_HELP,    KEY_SHELP,    KEY_LHELP,   KEY_HELP},
 {SDLK_MENU,    FALSE,  KEY_OPTIONS, KEY_SOPTIONS, KEY_OPTIONS, KEY_OPTIONS},
 {SDLK_ESCAPE,  FALSE,  0x1B,        0x1B,         0x1B,        ALT_ESC},
 {SDLK_KP_ENTER,TRUE,   PADENTER,    PADENTER,     CTL_PADENTER,ALT_PADENTER},
 {SDLK_KP_PLUS, TRUE,   PADPLUS,     '+',          CTL_PADPLUS, ALT_PADPLUS},
 {SDLK_KP_MINUS,TRUE,   PADMINUS,    '-',          CTL_PADMINUS,ALT_PADMINUS},
 {SDLK_KP_MULTIPLY,TRUE,PADSTAR,     '*',          CTL_PADSTAR, ALT_PADSTAR},
 {SDLK_KP_DIVIDE,TRUE,  PADSLASH,    '/',          CTL_PADSLASH,ALT_PADSLASH},
 {SDLK_KP_PERIOD,TRUE,  PADSTOP,     '.',          CTL_PADSTOP, ALT_PADSTOP},
 {SDLK_KP_0,    TRUE,   PAD0,        '0',          CTL_PAD0,    ALT_PAD0},
 {SDLK_KP_1,    TRUE,   KEY_C1,      '1',          CTL_PAD1,    ALT_PAD1},
 {SDLK_KP_2,    TRUE,   KEY_C2,      '2',          CTL_PAD2,    ALT_PAD2},
 {SDLK_KP_3,    TRUE,   KEY_C3,      '3',          CTL_PAD3,    ALT_PAD3},
 {SDLK_KP_4,    TRUE,   KEY_B1,      '4',          CTL_PAD4,    ALT_PAD4},
 {SDLK_KP_5,    TRUE,   KEY_B2,      '5',          CTL_PAD5,    ALT_PAD5},
 {SDLK_KP_6,    TRUE,   KEY_B3,      '6',          CTL_PAD6,    ALT_PAD6},
 {SDLK_KP_7,    TRUE,   KEY_A1,      '7',          CTL_PAD7,    ALT_PAD7},
 {SDLK_KP_8,    TRUE,   KEY_A2,      '8',          CTL_PAD8,    ALT_PAD8},
 {SDLK_KP_9,    TRUE,   KEY_A3,      '9',          CTL_PAD9,    ALT_PAD9},
 {0,            0,      0,           0,            0,           0}
};

void PDC_set_keyboard_binary([[maybe_unused]] bool on)
{
    PDC_LOG(("PDC_set_keyboard_binary() - called\n"));
}

/* check if a key or mouse event is waiting */

bool PDC_check_key(void)
{
    bool haveevent = false;

    PDC_pump_and_peep();

    /* SDL_EVENT_TEXT_INPUT can return multiple chars from the IME which we
       should handle before polling for additional events. */

    if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text[0]) {
        haveevent = true;
    } else {
        haveevent = !pdc_event_queue.empty();
        if (haveevent) {
            event = pdc_event_queue.front();
            pdc_event_queue.pop();
        }
    }

    return haveevent;
}

#ifdef PDC_WIDE
static int _utf8_to_unicode(char *chstr, size_t *b)
{
    int i, bytes, unicode;
    unsigned char byte = chstr[0];

    if (byte > 0xf0)
    {
        bytes = 4;
        unicode = byte & 0x7;
    }
    else if (byte > 0xe0)
    {
        bytes = 3;
        unicode = byte & 0xf;
    }
    else if (byte > 0xc0)
    {
        bytes = 2;
        unicode = byte & 0x1f;
    }
    else if (byte > 0x80) {
        /* starts with a continuation byte; invalid character */
        return -1;
    }
    else
    {
        bytes = 1;
        unicode = byte;
    }

    for (i = 1; i < bytes; i++)
        unicode = (unicode << 6) + (chstr[i] & 0x3f);

    *b = bytes;
    return unicode;
}
#endif

/* Handle ALT and CTRL sequences */
static int _handle_alt_keys(int key)
{
    if (key > 0x7f)
        return key;

    if (SP->key_modifiers & PDC_KEY_MODIFIER_CONTROL)
    {
        if (key >= 'A' && key <= 'Z') key -= 64;
        if (key >= 'a' && key <= 'z') key -= 96;
    }
    else if (SP->key_modifiers & PDC_KEY_MODIFIER_ALT)
    {
        if (key >= 'A' && key <= 'Z')
        {
            key += ALT_A - 'A';
            SP->key_code = TRUE;
        } else if (key >= 'a' && key <= 'z')
        {
            key += ALT_A - 'a';
            SP->key_code = TRUE;
        } else if (key >= '0' && key <= '9')
        {
            key += ALT_0 - '0';
            SP->key_code = TRUE;
        }
    }

    return key;
}

static int _process_key_event(void)
{
    int i, key = 0;

#ifdef PDC_WIDE
    size_t bytes;
#endif

    SP->key_code = FALSE;

    if (event.type == SDL_EVENT_KEY_UP)
    {
        switch (event.key.key)
        {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
                SP->key_modifiers &= ~PDC_KEY_MODIFIER_CONTROL;
                break;
            case SDLK_LALT:
            case SDLK_RALT:
                SP->key_modifiers &= ~PDC_KEY_MODIFIER_ALT;
                break;
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
                SP->key_modifiers &= ~PDC_KEY_MODIFIER_SHIFT;
                break;
        }

        if (!(SDL_GetModState() & SDL_KMOD_NUM))
            SP->key_modifiers &= ~PDC_KEY_MODIFIER_NUMLOCK;

        if (SP->return_key_modifiers && event.key.key == oldkey)
        {
            SP->key_code = TRUE;
            switch (event.key.key)
            {
            case SDLK_RSHIFT:
                return KEY_SHIFT_R;
            case SDLK_LSHIFT:
                return KEY_SHIFT_L;
            case SDLK_RCTRL:
                return KEY_CONTROL_R;
            case SDLK_LCTRL:
                return KEY_CONTROL_L;
            case SDLK_RALT:
                return KEY_ALT_R;
            case SDLK_LALT:
                return KEY_ALT_L;
            default:
                break;
            }
        }

        SP->key_code = FALSE;
        return -1;
    }
    else if (event.type == SDL_EVENT_TEXT_INPUT)
    {
#ifdef PDC_WIDE
        if ((key = _utf8_to_unicode(event.text.text, &bytes)) == -1)
        {
            event.text.text[0] = '\0';
        }
        else
        {
            memmove(event.text.text, event.text.text + bytes,
                    strlen(event.text.text) - bytes + 1);
        }
        return _handle_alt_keys(key);
#else
        key = (unsigned char)event.text.text[0];
        memmove((void*)event.text.text, event.text.text + 1,
                strlen(event.text.text));
        return key > 0x7f ? -1 : _handle_alt_keys(key);
#endif
    }

    oldkey = event.key.key;
    if (SDL_GetModState() & SDL_KMOD_NUM)
        SP->key_modifiers |= PDC_KEY_MODIFIER_NUMLOCK;

    switch (event.key.key)
    {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            SP->key_modifiers |= PDC_KEY_MODIFIER_CONTROL;
            break;
        case SDLK_LALT:
        case SDLK_RALT:
            SP->key_modifiers |= PDC_KEY_MODIFIER_ALT;
            break;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            SP->key_modifiers |= PDC_KEY_MODIFIER_SHIFT;
            break;
        case SDLK_RETURN:
            return 0x0d;
        default:
            key = event.key.key;
    }

    for (i = 0; key_table[i].keycode; i++)
    {
        if (key_table[i].keycode == event.key.key)
        {
            if ((event.key.mod & SDL_KMOD_SHIFT) ||
                (key_table[i].numkeypad && (event.key.mod & SDL_KMOD_NUM)))
            {
                key = key_table[i].shifted;
            }
            else if (event.key.mod & SDL_KMOD_CTRL)
            {
                key = key_table[i].control;
            }
            else if (event.key.mod & SDL_KMOD_ALT)
            {
                key = key_table[i].alt;
            }
            else
            {
                /* To get here, we ignore all other modifiers */
                key = key_table[i].normal;
            }

            SP->key_code = (key > 0x100);
            return key;
        }
    }

    /* SDL with TextInput ignores keys with CTRL */
    if (key && SP->key_modifiers & PDC_KEY_MODIFIER_CONTROL)
        return _handle_alt_keys(key);
    return -1;
}

static int _process_mouse_event(void)
{
    SDL_Keymod keymods;
    short shift_flags = 0;

    memset(&SP->mouse_status, 0, sizeof(MOUSE_STATUS));

    keymods = SDL_GetModState();

    if (keymods & SDL_KMOD_SHIFT)
        shift_flags |= BUTTON_SHIFT;

    if (keymods & SDL_KMOD_CTRL)
        shift_flags |= BUTTON_CONTROL;

    if (keymods & SDL_KMOD_ALT)
        shift_flags |= BUTTON_ALT;

    if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        int i;

        SP->mouse_status.x = static_cast<int>((event.motion.x - pdc_xoffset) / pdc_fwidth);
        SP->mouse_status.y = static_cast<int>((event.motion.y - pdc_yoffset) / pdc_fheight);

        if (!event.motion.state ||
           (SP->mouse_status.x == old_mouse_status.x &&
            SP->mouse_status.y == old_mouse_status.y))
            return -1;

        SP->mouse_status.changes = PDC_MOUSE_MOVED;

        for (i = 0; i < 3; i++)
        {
            if (event.motion.state & SDL_BUTTON_MASK(i + 1))
            {
                SP->mouse_status.button[i] = BUTTON_MOVED | shift_flags;
                SP->mouse_status.changes |= (1 << i);
            }
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        SP->mouse_status.x = SP->mouse_status.y = -1;

        if (event.wheel.y > 0)
            SP->mouse_status.changes = PDC_MOUSE_WHEEL_UP;
        else if (event.wheel.y < 0)
            SP->mouse_status.changes = PDC_MOUSE_WHEEL_DOWN;
        else if (event.wheel.x > 0)
            SP->mouse_status.changes = PDC_MOUSE_WHEEL_RIGHT;
        else if (event.wheel.x < 0)
            SP->mouse_status.changes = PDC_MOUSE_WHEEL_LEFT;
        else
            return -1;

        SP->key_code = TRUE;
        return KEY_MOUSE;
    }
    else
    {
        short action = (event.button.down) ?
                       BUTTON_PRESSED : BUTTON_RELEASED;
        Uint8 btn = event.button.button;

        if (btn < 1 || btn > 3)
            return -1;

        /* check for a click -- a press followed immediately by a release */

        if (action == BUTTON_PRESSED && SP->mouse_wait)
        {
            napms(SP->mouse_wait);

            if (!pdc_event_queue.empty())
            {
                const auto & ev = pdc_event_queue.front();

                if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == btn) {
                    action = BUTTON_CLICKED;
                    pdc_event_queue.pop();
                }
            }
        }

        SP->mouse_status.x = static_cast<int>((event.button.x - pdc_xoffset) / pdc_fwidth);
        SP->mouse_status.y = static_cast<int>((event.button.y - pdc_yoffset) / pdc_fheight);

        btn--;

        SP->mouse_status.button[btn] = action | shift_flags;
        SP->mouse_status.changes = (1 << btn);
    }

    old_mouse_status = SP->mouse_status;

    SP->key_code = TRUE;
    return KEY_MOUSE;
}

/* return the next available key or mouse event */

int PDC_get_key(void)
{
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        exit(1);
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        pdc_screen = SDL_GetWindowSurface(pdc_window);
        pdc_sheight = pdc_screen->h - pdc_xoffset;
        pdc_swidth = pdc_screen->w - pdc_yoffset;
        touchwin(curscr);
        wrefresh(curscr);

        if (!SP->resized)
        {
            SP->resized = TRUE;
            SP->key_code = TRUE;
            return KEY_RESIZE;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        SDL_ShowCursor();
        [[fallthrough]];
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
        oldkey = SDLK_SPACE;
        return _process_mouse_event();
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
        PDC_mouse_set();
        return _process_key_event();
    case SDL_EVENT_USER:
        PDC_blink_text();
    }

    return -1;
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
    SP->_trap_mbe ? SDL_ShowCursor() : SDL_HideCursor();

    return OK;
}

int PDC_modifiers_set(void)
{
    return OK;
}
