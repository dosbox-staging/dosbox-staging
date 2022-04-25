/* PDCurses */

#include "pdcsdl.h"

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));
}

void PDC_napms(int ms)
{
    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    PDC_update_rects();
    while (ms > 50)
    {
        SDL_PumpEvents();
        SDL_Delay(50);
        ms -= 50;
    }
    SDL_PumpEvents();
    SDL_Delay(ms);
}

const char *PDC_sysname(void)
{
    return "SDL";
}
