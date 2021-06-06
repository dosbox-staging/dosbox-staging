/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <string.h>

#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "vga.h"

#define _EGA_HALF_CLOCK		0x0001
#define _EGA_LINE_DOUBLE	0x0002
#define _VGA_PIXEL_DOUBLE	0x0004

#define SEQ_REGS 0x05
#define GFX_REGS 0x09
#define ATT_REGS 0x15

VideoModeBlock ModeList_VGA[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},/*was EGA_2*/
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},/*was EGA_2 */
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,0   },

{ 0x054  ,M_TEXT   ,1056,344, 132,43, 8,  8, 1 ,0xB8000 ,0x4000, 160, 449, 132,344, 0   },
{ 0x055  ,M_TEXT   ,1056,400, 132,25, 8, 16, 1 ,0xB8000 ,0x2000, 160, 449, 132,400, 0   },

/* Alias of mode 101 */
{ 0x069  ,M_LIN8   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0	},
/* Alias of mode 102 */
{ 0x06A  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,128 ,663 ,100,600 ,0	},

/* Follow vesa 1.2 for first 0x20 */
{ 0x100  ,M_LIN8   ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 ,0   },
{ 0x101  ,M_LIN8   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0	},
{ 0x102  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,628 ,100,600 ,0	},
{ 0x103  ,M_LIN8   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,628 ,100,600 ,0	},
{ 0x104  ,M_LIN4   ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,806 ,128,768 ,0	},
{ 0x105  ,M_LIN8   ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,806 ,128,768 ,0	},
{ 0x106  ,M_LIN4   ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,212 ,1066,160,1024,0	},
{ 0x107  ,M_LIN8   ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,212 ,1066,160,1024,0	},

/* VESA text modes */ 
{ 0x108  ,M_TEXT   ,640 ,480,  80,60, 8,  8 ,2 ,0xB8000 ,0x4000, 100 ,525 ,80 ,480 ,0   },
{ 0x109  ,M_TEXT   ,1056,400, 132,25, 8, 16, 1 ,0xB8000 ,0x2000, 160, 449, 132,400, 0   },
{ 0x10A  ,M_TEXT   ,1056,688, 132,43, 8,  8, 1 ,0xB8000 ,0x4000, 160, 449, 132,344, 0   },
{ 0x10B  ,M_TEXT   ,1056,400, 132,50, 8,  8, 1 ,0xB8000 ,0x4000, 160, 449, 132,400, 0   },
{ 0x10C  ,M_TEXT   ,1056,480, 132,60, 8,  8, 2 ,0xB8000 ,0x4000, 160, 531, 132,480, 0   },

/* VESA higher color modes */
{ 0x10D  ,M_LIN15  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x10E  ,M_LIN16  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x10F  ,M_LIN32  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,50  ,449 ,40 ,400 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x110  ,M_LIN15  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,525 ,160,480 ,0   },
{ 0x111  ,M_LIN16  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,525 ,160,480 ,0   },
{ 0x112  ,M_LIN32  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0   },
{ 0x113  ,M_LIN15  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,628 ,200,600 ,0   },
{ 0x114  ,M_LIN16  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,628 ,200,600 ,0   },
{ 0x115  ,M_LIN32  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,628 ,100,600 ,0   },
{ 0x116  ,M_LIN15  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,336 ,806 ,256,768 ,0	},
{ 0x117  ,M_LIN16  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,336 ,806 ,256,768 ,0	},
{ 0x118  ,M_LIN32  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,806 ,128,768 ,0	},

/* those should be interlaced but ok */
//{ 0x119  ,M_LIN15  ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,424 ,1066,320,1024,0	},
//{ 0x11A  ,M_LIN16  ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,424 ,1066,320,1024,0	},

{ 0x150  ,M_LIN8   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x151  ,M_LIN8   ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x152  ,M_LIN8   ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _VGA_PIXEL_DOUBLE  },
{ 0x153  ,M_LIN8   ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , _VGA_PIXEL_DOUBLE  },

{ 0x160  ,M_LIN15  ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x161  ,M_LIN15  ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 , 80 ,400 , _VGA_PIXEL_DOUBLE  },
{ 0x162  ,M_LIN15  ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 , _VGA_PIXEL_DOUBLE  },
{ 0x165  ,M_LIN15  ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,449 ,160 ,400 ,0   },

{ 0x170  ,M_LIN16  ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x171  ,M_LIN16  ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 , 80 ,400 , _VGA_PIXEL_DOUBLE  },
{ 0x172  ,M_LIN16  ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 , _VGA_PIXEL_DOUBLE  },
{ 0x175  ,M_LIN16  ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,449 ,160 ,400 ,0   },

{ 0x190  ,M_LIN32  ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000, 50 ,525 ,40 ,480 , _VGA_PIXEL_DOUBLE | _EGA_LINE_DOUBLE },
{ 0x191  ,M_LIN32  ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000, 50 ,449 ,40 ,400 , _VGA_PIXEL_DOUBLE  },
{ 0x192  ,M_LIN32  ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000, 50 ,525 ,40 ,480 , _VGA_PIXEL_DOUBLE  },

/* S3 specific modes */
{ 0x207  ,M_LIN8	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,182 ,948 ,144,864 ,0	},
{ 0x209  ,M_LIN15	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,364 ,948 ,288,864 ,0	},
{ 0x20A  ,M_LIN16	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,364 ,948 ,288,864 ,0	},
//{ 0x20B  ,M_LIN32	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,182 ,948 ,144,864 ,0	},
{ 0x213  ,M_LIN32   ,640 ,400,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 ,0	},

/* Some custom modes */
//{ 0x220  ,M_LIN32  ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,212 ,1066,160,1024,0	},
// A nice 16:9 mode
{ 0x222  ,M_LIN8   ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0	},
{ 0x223  ,M_LIN15  ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,525 ,212 ,480 ,0  },
{ 0x224  ,M_LIN16  ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,525 ,212 ,480 ,0  },
{ 0x225  ,M_LIN32  ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0  },

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock ModeList_VGA_Text_200lines[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,320 ,200 ,40 ,25 ,8 , 8 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK | _EGA_LINE_DOUBLE},
{ 0x001  ,M_TEXT   ,320 ,200 ,40 ,25 ,8 , 8 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK | _EGA_LINE_DOUBLE},
{ 0x002  ,M_TEXT   ,640 ,200 ,80 ,25 ,8 , 8 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE },
{ 0x003  ,M_TEXT   ,640 ,200 ,80 ,25 ,8 , 8 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE }
};

VideoModeBlock ModeList_VGA_Text_350lines[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,320 ,350 ,40 ,25 ,8 ,14 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,350 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,320 ,350 ,40 ,25 ,8 ,14 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,350 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,350 ,0	},
{ 0x003  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,350 ,0	},
{ 0x007  ,M_TEXT   ,720 ,350 ,80 ,25 ,9 ,14 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,350 ,0	}
};

VideoModeBlock ModeList_VGA_Tseng[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},/*was EGA_2*/
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},/*was EGA_2 */
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,0   },

{ 0x018  ,M_TEXT   ,1056 ,688, 132,44, 8, 8, 1 ,0xB0000 ,0x4000, 192, 800, 132, 704, 0 },
{ 0x019  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1 ,0xB0000 ,0x2000, 192, 449, 132, 400, 0 },
{ 0x01A  ,M_TEXT   ,1056 ,400, 132,28, 8, 16,1 ,0xB0000 ,0x2000, 192, 449, 132, 448, 0 },
{ 0x022  ,M_TEXT   ,1056 ,688, 132,44, 8, 8, 1 ,0xB8000 ,0x4000, 192, 800, 132, 704, 0 },
{ 0x023  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1 ,0xB8000 ,0x2000, 192, 449, 132, 400, 0 },
{ 0x024  ,M_TEXT   ,1056 ,400, 132,28, 8, 16,1 ,0xB8000 ,0x2000, 192, 449, 132, 448, 0 },
{ 0x025  ,M_LIN4   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 , 0 },
{ 0x029  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,663 ,100,600 , 0 },
{ 0x02D  ,M_LIN8   ,640 ,350 ,80 ,21 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,350 , 0 },
{ 0x02E  ,M_LIN8   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , 0 },
{ 0x02F  ,M_LIN8   ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , 0 },/* ET4000 only */
{ 0x030  ,M_LIN8   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,128 ,663 ,100,600 , 0 },
{ 0x036  ,M_LIN4   ,960 , 720,120,45 ,8 ,16 ,1 ,0xA0000 ,0xA000, 120 ,800 ,120,720 , 0 },/* STB only */
{ 0x037  ,M_LIN4   ,1024, 768,128,48 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,800 ,128,768 , 0 },
{ 0x038  ,M_LIN8   ,1024 ,768,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,128 ,800 ,128,768 , 0 },/* ET4000 only */
{ 0x03D  ,M_LIN4   ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0xA000, 160 ,1152,160,1024, 0 },/* newer ET4000 */
{ 0x03E  ,M_LIN4   ,1280, 960,160,60 ,8 ,16 ,1 ,0xA0000 ,0xA000, 160 ,1024,160,960 , 0 },/* Definicon only */ 
{ 0x06A  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,663 ,100,600 , 0 },/* newer ET4000 */

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock ModeList_VGA_Paradise[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_LINE_DOUBLE },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},/*was EGA_2*/
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},/*was EGA_2 */
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,0   },

