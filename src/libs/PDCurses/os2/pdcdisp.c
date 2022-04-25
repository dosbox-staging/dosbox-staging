/* PDCurses */

#include "pdcos2.h"
#include "../common/acs437.h"

ULONG pdc_last_blink;
static bool blinked_off = FALSE;

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    PDC_LOG(("PDC_gotoyx() - called: row %d col %d\n", row, col));

    VioSetCurPos(row, col, 0);
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    /* this should be enough for the maximum width of a screen. */

    char temp_line[256];
    int j;
    short fore, back;
    unsigned char mapped_attr;
    bool blink;

    pair_content(PAIR_NUMBER(attr), &fore, &back);
    blink = (SP->termattrs & A_BLINK) && (attr & A_BLINK);

    if (blink)
        attr &= ~A_BLINK;

    if (attr & A_BOLD)
        fore |= 8;
    if (attr & A_BLINK)
        back |= 8;

    fore = pdc_curstoreal[fore];
    back = pdc_curstoreal[back];

    if (attr & A_REVERSE)
        mapped_attr = back | (fore << 4);
    else
        mapped_attr = fore | (back << 4);

    /* replace the attribute part of the chtype with the
       actual color value for each chtype in the line */

    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];

        if (ch & A_ALTCHARSET && !(ch & 0xff80))
            ch = acs_map[ch & 0x7f];

        if (blink && blinked_off)
            ch = ' ';

        temp_line[j] = ch & 0xff;
    }

    VioWrtCharStrAtt((PCH)temp_line, (USHORT)len, (USHORT)lineno,
                     (USHORT)x, (PBYTE)&mapped_attr, 0);
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

void PDC_blink_text(void)
{
    int i, j, k;

    if (!(SP->termattrs & A_BLINK))
        blinked_off = FALSE;
    else
        blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line(i, j, k - j, srcp + j);
                j = k;
            }
    }

    PDC_gotoyx(SP->cursrow, SP->curscol);
    pdc_last_blink = PDC_ms_count();
}

void PDC_doupdate(void)
{
}
