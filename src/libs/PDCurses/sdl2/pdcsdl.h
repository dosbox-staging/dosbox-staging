/* PDCurses */

#include <SDL.h>
#ifdef PDC_WIDE
# include <SDL_ttf.h>
#endif

#include <curspriv.h>

#ifdef PDC_WIDE
PDCEX  TTF_Font *pdc_ttffont;
PDCEX  int pdc_font_size;
#endif
PDCEX  SDL_Window *pdc_window;
PDCEX  SDL_Surface *pdc_screen, *pdc_font, *pdc_icon, *pdc_back;
PDCEX  int pdc_sheight, pdc_swidth, pdc_yoffset, pdc_xoffset;

extern SDL_Surface *pdc_tileback;    /* used to regenerate the background
                                        of "transparent" cells */
extern SDL_Color pdc_color[PDC_MAXCOL];  /* colors for font palette */
extern Uint32 pdc_mapped[PDC_MAXCOL];    /* colors for FillRect(), as
                                            used in _highlight() */
extern int pdc_fheight, pdc_fwidth;  /* font height and width */
extern int pdc_fthick;               /* thickness for highlights and
                                        rendered ACS glyphs */
extern int pdc_flastc;               /* font palette's last color
                                        (treated as the foreground) */
extern bool pdc_own_window;          /* if pdc_window was not set
                                        before initscr(), PDCurses is
                                        responsible for (owns) it */

PDCEX  void PDC_update_rects(void);
PDCEX  void PDC_retile(void);

extern void PDC_pump_and_peep(void);
extern void PDC_blink_text(void);
