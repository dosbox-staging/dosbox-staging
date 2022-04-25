/* PDCurses */

#include "pdcos2.h"

/* return width of screen/viewport */

int PDC_get_columns(void)
{
    VIOMODEINFO modeInfo = {0};
    int cols = 0;

    PDC_LOG(("PDC_get_columns() - called\n"));

    modeInfo.cb = sizeof(modeInfo);
    VioGetMode(&modeInfo, 0);
    cols = modeInfo.col;

    PDC_LOG(("PDC_get_columns() - returned: cols %d\n", cols));

    return cols;
}

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
    VIOCURSORINFO cursorInfo;

    PDC_LOG(("PDC_get_cursor_mode() - called\n"));

    VioGetCurType (&cursorInfo, 0);

    return (cursorInfo.yStart << 8) | cursorInfo.cEnd;
}

/* return number of screen rows */

int PDC_get_rows(void)
{
    VIOMODEINFO modeInfo = {0};
    int rows = 0;

    PDC_LOG(("PDC_get_rows() - called\n"));

    modeInfo.cb = sizeof(modeInfo);
    VioGetMode(&modeInfo, 0);
    rows = modeInfo.row;

    PDC_LOG(("PDC_get_rows() - returned: rows %d\n", rows));

    return rows;
}