{ 0x054  ,M_TEXT   ,1056 ,688, 132,43, 8, 9, 1, 0xB0000, 0x4000, 192, 720, 132,688, 0 },
{ 0x055  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1, 0xB0000, 0x2000, 192, 449, 132,400, 0 },
{ 0x056  ,M_TEXT   ,1056 ,688, 132,43, 8, 9, 1, 0xB0000, 0x4000, 192, 720, 132,688, 0 },
{ 0x057  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1, 0xB0000, 0x2000, 192, 449, 132,400, 0 },
{ 0x058  ,M_LIN4   ,800 , 600, 100,37, 8, 16,1, 0xA0000, 0xA000, 128 ,663 ,100,600, 0 },
{ 0x05D  ,M_LIN4   ,1024, 768, 128,48 ,8, 16,1, 0xA0000, 0x10000,128 ,800 ,128,768 ,0 }, // documented only on C00 upwards
{ 0x05E  ,M_LIN8   ,640 , 400, 80 ,25, 8, 16,1, 0xA0000, 0x10000,100 ,449 ,80 ,400, 0 },
{ 0x05F  ,M_LIN8   ,640 , 480, 80 ,30, 8, 16,1, 0xA0000, 0x10000,100 ,525 ,80 ,480, 0 },

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};


VideoModeBlock ModeList_EGA[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,320 ,350 ,40 ,25 ,8 ,14 ,8 ,0xB8000 ,0x0800 ,50  ,366 ,40 ,350 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,320 ,350 ,40 ,25 ,8 ,14 ,8 ,0xB8000 ,0x0800 ,50  ,366 ,40 ,350 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,8 ,0xB8000 ,0x1000 ,96  ,366 ,80 ,350 ,0	},
{ 0x003  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,8 ,0xB8000 ,0x1000 ,96  ,366 ,80 ,350 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,60  ,262 ,40 ,200 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,60  ,262 ,40 ,200 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,117 ,262 ,80 ,200 ,_EGA_LINE_DOUBLE},
{ 0x007  ,M_TEXT   ,720 ,350 ,80 ,25 ,9 ,14 ,8 ,0xB0000 ,0x1000 ,101 ,370 ,80 ,350 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,60  ,262 ,40 ,200 ,_EGA_HALF_CLOCK	| _EGA_LINE_DOUBLE	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,117 ,262 ,80 ,200 ,_EGA_LINE_DOUBLE },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,101 ,370 ,80 ,350 ,0	},/*was EGA_2*/
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,96  ,366 ,80 ,350 ,0	},

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock ModeList_OTHER[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde ,special flags */
{ 0x000  ,M_TEXT   ,320 ,400 ,40 ,25 ,8 ,8  ,8 ,0xB8000 ,0x0800 ,56  ,31  ,40 ,25  ,0   },
{ 0x001  ,M_TEXT   ,320 ,400 ,40 ,25 ,8 ,8  ,8 ,0xB8000 ,0x0800 ,56  ,31  ,40 ,25  ,0	},
{ 0x002  ,M_TEXT   ,640 ,400 ,80 ,25 ,8 ,8  ,4 ,0xB8000 ,0x1000 ,113 ,31  ,80 ,25  ,0	},
{ 0x003  ,M_TEXT   ,640 ,400 ,80 ,25 ,8 ,8  ,4 ,0xB8000 ,0x1000 ,113 ,31  ,80 ,25  ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,56  ,127 ,40 ,100 ,0   },
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,56  ,127 ,40 ,100 ,0   },
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,56  ,127 ,40 ,100 ,0   },
{ 0x008  ,M_TANDY16,160 ,200 ,20 ,25 ,8 ,8  ,8 ,0xB8000 ,0x2000 ,56  ,127 ,40 ,100 ,0   },
{ 0x009  ,M_TANDY16,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xB8000 ,0x2000 ,113 ,63  ,80 ,50  ,0   },
{ 0x00A  ,M_CGA4   ,640 ,200 ,80 ,25 ,8 ,8  ,8 ,0xB8000 ,0x2000 ,113 ,63  ,80 ,50  ,0   },
//{ 0x00E  ,M_TANDY16,640 ,200 ,80 ,25 ,8 ,8  ,8 ,0xA0000 ,0x10000 ,113 ,256 ,80 ,200 ,0   },
{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock Hercules_Mode=
{ 0x007  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,1 ,0xB0000 ,0x1000 ,97 ,25  ,80 ,25  ,0	};

static Bit8u text_palette[64][3]=
{
  {0x00,0x00,0x00},{0x00,0x00,0x2a},{0x00,0x2a,0x00},{0x00,0x2a,0x2a},{0x2a,0x00,0x00},{0x2a,0x00,0x2a},{0x2a,0x2a,0x00},{0x2a,0x2a,0x2a},
  {0x00,0x00,0x15},{0x00,0x00,0x3f},{0x00,0x2a,0x15},{0x00,0x2a,0x3f},{0x2a,0x00,0x15},{0x2a,0x00,0x3f},{0x2a,0x2a,0x15},{0x2a,0x2a,0x3f},
  {0x00,0x15,0x00},{0x00,0x15,0x2a},{0x00,0x3f,0x00},{0x00,0x3f,0x2a},{0x2a,0x15,0x00},{0x2a,0x15,0x2a},{0x2a,0x3f,0x00},{0x2a,0x3f,0x2a},
  {0x00,0x15,0x15},{0x00,0x15,0x3f},{0x00,0x3f,0x15},{0x00,0x3f,0x3f},{0x2a,0x15,0x15},{0x2a,0x15,0x3f},{0x2a,0x3f,0x15},{0x2a,0x3f,0x3f},
  {0x15,0x00,0x00},{0x15,0x00,0x2a},{0x15,0x2a,0x00},{0x15,0x2a,0x2a},{0x3f,0x00,0x00},{0x3f,0x00,0x2a},{0x3f,0x2a,0x00},{0x3f,0x2a,0x2a},
  {0x15,0x00,0x15},{0x15,0x00,0x3f},{0x15,0x2a,0x15},{0x15,0x2a,0x3f},{0x3f,0x00,0x15},{0x3f,0x00,0x3f},{0x3f,0x2a,0x15},{0x3f,0x2a,0x3f},
  {0x15,0x15,0x00},{0x15,0x15,0x2a},{0x15,0x3f,0x00},{0x15,0x3f,0x2a},{0x3f,0x15,0x00},{0x3f,0x15,0x2a},{0x3f,0x3f,0x00},{0x3f,0x3f,0x2a},
  {0x15,0x15,0x15},{0x15,0x15,0x3f},{0x15,0x3f,0x15},{0x15,0x3f,0x3f},{0x3f,0x15,0x15},{0x3f,0x15,0x3f},{0x3f,0x3f,0x15},{0x3f,0x3f,0x3f}
};

static Bit8u mtext_palette[64][3]=
{
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
  {0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
  {0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
  {0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
  {0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f} 
};

static Bit8u mtext_s3_palette[64][3]=
{
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
  {0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
  {0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
  {0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
  {0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
  {0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
  {0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f} 
};

static Bit8u ega_palette[64][3]=
{
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
  {0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f}
};

static Bit8u cga_palette[16][3]=
{
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
};

static Bit8u cga_palette_2[64][3]=
{
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
};

static Bit8u vga_palette[248][3]=
{
  {0x00,0x00,0x00},{0x00,0x00,0x2a},{0x00,0x2a,0x00},{0x00,0x2a,0x2a},{0x2a,0x00,0x00},{0x2a,0x00,0x2a},{0x2a,0x15,0x00},{0x2a,0x2a,0x2a},
  {0x15,0x15,0x15},{0x15,0x15,0x3f},{0x15,0x3f,0x15},{0x15,0x3f,0x3f},{0x3f,0x15,0x15},{0x3f,0x15,0x3f},{0x3f,0x3f,0x15},{0x3f,0x3f,0x3f},
  {0x00,0x00,0x00},{0x05,0x05,0x05},{0x08,0x08,0x08},{0x0b,0x0b,0x0b},{0x0e,0x0e,0x0e},{0x11,0x11,0x11},{0x14,0x14,0x14},{0x18,0x18,0x18},
  {0x1c,0x1c,0x1c},{0x20,0x20,0x20},{0x24,0x24,0x24},{0x28,0x28,0x28},{0x2d,0x2d,0x2d},{0x32,0x32,0x32},{0x38,0x38,0x38},{0x3f,0x3f,0x3f},
  {0x00,0x00,0x3f},{0x10,0x00,0x3f},{0x1f,0x00,0x3f},{0x2f,0x00,0x3f},{0x3f,0x00,0x3f},{0x3f,0x00,0x2f},{0x3f,0x00,0x1f},{0x3f,0x00,0x10},
  {0x3f,0x00,0x00},{0x3f,0x10,0x00},{0x3f,0x1f,0x00},{0x3f,0x2f,0x00},{0x3f,0x3f,0x00},{0x2f,0x3f,0x00},{0x1f,0x3f,0x00},{0x10,0x3f,0x00},
  {0x00,0x3f,0x00},{0x00,0x3f,0x10},{0x00,0x3f,0x1f},{0x00,0x3f,0x2f},{0x00,0x3f,0x3f},{0x00,0x2f,0x3f},{0x00,0x1f,0x3f},{0x00,0x10,0x3f},
  {0x1f,0x1f,0x3f},{0x27,0x1f,0x3f},{0x2f,0x1f,0x3f},{0x37,0x1f,0x3f},{0x3f,0x1f,0x3f},{0x3f,0x1f,0x37},{0x3f,0x1f,0x2f},{0x3f,0x1f,0x27},

  {0x3f,0x1f,0x1f},{0x3f,0x27,0x1f},{0x3f,0x2f,0x1f},{0x3f,0x37,0x1f},{0x3f,0x3f,0x1f},{0x37,0x3f,0x1f},{0x2f,0x3f,0x1f},{0x27,0x3f,0x1f},
  {0x1f,0x3f,0x1f},{0x1f,0x3f,0x27},{0x1f,0x3f,0x2f},{0x1f,0x3f,0x37},{0x1f,0x3f,0x3f},{0x1f,0x37,0x3f},{0x1f,0x2f,0x3f},{0x1f,0x27,0x3f},
  {0x2d,0x2d,0x3f},{0x31,0x2d,0x3f},{0x36,0x2d,0x3f},{0x3a,0x2d,0x3f},{0x3f,0x2d,0x3f},{0x3f,0x2d,0x3a},{0x3f,0x2d,0x36},{0x3f,0x2d,0x31},
  {0x3f,0x2d,0x2d},{0x3f,0x31,0x2d},{0x3f,0x36,0x2d},{0x3f,0x3a,0x2d},{0x3f,0x3f,0x2d},{0x3a,0x3f,0x2d},{0x36,0x3f,0x2d},{0x31,0x3f,0x2d},
  {0x2d,0x3f,0x2d},{0x2d,0x3f,0x31},{0x2d,0x3f,0x36},{0x2d,0x3f,0x3a},{0x2d,0x3f,0x3f},{0x2d,0x3a,0x3f},{0x2d,0x36,0x3f},{0x2d,0x31,0x3f},
  {0x00,0x00,0x1c},{0x07,0x00,0x1c},{0x0e,0x00,0x1c},{0x15,0x00,0x1c},{0x1c,0x00,0x1c},{0x1c,0x00,0x15},{0x1c,0x00,0x0e},{0x1c,0x00,0x07},
  {0x1c,0x00,0x00},{0x1c,0x07,0x00},{0x1c,0x0e,0x00},{0x1c,0x15,0x00},{0x1c,0x1c,0x00},{0x15,0x1c,0x00},{0x0e,0x1c,0x00},{0x07,0x1c,0x00},
  {0x00,0x1c,0x00},{0x00,0x1c,0x07},{0x00,0x1c,0x0e},{0x00,0x1c,0x15},{0x00,0x1c,0x1c},{0x00,0x15,0x1c},{0x00,0x0e,0x1c},{0x00,0x07,0x1c},

  {0x0e,0x0e,0x1c},{0x11,0x0e,0x1c},{0x15,0x0e,0x1c},{0x18,0x0e,0x1c},{0x1c,0x0e,0x1c},{0x1c,0x0e,0x18},{0x1c,0x0e,0x15},{0x1c,0x0e,0x11},
  {0x1c,0x0e,0x0e},{0x1c,0x11,0x0e},{0x1c,0x15,0x0e},{0x1c,0x18,0x0e},{0x1c,0x1c,0x0e},{0x18,0x1c,0x0e},{0x15,0x1c,0x0e},{0x11,0x1c,0x0e},
  {0x0e,0x1c,0x0e},{0x0e,0x1c,0x11},{0x0e,0x1c,0x15},{0x0e,0x1c,0x18},{0x0e,0x1c,0x1c},{0x0e,0x18,0x1c},{0x0e,0x15,0x1c},{0x0e,0x11,0x1c},
  {0x14,0x14,0x1c},{0x16,0x14,0x1c},{0x18,0x14,0x1c},{0x1a,0x14,0x1c},{0x1c,0x14,0x1c},{0x1c,0x14,0x1a},{0x1c,0x14,0x18},{0x1c,0x14,0x16},
  {0x1c,0x14,0x14},{0x1c,0x16,0x14},{0x1c,0x18,0x14},{0x1c,0x1a,0x14},{0x1c,0x1c,0x14},{0x1a,0x1c,0x14},{0x18,0x1c,0x14},{0x16,0x1c,0x14},
  {0x14,0x1c,0x14},{0x14,0x1c,0x16},{0x14,0x1c,0x18},{0x14,0x1c,0x1a},{0x14,0x1c,0x1c},{0x14,0x1a,0x1c},{0x14,0x18,0x1c},{0x14,0x16,0x1c},
  {0x00,0x00,0x10},{0x04,0x00,0x10},{0x08,0x00,0x10},{0x0c,0x00,0x10},{0x10,0x00,0x10},{0x10,0x00,0x0c},{0x10,0x00,0x08},{0x10,0x00,0x04},
  {0x10,0x00,0x00},{0x10,0x04,0x00},{0x10,0x08,0x00},{0x10,0x0c,0x00},{0x10,0x10,0x00},{0x0c,0x10,0x00},{0x08,0x10,0x00},{0x04,0x10,0x00},

  {0x00,0x10,0x00},{0x00,0x10,0x04},{0x00,0x10,0x08},{0x00,0x10,0x0c},{0x00,0x10,0x10},{0x00,0x0c,0x10},{0x00,0x08,0x10},{0x00,0x04,0x10},
  {0x08,0x08,0x10},{0x0a,0x08,0x10},{0x0c,0x08,0x10},{0x0e,0x08,0x10},{0x10,0x08,0x10},{0x10,0x08,0x0e},{0x10,0x08,0x0c},{0x10,0x08,0x0a},
  {0x10,0x08,0x08},{0x10,0x0a,0x08},{0x10,0x0c,0x08},{0x10,0x0e,0x08},{0x10,0x10,0x08},{0x0e,0x10,0x08},{0x0c,0x10,0x08},{0x0a,0x10,0x08},
  {0x08,0x10,0x08},{0x08,0x10,0x0a},{0x08,0x10,0x0c},{0x08,0x10,0x0e},{0x08,0x10,0x10},{0x08,0x0e,0x10},{0x08,0x0c,0x10},{0x08,0x0a,0x10},
  {0x0b,0x0b,0x10},{0x0c,0x0b,0x10},{0x0d,0x0b,0x10},{0x0f,0x0b,0x10},{0x10,0x0b,0x10},{0x10,0x0b,0x0f},{0x10,0x0b,0x0d},{0x10,0x0b,0x0c},
  {0x10,0x0b,0x0b},{0x10,0x0c,0x0b},{0x10,0x0d,0x0b},{0x10,0x0f,0x0b},{0x10,0x10,0x0b},{0x0f,0x10,0x0b},{0x0d,0x10,0x0b},{0x0c,0x10,0x0b},
  {0x0b,0x10,0x0b},{0x0b,0x10,0x0c},{0x0b,0x10,0x0d},{0x0b,0x10,0x0f},{0x0b,0x10,0x10},{0x0b,0x0f,0x10},{0x0b,0x0d,0x10},{0x0b,0x0c,0x10}
};
VideoModeBlock * CurMode;

static bool SetCurMode(VideoModeBlock modeblock[],Bit16u mode) {
	Bitu i=0;
	while (modeblock[i].mode!=0xffff) {
		if (modeblock[i].mode!=mode) i++;
		else {
			if ((!int10.vesa_oldvbe) || (ModeList_VGA[i].mode<0x120)) {
				CurMode=&modeblock[i];
				return true;
			}
			return false;
		}
	}
	return false;
}

static void SetTextLines(void) {
	// check for scanline backwards compatibility (VESA text modes??)
	switch (real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL)&0x90) {
	case 0x80: // 200 lines emulation
		if (CurMode->mode <= 3) {
			CurMode = &ModeList_VGA_Text_200lines[CurMode->mode];
		} else if (CurMode->mode == 7) {
			CurMode = &ModeList_VGA_Text_350lines[4];
		}
		break;
	case 0x00: // 350 lines emulation
		if (CurMode->mode <= 3) {
			CurMode = &ModeList_VGA_Text_350lines[CurMode->mode];
		} else if (CurMode->mode == 7) {
			CurMode = &ModeList_VGA_Text_350lines[4];
		}
		break;
	}
}

void INT10_SetCurMode(void) {
	Bit16u bios_mode=(Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE);
	if (GCC_UNLIKELY(CurMode->mode!=bios_mode)) {
		bool mode_changed=false;
		switch (machine) {
		case MCH_CGA:
			if (bios_mode<7) mode_changed=SetCurMode(ModeList_OTHER,bios_mode);
			break;
		case TANDY_ARCH_CASE:
			if (bios_mode!=7 && bios_mode<=0xa) mode_changed=SetCurMode(ModeList_OTHER,bios_mode);
			break;
		case MCH_HERC:
			if (bios_mode<7) mode_changed=SetCurMode(ModeList_OTHER,bios_mode);
			else if (bios_mode==7) {mode_changed=true;CurMode=&Hercules_Mode;}
			break;
		case MCH_EGA:
			mode_changed=SetCurMode(ModeList_EGA,bios_mode);
			break;
		case VGA_ARCH_CASE:
			switch (svgaCard) {
			case SVGA_TsengET4K:
			case SVGA_TsengET3K:
				mode_changed=SetCurMode(ModeList_VGA_Tseng,bios_mode);
				break;
			case SVGA_ParadisePVGA1A:
				mode_changed=SetCurMode(ModeList_VGA_Paradise,bios_mode);
				break;
			case SVGA_S3Trio:
				if (bios_mode>=0x68 && CurMode->mode==(bios_mode+0x98)) break;
				// fall-through
			default:
				mode_changed=SetCurMode(ModeList_VGA,bios_mode);
				break;
			}
			if (mode_changed && CurMode->type==M_TEXT) SetTextLines();
			break;
		}
		if (mode_changed) LOG(LOG_INT10,LOG_WARN)("BIOS video mode changed to %X",bios_mode);
	}
}

static void FinishSetMode(bool clearmem) {
	/* Clear video memory if needs be */
	if (clearmem) {
		switch (CurMode->type) {
		case M_TANDY16:
		case M_CGA4:
			if ((machine==MCH_PCJR) && (CurMode->mode >= 9)) {
				// PCJR cannot access the full 32k at 0xb800
				for (Bit16u ct=0;ct<16*1024;ct++) {
					// 0x1800 is the last 32k block in 128k, as set in the CRTCPU_PAGE register 
					real_writew(0x1800,ct*2,0x0000);
				}
				break;
			}
			// fall-through
		case M_CGA2:
			for (Bit16u ct=0;ct<16*1024;ct++) {
				real_writew( 0xb800,ct*2,0x0000);
			}
			break;
		case M_TEXT: {
			Bit16u seg = (CurMode->mode==7)?0xb000:0xb800;
			for (Bit16u ct=0;ct<16*1024;ct++) real_writew(seg,ct*2,0x0720);
			break;
		}
		case M_EGA:	
		case M_VGA:
		case M_LIN8:
		case M_LIN4:
		case M_LIN15:
		case M_LIN16:
		case M_LIN32:
			/* Hack we just access the memory directly */
			memset(vga.mem.linear,0,vga.vmemsize);
			memset(vga.fastmem, 0, vga.vmemsize<<1);
		}
	}
	/* Setup the BIOS */
	if (CurMode->mode<128) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,(Bit8u)CurMode->mode);
	else real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,(Bit8u)(CurMode->mode-0x98));	//Looks like the s3 bios
	real_writew(BIOSMEM_SEG,BIOSMEM_NB_COLS,(Bit16u)CurMode->twidth);
	real_writew(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,(Bit16u)CurMode->plength);
	real_writew(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS,((CurMode->mode==7 )|| (CurMode->mode==0x0f)) ? 0x3b4 : 0x3d4);

	if (IS_EGAVGA_ARCH) {
		real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,(Bit8u)(CurMode->theight-1));
		real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,(Bit16u)CurMode->cheight);
		real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,(0x60|(clearmem?0:0x80)));
		real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0x09);
		// this is an index into the dcc table:
		if (IS_VGA_ARCH) real_writeb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX,0x0b);

		/* Set font pointer */
		if (CurMode->mode<=3 || CurMode->mode==7)
			RealSetVec(0x43,int10.rom.font_8_first);
		else {
			switch (CurMode->cheight) {
			case 8:RealSetVec(0x43,int10.rom.font_8_first);break;
			case 14:RealSetVec(0x43,int10.rom.font_14);break;
			case 16:RealSetVec(0x43,int10.rom.font_16);break;
			}
		}
	}

	// Set cursor shape
	if (CurMode->type==M_TEXT) {
		INT10_SetCursorShape(0x06,0x07);
	}
	// Set cursor pos for page 0..7
	for (Bit8u ct=0;ct<8;ct++) INT10_SetCursorPos(0,0,ct);
	// Set active page 0
	INT10_SetActivePage(0);
}

bool INT10_SetVideoMode_OTHER(Bit16u mode,bool clearmem) {
	switch (machine) {
	case MCH_CGA:
		if (mode>6) return false;
	case TANDY_ARCH_CASE:
		if (mode>0xa) return false;
		if (mode==7) mode=0; // PCJR defaults to 0 on illegal mode 7
		if (!SetCurMode(ModeList_OTHER,mode)) {
			LOG(LOG_INT10,LOG_ERROR)("Trying to set illegal mode %X",mode);
			return false;
		}
		break;
	case MCH_HERC:
		// Allow standard color modes if equipment word is not set to mono (Victory Road)
		if ((real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0x30)!=0x30 && mode<7) {
			SetCurMode(ModeList_OTHER,mode);
			FinishSetMode(clearmem);
			return true;
		}
		CurMode=&Hercules_Mode;
		mode=7; // in case the video parameter table is modified
		break;
	}
	LOG(LOG_INT10,LOG_NORMAL)("Set Video Mode %X",mode);

	/* Setup the VGA to the correct mode */
//	VGA_SetMode(CurMode->type);
	/* Setup the CRTC */
	Bitu crtc_base=machine==MCH_HERC ? 0x3b4 : 0x3d4;
	//Horizontal total
	IO_WriteW(crtc_base,0x00 | (CurMode->htotal) << 8);
	//Horizontal displayed
	IO_WriteW(crtc_base,0x01 | (CurMode->hdispend) << 8);
	//Horizontal sync position
	IO_WriteW(crtc_base,0x02 | (CurMode->hdispend+1) << 8);
	//Horizontal sync width, seems to be fixed to 0xa, for cga at least, hercules has 0xf
	IO_WriteW(crtc_base,0x03 | (0xa) << 8);
	////Vertical total
	IO_WriteW(crtc_base,0x04 | (CurMode->vtotal) << 8);
	//Vertical total adjust, 6 for cga,hercules,tandy
	IO_WriteW(crtc_base,0x05 | (6) << 8);
	//Vertical displayed
	IO_WriteW(crtc_base,0x06 | (CurMode->vdispend) << 8);
	//Vertical sync position
	IO_WriteW(crtc_base,0x07 | (CurMode->vdispend + ((CurMode->vtotal - CurMode->vdispend)/2)-1) << 8);
	//Maximum scanline
	Bit8u scanline,crtpage;
	scanline=8;
	switch(CurMode->type) {
	case M_TEXT:
		if (machine==MCH_HERC) scanline=14;
		else scanline=8;
		break;
	case M_CGA2:
		scanline=2;
		break;
	case M_CGA4:
		if (CurMode->mode!=0xa) scanline=2;
		else scanline=4;
		break;
	case M_TANDY16:
		if (CurMode->mode!=0x9) scanline=2;
		else scanline=4;
		break;
	}
	IO_WriteW(crtc_base,0x09 | (scanline-1) << 8);
	//Setup the CGA palette using VGA DAC palette
	for (Bit8u ct=0;ct<16;ct++) VGA_DAC_SetEntry(ct,cga_palette[ct][0],cga_palette[ct][1],cga_palette[ct][2]);
	//Setup the tandy palette
	for (Bit8u ct=0;ct<16;ct++) VGA_DAC_CombineColor(ct,ct);
	//Setup the special registers for each machine type
	Bit8u mode_control_list[0xa+1]={
		0x2c,0x28,0x2d,0x29,	//0-3
		0x2a,0x2e,0x1e,0x29,	//4-7
		0x2a,0x2b,0x3b			//8-a
	};
	Bit8u mode_control_list_pcjr[0xa+1]={
		0x0c,0x08,0x0d,0x09,	//0-3
		0x0a,0x0e,0x0e,0x09,	//4-7		
		0x1a,0x1b,0x0b			//8-a
	};
	Bit8u mode_control,color_select;
	switch (machine) {
	case MCH_HERC:
		IO_WriteB(0x3b8,0x28);	// TEXT mode and blinking characters

		Herc_Palette();
		VGA_DAC_CombineColor(0,0);
		VGA_DAC_CombineColor(1,7);

		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x29); // attribute controls blinking
		break;
	case MCH_CGA:
		mode_control=mode_control_list[CurMode->mode];
		if (CurMode->mode == 0x6) color_select=0x3f;
		else color_select=0x30;
		IO_WriteB(0x3d8,mode_control);
		IO_WriteB(0x3d9,color_select);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		break;
	case MCH_TANDY:
		/* Init some registers */
		IO_WriteB(0x3da,0x1);IO_WriteB(0x3de,0xf);		//Palette mask always 0xf
		IO_WriteB(0x3da,0x2);IO_WriteB(0x3de,0x0);		//black border
		IO_WriteB(0x3da,0x3);							//Tandy color overrides?
		switch (CurMode->mode) {
		case 0x8:	
			IO_WriteB(0x3de,0x14);break;
		case 0x9:
			IO_WriteB(0x3de,0x14);break;
		case 0xa:
			IO_WriteB(0x3de,0x0c);break;
		default:
			IO_WriteB(0x3de,0x0);break;
		}
		// write palette
		for(Bitu i = 0; i < 16; i++) {
			IO_WriteB(0x3da,i+0x10);
			IO_WriteB(0x3de,i);
		}
		//Clear extended mapping
		IO_WriteB(0x3da,0x5);
		IO_WriteB(0x3de,0x0);
		//Clear monitor mode
		IO_WriteB(0x3da,0x8);
		IO_WriteB(0x3de,0x0);
		crtpage=(CurMode->mode>=0x9) ? 0xf6 : 0x3f;
		IO_WriteB(0x3df,crtpage);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CRTCPU_PAGE,crtpage);
		mode_control=mode_control_list[CurMode->mode];
		if (CurMode->mode == 0x6 || CurMode->mode==0xa) color_select=0x3f;
		else color_select=0x30;
		IO_WriteB(0x3d8,mode_control);
		IO_WriteB(0x3d9,color_select);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		break;
	case MCH_PCJR:
		/* Init some registers */
		IO_ReadB(0x3da);
		IO_WriteB(0x3da,0x1);IO_WriteB(0x3da,0xf);		//Palette mask always 0xf
		IO_WriteB(0x3da,0x2);IO_WriteB(0x3da,0x0);		//black border
		IO_WriteB(0x3da,0x3);
		if (CurMode->mode<=0x04) IO_WriteB(0x3da,0x02);
		else if (CurMode->mode==0x06) IO_WriteB(0x3da,0x08);
		else IO_WriteB(0x3da,0x00);

		/* set CRT/Processor page register */
		if (CurMode->mode<0x04) crtpage=0x3f;
		else if (CurMode->mode>=0x09) crtpage=0xf6;
		else crtpage=0x7f;
		IO_WriteB(0x3df,crtpage);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CRTCPU_PAGE,crtpage);

		mode_control=mode_control_list_pcjr[CurMode->mode];
		IO_WriteB(0x3da,0x0);IO_WriteB(0x3da,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);

		if (CurMode->mode == 0x6 || CurMode->mode==0xa) color_select=0x3f;
		else color_select=0x30;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		INT10_SetColorSelect(1);
		INT10_SetBackgroundBorder(0);
		break;
	}

	RealPt vparams = RealGetVec(0x1d);
	if ((vparams != RealMake(0xf000,0xf0a4)) && (mode < 8)) {
		// load crtc parameters from video params table
		Bit16u crtc_block_index = 0;
		if (mode < 2) crtc_block_index = 0;
		else if (mode < 4) crtc_block_index = 1;
		else if (mode < 7) crtc_block_index = 2;
		else if (mode == 7) crtc_block_index = 3; // MDA mono mode; invalid for others
		else if (mode < 9) crtc_block_index = 2;
		else crtc_block_index = 3; // Tandy/PCjr modes

		// init CRTC registers
		for (Bit16u i = 0; i < 16; i++)
			IO_WriteW(crtc_base, i | (real_readb(RealSeg(vparams), 
				RealOff(vparams) + i + crtc_block_index*16) << 8));
	}
	FinishSetMode(clearmem);
	return true;
}


