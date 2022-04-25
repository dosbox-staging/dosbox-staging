/* PDCurses */

#include "pdcdos.h"

#include <stdlib.h>

/* return width of screen/viewport */

int PDC_get_columns(void)
{
    PDCREGS regs;
    int cols;

    PDC_LOG(("PDC_get_columns() - called\n"));

    regs.h.ah = 0x0f;
    PDCINT(0x10, regs);
    cols = (int)regs.h.ah;

    PDC_LOG(("PDC_get_columns() - returned: cols %d\n", cols));

    return cols;
}

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
    PDC_LOG(("PDC_get_cursor_mode() - called\n"));

    return getdosmemword(0x460);
}

/* return number of screen rows */

int PDC_get_rows(void)
{
    int rows;

    PDC_LOG(("PDC_get_rows() - called\n"));

    rows = getdosmembyte(0x484) + 1;

    if (rows == 1 && pdc_adapter == _MDS_GENIUS)
        rows = 66;
    if (rows == 1 && pdc_adapter == _MDA)
        rows = 25;

    if (rows == 1)
    {
        rows = 25;
        pdc_direct_video = FALSE;
    }

    switch (pdc_adapter)
    {
    case _EGACOLOR:
    case _EGAMONO:
        switch (rows)
        {
        case 25:
        case 43:
            break;
        default:
            rows = 25;
        }
        break;

    case _VGACOLOR:
    case _VGAMONO:
        break;

    default:
        rows = 25;
        break;
    }

    PDC_LOG(("PDC_get_rows() - returned: rows %d\n", rows));

    return rows;
}
