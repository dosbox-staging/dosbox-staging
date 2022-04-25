/* PDCurses */

#include "pdcdos.h"
#include "../common/acs437.h"

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    PDCREGS regs;

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d\n", row, col));

    regs.h.ah = 0x02;
    regs.h.bh = 0;
    regs.h.dh = (unsigned char) row;
    regs.h.dl = (unsigned char) col;
    PDCINT(0x10, regs);
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    attr_t sysattrs;
    int j;
    short fore, back;
    unsigned char mapped_attr;

    sysattrs = SP->termattrs;
    pair_content(PAIR_NUMBER(attr), &fore, &back);

    if (attr & A_BOLD)
        fore |= 8;
    if (attr & A_BLINK)
        back |= 8;

    fore = pdc_curstoreal[fore];
    back = pdc_curstoreal[back];

    if (attr & A_REVERSE)
    {
        if (sysattrs & A_BLINK)
            mapped_attr = (back & 7) | (((fore & 7) | (back & 8)) << 4);
        else
            mapped_attr = back | (fore << 4);
    }
    else
    {
        if ((attr & A_UNDERLINE) && (sysattrs & A_UNDERLINE))
            fore = (fore & 8) | 1;

        mapped_attr = fore | (back << 4);
    }

    if (pdc_direct_video)
    {
#if SMALL || MEDIUM
        struct SREGS segregs;
        int ds;
#endif
        /* this should be enough for the maximum width of a screen */

        struct {unsigned char text, attr;} temp_line[256];

        /* replace the attribute part of the chtype with the actual
           color value for each chtype in the line */

        for (j = 0; j < len; j++)
        {
            chtype ch = srcp[j];

            temp_line[j].attr = mapped_attr;

            if (ch & A_ALTCHARSET && !(ch & 0xff80))
                ch = acs_map[ch & 0x7f];

            temp_line[j].text = ch & 0xff;
        }

#ifdef __DJGPP__
        dosmemput(temp_line, len * 2,
                  (unsigned long)_FAR_POINTER(pdc_video_seg,
                  pdc_video_ofs + (lineno * curscr->_maxx + x) * 2));
#else
# if SMALL || MEDIUM
        segread(&segregs);
        ds = segregs.ds;
        movedata(ds, (int)temp_line, pdc_video_seg,
                 pdc_video_ofs + (lineno * curscr->_maxx + x) * 2, len * 2);
# else
        memcpy((void *)_FAR_POINTER(pdc_video_seg,
               pdc_video_ofs + (lineno * curscr->_maxx + x) * 2),
               temp_line, len * 2);
# endif
#endif

    }
    else
        for (j = 0; j < len;)
        {
            PDCREGS regs;
            unsigned short count = 1;
            chtype ch = srcp[j];

            while ((j + count < len) && (ch == srcp[j + count]))
                count++;

            PDC_gotoyx(lineno, j + x);

            regs.h.ah = 0x09;
            regs.W.bx = mapped_attr;
            regs.W.cx = count;

            if (ch & A_ALTCHARSET && !(ch & 0xff80))
                ch = acs_map[ch & 0x7f];

            regs.h.al = (unsigned char) (ch & 0xff);

            PDCINT(0x10, regs);

            j += count;
        }
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

void PDC_doupdate(void)
{
}