bool INT10_SetVideoMode(Bit16u mode) {
	bool clearmem=true;Bitu i;
	if (mode>=0x100) {
		if ((mode & 0x4000) && int10.vesa_nolfb) return false;
		if (mode & 0x8000) clearmem=false;
		mode&=0xfff;
	}
	if ((mode<0x100) && (mode & 0x80)) {
		clearmem=false;
		mode-=0x80;
	}
	int10.vesa_setmode=0xffff;
	LOG(LOG_INT10,LOG_NORMAL)("Set Video Mode %X",mode);
	if (!IS_EGAVGA_ARCH) return INT10_SetVideoMode_OTHER(mode,clearmem);

	/* First read mode setup settings from bios area */
//	Bit8u video_ctl=real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL);
//	Bit8u vga_switches=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES);
	Bit8u modeset_ctl=real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL);

	if (IS_VGA_ARCH) {
		if (svga.accepts_mode) {
			if (!svga.accepts_mode(mode)) return false;
		}

		switch(svgaCard) {
		case SVGA_TsengET4K:
		case SVGA_TsengET3K:
			if (!SetCurMode(ModeList_VGA_Tseng,mode)){
				LOG(LOG_INT10,LOG_ERROR)("VGA:Trying to set illegal mode %X",mode);
				return false;
			}
			break;
		case SVGA_ParadisePVGA1A:
			if (!SetCurMode(ModeList_VGA_Paradise,mode)){
				LOG(LOG_INT10,LOG_ERROR)("VGA:Trying to set illegal mode %X",mode);
				return false;
			}
			break;
		default:
			if (!SetCurMode(ModeList_VGA,mode)){
				LOG(LOG_INT10,LOG_ERROR)("VGA:Trying to set illegal mode %X",mode);
				return false;
			}
		}
		if (CurMode->type==M_TEXT) SetTextLines();
	} else {
		if (!SetCurMode(ModeList_EGA,mode)){
			LOG(LOG_INT10,LOG_ERROR)("EGA:Trying to set illegal mode %X",mode);
			return false;
		}
	}

	/* Setup the VGA to the correct mode */

	Bit16u crtc_base;
	bool mono_mode=(mode == 7) || (mode==0xf);  
	if (mono_mode) crtc_base=0x3b4;
	else crtc_base=0x3d4;

	if (IS_VGA_ARCH && (svgaCard == SVGA_S3Trio)) {
		// Disable MMIO here so we can read / write memory
		IO_Write(crtc_base,0x53);
		IO_Write(crtc_base+1,0x0);
	}

	/* Setup MISC Output Register */
	Bit8u misc_output=0x2 | (mono_mode ? 0x0 : 0x1);

	if (machine==MCH_EGA) {
		// 16MHz clock for 350-line EGA modes except mode F
		if ((CurMode->vdispend==350) && (mode!=0xf)) misc_output|=0x4;
	} else {
		// 28MHz clock for 9-pixel wide chars
		if ((CurMode->type==M_TEXT) && (CurMode->cwidth==9)) misc_output|=0x4;
	}

	switch (CurMode->vdispend) {
	case 400: 
		misc_output|=0x60;
		break;
	case 480:
		misc_output|=0xe0;
		break;
	case 350:
		misc_output|=0xa0;
		break;
	default:
		misc_output|=0x60;
	}
	IO_Write(0x3c2,misc_output);		//Setup for 3b4 or 3d4
	
	/* Program Sequencer */
	Bit8u seq_data[SEQ_REGS];
	memset(seq_data,0,SEQ_REGS);
	seq_data[1]|=0x01;	//8 dot fonts by default
	if (CurMode->special & _EGA_HALF_CLOCK) seq_data[1]|=0x08; //Check for half clock
	if ((machine==MCH_EGA) && (CurMode->special & _EGA_HALF_CLOCK)) seq_data[1]|=0x02;
	seq_data[4]|=0x02;	//More than 64kb
	switch (CurMode->type) {
	case M_TEXT:
		if (CurMode->cwidth==9) seq_data[1] &= ~1;
		seq_data[2]|=0x3;				//Enable plane 0 and 1
		seq_data[4]|=0x01;				//Alpanumeric
		if (IS_VGA_ARCH) seq_data[4]|=0x04;				//odd/even enabled
		break;
	case M_CGA2:
		seq_data[2]|=0xf;				//Enable plane 0
		if (machine==MCH_EGA) seq_data[4]|=0x04;		//odd/even enabled
		break;
	case M_CGA4:
		if (machine==MCH_EGA) seq_data[2]|=0x03;		//Enable plane 0 and 1
		break;
	case M_LIN4:
	case M_EGA:
		seq_data[2]|=0xf;				//Enable all planes for writing
		if (machine==MCH_EGA) seq_data[4]|=0x04;		//odd/even enabled
		break;
	case M_LIN8:						//Seems to have the same reg layout from testing
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
	case M_VGA:
		seq_data[2]|=0xf;				//Enable all planes for writing
		seq_data[4]|=0xc;				//Graphics - odd/even - Chained
		break;
	}
	for (Bit8u ct=0;ct<SEQ_REGS;ct++) {
		IO_Write(0x3c4,ct);
		IO_Write(0x3c5,seq_data[ct]);
	}
	vga.config.compatible_chain4 = true; // this may be changed by SVGA chipset emulation

	/* Program CRTC */
	/* First disable write protection */
	IO_Write(crtc_base,0x11);
	IO_Write(crtc_base+1,IO_Read(crtc_base+1)&0x7f);
	/* Clear all the regs */
	for (Bit8u ct=0x0;ct<=0x18;ct++) {
		IO_Write(crtc_base,ct);IO_Write(crtc_base+1,0);
	}
	Bit8u overflow=0;Bit8u max_scanline=0;
	Bit8u ver_overflow=0;Bit8u hor_overflow=0;
	/* Horizontal Total */
	IO_Write(crtc_base,0x00);IO_Write(crtc_base+1,(Bit8u)(CurMode->htotal-5));
	hor_overflow|=((CurMode->htotal-5) & 0x100) >> 8;
	/* Horizontal Display End */
	IO_Write(crtc_base,0x01);IO_Write(crtc_base+1,(Bit8u)(CurMode->hdispend-1));
	hor_overflow|=((CurMode->hdispend-1) & 0x100) >> 7;
	/* Start horizontal Blanking */
	IO_Write(crtc_base,0x02);IO_Write(crtc_base+1,(Bit8u)CurMode->hdispend);
	hor_overflow|=((CurMode->hdispend) & 0x100) >> 6;
	/* End horizontal Blanking */
	Bitu blank_end=(CurMode->htotal-2) & 0x7f;
	IO_Write(crtc_base,0x03);IO_Write(crtc_base+1,0x80|(blank_end & 0x1f));

	/* Start Horizontal Retrace */
	Bitu ret_start;
	if ((CurMode->special & _EGA_HALF_CLOCK) && (CurMode->type!=M_CGA2)) ret_start = (CurMode->hdispend+3);
	else if (CurMode->type==M_TEXT) ret_start = (CurMode->hdispend+5);
	else ret_start = (CurMode->hdispend+4);
	IO_Write(crtc_base,0x04);IO_Write(crtc_base+1,(Bit8u)ret_start);
	hor_overflow|=(ret_start & 0x100) >> 4;

	/* End Horizontal Retrace */
	Bitu ret_end;
	if (CurMode->special & _EGA_HALF_CLOCK) {
		if (CurMode->type==M_CGA2) ret_end=0;	// mode 6
		else if (CurMode->special & _EGA_LINE_DOUBLE) ret_end = (CurMode->htotal-18) & 0x1f;
		else ret_end = ((CurMode->htotal-18) & 0x1f) | 0x20; // mode 0&1 have 1 char sync delay
	} else if (CurMode->type==M_TEXT) ret_end = (CurMode->htotal-3) & 0x1f;
	else ret_end = (CurMode->htotal-4) & 0x1f;
	
	IO_Write(crtc_base,0x05);IO_Write(crtc_base+1,(Bit8u)(ret_end | (blank_end & 0x20) << 2));

	/* Vertical Total */
	IO_Write(crtc_base,0x06);IO_Write(crtc_base+1,(Bit8u)(CurMode->vtotal-2));
	overflow|=((CurMode->vtotal-2) & 0x100) >> 8;
	overflow|=((CurMode->vtotal-2) & 0x200) >> 4;
	ver_overflow|=((CurMode->vtotal-2) & 0x400) >> 10;

	Bitu vretrace;
	if (IS_VGA_ARCH) {
		switch (CurMode->vdispend) {
		case 400: vretrace=CurMode->vdispend+12;
				break;
		case 480: vretrace=CurMode->vdispend+10;
				break;
		case 350: vretrace=CurMode->vdispend+37;
				break;
		default: vretrace=CurMode->vdispend+12;
		}
	} else {
		switch (CurMode->vdispend) {
		case 350: vretrace=CurMode->vdispend;
				break;
		default: vretrace=CurMode->vdispend+24;
		}
	}

	/* Vertical Retrace Start */
	IO_Write(crtc_base,0x10);IO_Write(crtc_base+1,(Bit8u)vretrace);
	overflow|=(vretrace & 0x100) >> 6;
	overflow|=(vretrace & 0x200) >> 2;
	ver_overflow|=(vretrace & 0x400) >> 6;

	/* Vertical Retrace End */
	IO_Write(crtc_base,0x11);IO_Write(crtc_base+1,(vretrace+2) & 0xF);

	/* Vertical Display End */
	IO_Write(crtc_base,0x12);IO_Write(crtc_base+1,(Bit8u)(CurMode->vdispend-1));
	overflow|=((CurMode->vdispend-1) & 0x100) >> 7;
	overflow|=((CurMode->vdispend-1) & 0x200) >> 3;
	ver_overflow|=((CurMode->vdispend-1) & 0x400) >> 9;
	
	Bitu vblank_trim;
	if (IS_VGA_ARCH) {
		switch (CurMode->vdispend) {
		case 400: vblank_trim=6;
				break;
		case 480: vblank_trim=7;
				break;
		case 350: vblank_trim=5;
				break;
		default: vblank_trim=8;
		}
	} else {
		switch (CurMode->vdispend) {
		case 350: vblank_trim=0;
				break;
		default: vblank_trim=23;
		}
	}

	/* Vertical Blank Start */
	IO_Write(crtc_base,0x15);IO_Write(crtc_base+1,(Bit8u)(CurMode->vdispend+vblank_trim));
	overflow|=((CurMode->vdispend+vblank_trim) & 0x100) >> 5;
	max_scanline|=((CurMode->vdispend+vblank_trim) & 0x200) >> 4;
	ver_overflow|=((CurMode->vdispend+vblank_trim) & 0x400) >> 8;

	/* Vertical Blank End */
	IO_Write(crtc_base,0x16);IO_Write(crtc_base+1,(Bit8u)(CurMode->vtotal-vblank_trim-2));

	/* Line Compare */
	Bitu line_compare=(CurMode->vtotal < 1024) ? 1023 : 2047;
	IO_Write(crtc_base,0x18);IO_Write(crtc_base+1,line_compare&0xff);
	overflow|=(line_compare & 0x100) >> 4;
	max_scanline|=(line_compare & 0x200) >> 3;
	ver_overflow|=(line_compare & 0x400) >> 4;
	Bit8u underline=0;
	/* Maximum scanline / Underline Location */
	if (CurMode->special & _EGA_LINE_DOUBLE) {
		if (machine!=MCH_EGA) max_scanline|=0x80;
	}
	switch (CurMode->type) {
	case M_TEXT:
		max_scanline|=CurMode->cheight-1;
		underline=mono_mode ? CurMode->cheight-1 : 0x1f; // mode 7 uses underline position
		break;
	case M_VGA:
		underline=0x40;
		max_scanline|=1;		//Vga doesn't use double line but this
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		underline=0x60;			//Seems to enable the every 4th clock on my s3
		break;
	case M_CGA2:
	case M_CGA4:
		max_scanline|=1;
		break;
	default:
		if (CurMode->vdispend==350) underline=0x0f;
		break;
	}

	IO_Write(crtc_base,0x09);IO_Write(crtc_base+1,max_scanline);
	IO_Write(crtc_base,0x14);IO_Write(crtc_base+1,underline);

	/* OverFlow */
	IO_Write(crtc_base,0x07);IO_Write(crtc_base+1,overflow);

	if (svgaCard == SVGA_S3Trio) {
		/* Extended Horizontal Overflow */
		IO_Write(crtc_base,0x5d);IO_Write(crtc_base+1,hor_overflow);
		/* Extended Vertical Overflow */
		IO_Write(crtc_base,0x5e);IO_Write(crtc_base+1,ver_overflow);
	}

	/* Offset Register */
	Bitu offset;
	switch (CurMode->type) {
	case M_LIN8:
		offset = CurMode->swidth/8;
		break;
	case M_LIN15:
	case M_LIN16:
		offset = 2 * CurMode->swidth/8;
		break;
	case M_LIN32:
		offset = 4 * CurMode->swidth/8;
		break;
	default:
		offset = CurMode->hdispend/2;
	}
	IO_Write(crtc_base,0x13);
	IO_Write(crtc_base + 1,offset & 0xff);

	if (svgaCard == SVGA_S3Trio) {
		/* Extended System Control 2 Register  */
		/* This register actually has more bits but only use the extended offset ones */
		IO_Write(crtc_base,0x51);
		IO_Write(crtc_base + 1,(Bit8u)((offset & 0x300) >> 4));
		/* Clear remaining bits of the display start */
		IO_Write(crtc_base,0x69);
		IO_Write(crtc_base + 1,0);
		/* Extended Vertical Overflow */
		IO_Write(crtc_base,0x5e);IO_Write(crtc_base+1,ver_overflow);
	}

	/* Mode Control */
	Bit8u mode_control=0;

	switch (CurMode->type) {
	case M_CGA2:
		mode_control=0xc2; // 0x06 sets address wrap.
		break;
	case M_CGA4:
		mode_control=0xa2;
		break;
	case M_LIN4:
	case M_EGA:
		if (CurMode->mode==0x11) // 0x11 also sets address wrap.  thought maybe all 2 color modes did but 0x0f doesn't.
			mode_control=0xc3; // so.. 0x11 or 0x0f a one off?
		else {
			if (machine==MCH_EGA) {
				if (CurMode->special & _EGA_LINE_DOUBLE) mode_control=0xc3;
				else mode_control=0x8b;
			} else {
				mode_control=0xe3;
			}
		}
		break;
	case M_TEXT:
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		mode_control=0xa3;
		if (CurMode->special & _VGA_PIXEL_DOUBLE)
			mode_control |= 0x08;
		break;
	}

	IO_Write(crtc_base,0x17);IO_Write(crtc_base+1,mode_control);
	/* Renable write protection */
	IO_Write(crtc_base,0x11);
	IO_Write(crtc_base+1,IO_Read(crtc_base+1)|0x80);

	if (svgaCard == SVGA_S3Trio) {
		/* Setup the correct clock */
		if (CurMode->mode>=0x100) {
			if (CurMode->vdispend>480)
				misc_output|=0xc0;	//480-line sync
			misc_output|=0x0c;		//Select clock 3 
			Bitu clock=CurMode->vtotal*8*CurMode->htotal*70;
			VGA_SetClock(3,clock/1000);
		}
		Bit8u misc_control_2;
		/* Setup Pixel format */
		switch (CurMode->type) {
		case M_LIN8:
			misc_control_2=0x00;
			break;
		case M_LIN15:
			misc_control_2=0x30;
			break;
		case M_LIN16:
			misc_control_2=0x50;
			break;
		case M_LIN32:
			misc_control_2=0xd0;
			break;
		default:
			misc_control_2=0x0;
			break;
		}
		IO_WriteB(crtc_base,0x67);IO_WriteB(crtc_base+1,misc_control_2);
	}

	/* Write Misc Output */
	IO_Write(0x3c2,misc_output);
	/* Program Graphics controller */
	Bit8u gfx_data[GFX_REGS];
	memset(gfx_data,0,GFX_REGS);
	gfx_data[0x7]=0xf;				/* Color don't care */
	gfx_data[0x8]=0xff;				/* BitMask */
	switch (CurMode->type) {
	case M_TEXT:
		gfx_data[0x5]|=0x10;		//Odd-Even Mode
		gfx_data[0x6]|=mono_mode ? 0x0a : 0x0e;		//Either b800 or b000
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
	case M_VGA:
		gfx_data[0x5]|=0x40;		//256 color mode
		gfx_data[0x6]|=0x05;		//graphics mode at 0xa000-affff
		break;
	case M_LIN4:
	case M_EGA:
		if (CurMode->mode == 0x0f)
			gfx_data[0x7]=0x05;		// only planes 0 and 2 are used
		gfx_data[0x6]|=0x05;		//graphics mode at 0xa000-affff
		break;
	case M_CGA4:
		gfx_data[0x5]|=0x20;		//CGA mode
		gfx_data[0x6]|=0x0f;		//graphics mode at at 0xb800=0xbfff
		if (machine==MCH_EGA) gfx_data[0x5]|=0x10;
		break;
	case M_CGA2:
		if (machine==MCH_EGA) {
			gfx_data[0x6]|=0x0d;		//graphics mode at at 0xb800=0xbfff
		} else {
			gfx_data[0x6]|=0x0f;		//graphics mode at at 0xb800=0xbfff
		}
		break;
	}
	for (Bit8u ct=0;ct<GFX_REGS;ct++) {
		IO_Write(0x3ce,ct);
		IO_Write(0x3cf,gfx_data[ct]);
	}
	Bit8u att_data[ATT_REGS];
	memset(att_data,0,ATT_REGS);
	att_data[0x12]=0xf;				//Always have all color planes enabled
	/* Program Attribute Controller */
	switch (CurMode->type) {
	case M_EGA:
	case M_LIN4:
		att_data[0x10]=0x01;		//Color Graphics
		switch (CurMode->mode) {
		case 0x0f:
			att_data[0x12]=0x05;	// planes 0 and 2 enabled
			att_data[0x10]|=0x0a;	// monochrome and blinking
	
			att_data[0x01]=0x08; // low-intensity
			att_data[0x04]=0x18; // blink-on case
			att_data[0x05]=0x18; // high-intensity
			att_data[0x09]=0x08; // low-intensity in blink-off case
			att_data[0x0d]=0x18; // high-intensity in blink-off
			break;
		case 0x11:
			for (i=1;i<16;i++) att_data[i]=0x3f;
			break;
		case 0x10:
		case 0x12: 
			goto att_text16;
		default:
			if ( CurMode->type == M_LIN4 )
				goto att_text16;
			for (Bit8u ct=0;ct<8;ct++) {
				att_data[ct]=ct;
				att_data[ct+8]=ct+0x10;
			}
			break;
		}
		break;
	case M_TANDY16:
		att_data[0x10]=0x01;		//Color Graphics
		for (Bit8u ct=0;ct<16;ct++) att_data[ct]=ct;
		break;
	case M_TEXT:
		if (CurMode->cwidth==9) {
			att_data[0x13]=0x08;	//Pel panning on 8, although we don't have 9 dot text mode
			att_data[0x10]=0x0C;	//Color Text with blinking, 9 Bit characters
		} else {
			att_data[0x13]=0x00;
			att_data[0x10]=0x08;	//Color Text with blinking, 8 Bit characters
		}
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,0x30);
att_text16:
		if (CurMode->mode==7) {
			att_data[0]=0x00;
			att_data[8]=0x10;
			for (i=1; i<8; i++) {
				att_data[i]=0x08;
				att_data[i+8]=0x18;
			}
		} else {
			for (Bit8u ct=0;ct<8;ct++) {
				att_data[ct]=ct;
				att_data[ct+8]=ct+0x38;
			}
			att_data[0x06]=0x14;	//Odd Color 6 yellow/brown.
		}
		break;
	case M_CGA2:
		att_data[0x10]=0x01;		//Color Graphics
		att_data[0]=0x0;
		for (i=1;i<0x10;i++) att_data[i]=0x17;
		att_data[0x12]=0x1;			//Only enable 1 plane
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,0x3f);
		break;
	case M_CGA4:
		att_data[0x10]=0x01;		//Color Graphics
		att_data[0]=0x0;
		att_data[1]=0x13;
		att_data[2]=0x15;
		att_data[3]=0x17;
		att_data[4]=0x02;
		att_data[5]=0x04;
		att_data[6]=0x06;
		att_data[7]=0x07;
		for (Bit8u ct=0x8;ct<0x10;ct++) 
			att_data[ct] = ct + 0x8;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,0x30);
		break;
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		for (Bit8u ct=0;ct<16;ct++) att_data[ct]=ct;
		att_data[0x10]=0x41;		//Color Graphics 8-bit
		break;
	}
	IO_Read(mono_mode ? 0x3ba : 0x3da);
	if ((modeset_ctl & 8)==0) {
		for (Bit8u ct=0;ct<ATT_REGS;ct++) {
			IO_Write(0x3c0,ct);
			IO_Write(0x3c0,att_data[ct]);
		}
		vga.config.pel_panning = 0;
		IO_Write(0x3c0,0x20); IO_Write(0x3c0,0x00); //Disable palette access
		IO_Write(0x3c6,0xff); //Reset Pelmask
		/* Setup the DAC */
		IO_Write(0x3c8,0);
		switch (CurMode->type) {
		case M_EGA:
			if (CurMode->mode>0xf) {
				goto dac_text16;
			} else if (CurMode->mode==0xf) {
				for (i=0;i<64;i++) {
					IO_Write(0x3c9,mtext_s3_palette[i][0]);
					IO_Write(0x3c9,mtext_s3_palette[i][1]);
					IO_Write(0x3c9,mtext_s3_palette[i][2]);
				}
			} else {
				for (i=0;i<64;i++) {
					IO_Write(0x3c9,ega_palette[i][0]);
					IO_Write(0x3c9,ega_palette[i][1]);
					IO_Write(0x3c9,ega_palette[i][2]);
				}
			}
			break;
		case M_CGA2:
		case M_CGA4:
		case M_TANDY16:
			for (i=0;i<64;i++) {
				IO_Write(0x3c9,cga_palette_2[i][0]);
				IO_Write(0x3c9,cga_palette_2[i][1]);
				IO_Write(0x3c9,cga_palette_2[i][2]);
			}
			break;
		case M_TEXT:
			if (CurMode->mode==7) {
				if ((IS_VGA_ARCH) && (svgaCard == SVGA_S3Trio)) {
					for (i=0;i<64;i++) {
						IO_Write(0x3c9,mtext_s3_palette[i][0]);
						IO_Write(0x3c9,mtext_s3_palette[i][1]);
						IO_Write(0x3c9,mtext_s3_palette[i][2]);
					}
				} else {
					for (i=0;i<64;i++) {
						IO_Write(0x3c9,mtext_palette[i][0]);
						IO_Write(0x3c9,mtext_palette[i][1]);
						IO_Write(0x3c9,mtext_palette[i][2]);
					}
				}
				break;
			} //FALLTHROUGH!!!!
		case M_LIN4: //Added for CAD Software
dac_text16:
			for (i=0;i<64;i++) {
				IO_Write(0x3c9,text_palette[i][0]);
				IO_Write(0x3c9,text_palette[i][1]);
				IO_Write(0x3c9,text_palette[i][2]);
			}
			break;
		case M_VGA:
		case M_LIN8:
		case M_LIN15:
		case M_LIN16:
		case M_LIN32:
			// IBM and clones use 248 default colors in the palette for 256-color mode.
			// The last 8 colors of the palette are only initialized to 0 at BIOS init.
			// Palette index is left at 0xf8 as on most clones, IBM leaves it at 0x10.
			for (i=0;i<248;i++) {
				IO_Write(0x3c9,vga_palette[i][0]);
				IO_Write(0x3c9,vga_palette[i][1]);
				IO_Write(0x3c9,vga_palette[i][2]);
			}
			break;
		}
		if (IS_VGA_ARCH) {
			/* check if gray scale summing is enabled */
			if (real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 2) {
				INT10_PerformGrayScaleSumming(0,256);
			}
		}
	} else {
		for (Bit8u ct=0x10;ct<ATT_REGS;ct++) {
			if (ct==0x11) continue;	// skip overscan register
			IO_Write(0x3c0,ct);
			IO_Write(0x3c0,att_data[ct]);
		}
		vga.config.pel_panning = 0;
		IO_Write(0x3c0,0x20); //Disable palette access
	}
	/* Write palette register data to dynamic save area if pointer is non-zero */
	RealPt vsavept=real_readd(BIOSMEM_SEG,BIOSMEM_VS_POINTER);
	RealPt dsapt=real_readd(RealSeg(vsavept),RealOff(vsavept)+4);
	if (dsapt) {
		for (Bit8u ct=0;ct<0x10;ct++) {
			real_writeb(RealSeg(dsapt),RealOff(dsapt)+ct,att_data[ct]);
		}
		real_writeb(RealSeg(dsapt),RealOff(dsapt)+0x10,0); // overscan
	}
	/* Setup some special stuff for different modes */
	switch (CurMode->type) {
	case M_CGA2:
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x1e);
		break;
	case M_CGA4:
		if (CurMode->mode==4) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2a);
		else if (CurMode->mode==5) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2e);
		else real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2);
		break;
	case M_TEXT:
		switch (CurMode->mode) {
		case 0:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2c);break;
		case 1:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x28);break;
		case 2:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2d);break;
		case 3:
		case 7:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x29);break;
		}
		break;
	}

	if (svgaCard == SVGA_S3Trio) {
		/* Setup the CPU Window */
		IO_Write(crtc_base,0x6a);
		IO_Write(crtc_base+1,0);
		/* Setup the linear frame buffer */
		IO_Write(crtc_base,0x59);
		IO_Write(crtc_base+1,(Bit8u)((S3_LFB_BASE >> 24)&0xff));
		IO_Write(crtc_base,0x5a);
		IO_Write(crtc_base+1,(Bit8u)((S3_LFB_BASE >> 16)&0xff));
		IO_Write(crtc_base,0x6b); // BIOS scratchpad
		IO_Write(crtc_base+1,(Bit8u)((S3_LFB_BASE >> 24)&0xff));
		
		/* Setup some remaining S3 registers */
		IO_Write(crtc_base,0x41); // BIOS scratchpad
		IO_Write(crtc_base+1,0x88);
		IO_Write(crtc_base,0x52); // extended BIOS scratchpad
		IO_Write(crtc_base+1,0x80);

		IO_Write(0x3c4,0x15);
		IO_Write(0x3c5,0x03);

		// Accellerator setup 
		Bitu reg_50=S3_XGA_8BPP;
		switch (CurMode->type) {
			case M_LIN15:
			case M_LIN16: reg_50|=S3_XGA_16BPP; break;
			case M_LIN32: reg_50|=S3_XGA_32BPP; break;
			default: break;
		}
		switch(CurMode->swidth) {
			case 640:  reg_50|=S3_XGA_640; break;
			case 800:  reg_50|=S3_XGA_800; break;
			case 1024: reg_50|=S3_XGA_1024; break;
			case 1152: reg_50|=S3_XGA_1152; break;
			case 1280: reg_50|=S3_XGA_1280; break;
			default: break;
		}
		IO_WriteB(crtc_base,0x50); IO_WriteB(crtc_base+1,reg_50);

		Bit8u reg_31, reg_3a;
		switch (CurMode->type) {
			case M_LIN15:
			case M_LIN16:
			case M_LIN32:
				reg_3a=0x15;
				break;
			case M_LIN8:
				// S3VBE20 does it this way. The other double pixel bit does not
				// seem to have an effect on the Trio64.
				if(CurMode->special&_VGA_PIXEL_DOUBLE) reg_3a=0x5;
				else reg_3a=0x15;
				break;
			default:
				reg_3a=5;
				break;
		};
		
		switch (CurMode->type) {
		case M_LIN4: // <- Theres a discrepance with real hardware on this
		case M_LIN8:
		case M_LIN15:
		case M_LIN16:
		case M_LIN32:
			reg_31 = 9;
			break;
		default:
			reg_31 = 5;
			break;
		}
		IO_Write(crtc_base,0x3a);IO_Write(crtc_base+1,reg_3a);
		IO_Write(crtc_base,0x31);IO_Write(crtc_base+1,reg_31);	//Enable banked memory and 256k+ access
		IO_Write(crtc_base,0x58);IO_Write(crtc_base+1,0x3);		//Enable 8 mb of linear addressing

		IO_Write(crtc_base,0x38);IO_Write(crtc_base+1,0x48);	//Register lock 1
		IO_Write(crtc_base,0x39);IO_Write(crtc_base+1,0xa5);	//Register lock 2
	} else if (svga.set_video_mode) {
		VGA_ModeExtraData modeData;
		modeData.ver_overflow = ver_overflow;
		modeData.hor_overflow = hor_overflow;
		modeData.offset = offset;
		modeData.modeNo = CurMode->mode;
		modeData.htotal = CurMode->htotal;
		modeData.vtotal = CurMode->vtotal;
		svga.set_video_mode(crtc_base, &modeData);
	}

	FinishSetMode(clearmem);

	/* Set vga attrib register into defined state */
	IO_Read(mono_mode ? 0x3ba : 0x3da);
	IO_Write(0x3c0,0x20);
	IO_Read(mono_mode ? 0x3ba : 0x3da); // Kukoo2 demo 

	/* Load text mode font */
	if (CurMode->type==M_TEXT) {
		INT10_ReloadFont();
	}
	return true;
}

