/* PDCurses */

#include "pdcsdl.h"

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));
}

void PDC_napms(int ms)
{
    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    while (ms > 50)
    {
        PDC_pump_and_peep();
        SDL_Delay(50);
        ms -= 50;
    }
    PDC_pump_and_peep();
    SDL_Delay(ms);
}

const char *PDC_sysname(void)
{
    return "SDL2";
}