Bitu VideoModeMemSize(Bitu mode) {
	if (!IS_VGA_ARCH)
		return 0;

	VideoModeBlock* modelist = NULL;

	switch (svgaCard) {
	case SVGA_TsengET4K:
	case SVGA_TsengET3K:
		modelist = ModeList_VGA_Tseng;
		break;
	case SVGA_ParadisePVGA1A:
		modelist = ModeList_VGA_Paradise;
		break;
	default:
		modelist = ModeList_VGA;
		break;
	}

	VideoModeBlock* vmodeBlock = NULL;
	Bitu i=0;
	while (modelist[i].mode!=0xffff) {
		if (modelist[i].mode==mode) {
			vmodeBlock = &modelist[i];
			break;
		}
		i++;
	}
	if (!vmodeBlock)
        return 0;

	switch(vmodeBlock->type) {
	case M_LIN4:
		return vmodeBlock->swidth*vmodeBlock->sheight/2;
	case M_LIN8:
		return vmodeBlock->swidth*vmodeBlock->sheight;
	case M_LIN15: case M_LIN16:
		return vmodeBlock->swidth*vmodeBlock->sheight*2;
	case M_LIN32:
		return vmodeBlock->swidth*vmodeBlock->sheight*4;
	case M_TEXT:
		return vmodeBlock->twidth*vmodeBlock->theight*2;
	}
	// Return 0 for all other types, those always fit in memory
	return 0;
}
