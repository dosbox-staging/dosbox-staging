/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <string.h>

#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "mouse.h"
#include "vga.h"
#include "bios.h"

#define SEQ_REGS 0x05
#define GFX_REGS 0x09
#define ATT_REGS 0x15

extern bool allow_vesa_lowres_modes;
extern bool vesa12_modes_32bpp;
extern bool allow_vesa_32bpp;
extern bool allow_vesa_24bpp;
extern bool allow_vesa_16bpp;
extern bool allow_vesa_15bpp;
extern bool allow_vesa_8bpp;
extern bool allow_vesa_4bpp;
extern bool allow_vesa_tty;

VideoModeBlock ModeList_VGA[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN | _REPEAT1},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},/*was EGA_2*/
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},/*was EGA_2 */
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,_REPEAT1   },

{ 0x054  ,M_TEXT   ,1056,344, 132,43, 8,  8, 1 ,0xB8000 ,0x4000, 160, 449, 132,344, 0   },
{ 0x055  ,M_TEXT   ,1056,400, 132,25, 8, 16, 1 ,0xB8000 ,0x2000, 160, 449, 132,400, 0   },

/* Alias of mode 101 */
{ 0x069  ,M_LIN8   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0	},
/* Alias of mode 102 */
{ 0x06A  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,128 ,663 ,100,600 ,0	},

/* Follow vesa 1.2 for first 0x20 */
{ 0x100  ,M_LIN8   ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 ,0   },
{ 0x101  ,M_LIN8   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , _VGA_PIXEL_DOUBLE },
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

/* VESA higher color modes.
 * Note v1.2 of the VESA BIOS extensions explicitly states modes 0x10F, 0x112, 0x115, 0x118 are 8:8:8 (24-bit) not 8:8:8:8 (32-bit).
 * This also fixes COMA "Parhaat" 1997 demo, by offering a true 24bpp mode so that it doesn't try to draw 24bpp on a 32bpp VESA linear framebuffer.
 * NTS: The 24bpp modes listed here will not be available to the DOS game/demo if the user says that the VBE 1.2 modes are 32bpp,
 *      instead the redefinitions in the next block will apply to allow M_LIN32. To use the 24bpp modes here, you must set 'vesa vbe 1.2 modes are 32bpp=false' */
{ 0x10D  ,M_LIN15  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _DOUBLESCAN },
{ 0x10E  ,M_LIN16  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _DOUBLESCAN },
{ 0x10F  ,M_LIN24  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,50  ,449 ,40 ,400 , _DOUBLESCAN },
{ 0x110  ,M_LIN15  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,525 ,160,480 ,0   },
{ 0x111  ,M_LIN16  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,525 ,160,480 ,0   },
{ 0x112  ,M_LIN24  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0   },
{ 0x113  ,M_LIN15  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,628 ,200,600 ,0   },
{ 0x114  ,M_LIN16  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,628 ,200,600 ,0   },
{ 0x115  ,M_LIN24  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,628 ,100,600 ,0   },
{ 0x116  ,M_LIN15  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,336 ,806 ,256,768 ,0	},
{ 0x117  ,M_LIN16  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,336 ,806 ,256,768 ,0	},
{ 0x118  ,M_LIN24  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,806 ,128,768 ,0	},

/* But of course... there are other demos that assume mode 0x10F is 32bpp!
 * So we have another definition of those modes that overlaps some of the same mode numbers above.
 * This allows "Phenomena" demo to use 32bpp 320x200 mode if you set 'vesa vbe 1.2 modes are 32bpp=true'.
 * The code will allow either this block's mode 0x10F (LIN32), or the previous block's mode 0x10F (LIN24), but not both. */
{ 0x10F  ,M_LIN32  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,50  ,449 ,40 ,400 , _DOUBLESCAN },
{ 0x112  ,M_LIN32  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0   },
{ 0x115  ,M_LIN32  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,628 ,100,600 ,0   },
{ 0x118  ,M_LIN32  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,806 ,128,768 ,0	},

/* RGBX 8:8:8:8 modes. These were once the M_LIN32 modes DOSBox mapped to 0x10F-0x11B prior to implementing M_LIN24. */
{ 0x210  ,M_LIN32  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,50  ,449 ,40 ,400 , _DOUBLESCAN },
{ 0x211  ,M_LIN32  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 ,0   },
{ 0x212  ,M_LIN32  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,628 ,100,600 ,0   },
{ 0x213  ,M_LIN32  ,1024,768 ,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,806 ,128,768 ,0	},

/* those should be interlaced but ok */
{ 0x119  ,M_LIN15  ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,424 ,1066,320,1024,0	},
{ 0x11A  ,M_LIN16  ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,424 ,1066,320,1024,0	},

{ 0x11C  ,M_LIN8   ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x10000,100 ,449 ,80 ,350 ,0	},
// special mode for Birth demo by Incognita
{ 0x11D  ,M_LIN15  ,640 ,350 ,80 ,25 ,8 ,14 ,1 ,0xA0000 ,0x10000,200 ,449 ,160,350 ,0   },
{ 0x11F  ,M_LIN16  ,640 ,350 ,80 ,25 ,8 ,14 ,1 ,0xA0000 ,0x10000,200 ,449 ,160,350 ,0   },
{ 0x120  ,M_LIN8   ,1600,1200,200,75 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,1240,200,1200,0	},
{ 0x142  ,M_LIN32  ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x10000 ,100 ,449 ,80 ,350 ,0	},

{ 0x150  ,M_LIN8   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _S3_PIXEL_DOUBLE | _DOUBLESCAN },
{ 0x151  ,M_LIN8   ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , _S3_PIXEL_DOUBLE | _DOUBLESCAN },
{ 0x152  ,M_LIN8   ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _S3_PIXEL_DOUBLE  },
{ 0x153  ,M_LIN8   ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , _S3_PIXEL_DOUBLE  },

{ 0x160  ,M_LIN15  ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 , _DOUBLESCAN },
{ 0x161  ,M_LIN15  ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 , 80 ,400 ,0 },
{ 0x162  ,M_LIN15  ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 ,0 },
{ 0x165  ,M_LIN15  ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,449 ,160 ,400 ,0   },

// hack: 320x480x256-color alias for Habitual demo. doing this removes the need to run S3VBE20.EXE before running the demo.
//       the reason it has to be this particular video mode is because HABITUAL.EXE does not query modes, it simply assumes
//       that mode 0x166 is this particular mode and errors out if it can't set it.
{ 0x166  ,M_LIN8   ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , _S3_PIXEL_DOUBLE  },

{ 0x170  ,M_LIN16  ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 , _DOUBLESCAN },
{ 0x171  ,M_LIN16  ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 , 80 ,400 ,0 },
{ 0x172  ,M_LIN16  ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,525 , 80 ,480 ,0 },
{ 0x175  ,M_LIN16  ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,449 ,160 ,400 ,0   },

{ 0x190  ,M_LIN32  ,320 ,240 ,40 ,30 ,8 ,8  ,1 ,0xA0000 ,0x10000, 50 ,525 ,40 ,480 , _DOUBLESCAN },
{ 0x191  ,M_LIN32  ,320 ,400 ,40 ,50 ,8 ,8  ,1 ,0xA0000 ,0x10000, 50 ,449 ,40 ,400 ,0 },
{ 0x192  ,M_LIN32  ,320 ,480 ,40 ,60 ,8 ,8  ,1 ,0xA0000 ,0x10000, 50 ,525 ,40 ,480 ,0 },

// S3 specific modes
{ 0x207  ,M_LIN8	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,182 ,948 ,144,864 ,0	},
{ 0x209  ,M_LIN15	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,364 ,948 ,288,864 ,0	},
{ 0x20A  ,M_LIN16	,1152,864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,364 ,948 ,288,864 ,0	},
{ 0x20B  ,M_LIN32	,1152, 864,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,182 ,948 ,144,864 ,0	},
{ 0x213  ,M_LIN32   ,640 ,400,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 ,0	},

// Some custom modes

// 720x480 3:2 modes
{ 0x21B  ,M_LIN4   ,720 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0	},
{ 0x21C  ,M_LIN8   ,720 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0	},
{ 0x21D  ,M_LIN15  ,720 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,525 ,212 ,480 ,0  },
{ 0x21E  ,M_LIN16  ,720 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,525 ,212 ,480 ,0  },
{ 0x21F  ,M_LIN32  ,720 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0  },

// 848x480 16:9 modes
{ 0x220  ,M_LIN4   ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0	},
{ 0x221  ,M_LIN8   ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0	},
{ 0x222  ,M_LIN15  ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,525 ,212 ,480 ,0  },
{ 0x223  ,M_LIN16  ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,525 ,212 ,480 ,0  },
{ 0x224  ,M_LIN32  ,848 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,132 ,525 ,106 ,480 ,0  },

// 1280x800 8:5 modes
{ 0x225  ,M_LIN4   ,1280,800 ,160,50 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,880 ,160 ,800 ,0	},
{ 0x226  ,M_LIN8   ,1280,800 ,160,50 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,880 ,160 ,800 ,0	},
{ 0x227  ,M_LIN15  ,1280,800 ,160,50 ,8 ,16 ,1 ,0xA0000 ,0x10000,400 ,880 ,320 ,800 ,0  },
{ 0x228  ,M_LIN16  ,1280,800 ,160,50 ,8 ,16 ,1 ,0xA0000 ,0x10000,400 ,880 ,320 ,800 ,0  },
{ 0x229  ,M_LIN32  ,1280,800 ,160,50 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,880 ,160 ,800 ,0  },

// 1280x960 4:3 modes
{ 0x22a  ,M_LIN4   ,1280,960 ,160,60 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,1020,160 ,960 ,0	},
{ 0x22b  ,M_LIN8   ,1280,960 ,160,60 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,1020,160 ,960 ,0	},
{ 0x22c  ,M_LIN15  ,1280,960 ,160,60 ,8 ,16 ,1 ,0xA0000 ,0x10000,400 ,1020,320 ,960 ,0  },
{ 0x22d  ,M_LIN16  ,1280,960 ,160,60 ,8 ,16 ,1 ,0xA0000 ,0x10000,400 ,1020,320 ,960 ,0  },
{ 0x22e  ,M_LIN32  ,1280,960 ,160,60 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,1020,160 ,960 ,0  },

// 1280x1024 5:4 rest
{ 0x22f  ,M_LIN32  ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0x10000,212 ,1066,160,1024,0	},

// 1400x1050 4:3 - 4bpp is no good xD
{ 0x22b  ,M_LIN4   ,1400,1050,175,66 ,8 ,16 ,1 ,0xA0000 ,0x10000,220 ,1100,175 ,1050,0	},
{ 0x230  ,M_LIN8   ,1400,1050,175,66 ,8 ,16 ,1 ,0xA0000 ,0x10000,220 ,1100,175 ,1050,0	},
{ 0x231  ,M_LIN15  ,1400,1050,175,66 ,8 ,16 ,1 ,0xA0000 ,0x10000,440 ,1100,350 ,1050,0  },
{ 0x232  ,M_LIN16  ,1400,1050,175,66 ,8 ,16 ,1 ,0xA0000 ,0x10000,440 ,1100,350 ,1050,0  },
{ 0x233  ,M_LIN32  ,1400,1050,175,66 ,8 ,16 ,1 ,0xA0000 ,0x10000,220 ,1100,175 ,1050,0  },

// 1440x900 8:5 modes
{ 0x234  ,M_LIN4   ,1440, 900,180,56 ,8 ,16 ,1 ,0xA0000 ,0x10000,220 , 980,180 , 900,0  },
{ 0x235  ,M_LIN8   ,1440, 900,180,56 ,8 ,16 ,1 ,0xA0000 ,0x10000,220 , 980,180 , 900,0  },
{ 0x236  ,M_LIN15  ,1440, 900,180,56 ,8 ,16 ,1 ,0xA0000 ,0x10000,440 , 980,360 , 900,0  },
{ 0x237  ,M_LIN16  ,1440, 900,180,56 ,8 ,16 ,1 ,0xA0000 ,0x10000,440 , 980,360 , 900,0  },
{ 0x238  ,M_LIN32  ,1440, 900,180,56 ,8 ,16 ,1 ,0xA0000 ,0x10000,220 , 980,180 , 900,0  },

// 1600x1200 4:3 rest - 32bpp needs more than 4 megs
{ 0x239  ,M_LIN4   ,1600,1200,200,75 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,1240,200, 1200,0	},
{ 0x23a  ,M_LIN15  ,1600,1200,200,75 ,8 ,16 ,1 ,0xA0000 ,0x10000,500 ,1240,400 ,1200,0	},
{ 0x23b  ,M_LIN16  ,1600,1200,200,75 ,8 ,16 ,1 ,0xA0000 ,0x10000,500 ,1240,400 ,1200,0	},
{ 0x23c  ,M_LIN32  ,1600,1200,200,75 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,1240,200 ,1200,0	},

// 1280x720 16:9 modes
{ 0x23D  ,M_LIN4   ,1280,720 ,160,45 ,8 ,16 ,1 ,0xA0000 ,0x10000,176 ,792 ,160 ,720 ,0	},
{ 0x23E  ,M_LIN8   ,1280,720 ,160,45 ,8 ,16 ,1 ,0xA0000 ,0x10000,176 ,792 ,160 ,720 ,0	},
{ 0x23F  ,M_LIN15  ,1280,720 ,160,45 ,8 ,16 ,1 ,0xA0000 ,0x10000,352 ,792 ,320 ,720 ,0  },
{ 0x240  ,M_LIN16  ,1280,720 ,160,45 ,8 ,16 ,1 ,0xA0000 ,0x10000,352 ,792 ,320 ,720 ,0  },
{ 0x241  ,M_LIN32  ,1280,720 ,160,45 ,8 ,16 ,1 ,0xA0000 ,0x10000,176 ,792 ,160 ,720 ,0  },

// 1920x1080 16:9 modes
{ 0x242  ,M_LIN4   ,1920,1080,240,67 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,1188,240 ,1080,0	},
{ 0x243  ,M_LIN8   ,1920,1080,240,67 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,1188,240 ,1080,0	},
{ 0x244  ,M_LIN15  ,1920,1080,240,67 ,8 ,16 ,1 ,0xA0000 ,0x10000,528 ,1188,480 ,1080,0  },
{ 0x245  ,M_LIN16  ,1920,1080,240,67 ,8 ,16 ,1 ,0xA0000 ,0x10000,528 ,1188,480 ,1080,0  },
{ 0x246  ,M_LIN32  ,1920,1080,240,67 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,1188,240 ,1080,0  },

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock ModeList_VGA_Text_200lines[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,320 ,200 ,40 ,25 ,8 , 8 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK | _DOUBLESCAN},
{ 0x001  ,M_TEXT   ,320 ,200 ,40 ,25 ,8 , 8 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK | _DOUBLESCAN},
{ 0x002  ,M_TEXT   ,640 ,200 ,80 ,25 ,8 , 8 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN },
{ 0x003  ,M_TEXT   ,640 ,200 ,80 ,25 ,8 , 8 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN }
};

VideoModeBlock ModeList_VGA_Text_350lines[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,320 ,350 ,40 ,25 ,8 ,14 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,350 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,320 ,350 ,40 ,25 ,8 ,14 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,350 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,350 ,0	},
{ 0x003  ,M_TEXT   ,640 ,350 ,80 ,25 ,8 ,14 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,350 ,0	}
};

VideoModeBlock ModeList_VGA_Tseng[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN | _REPEAT1},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,_REPEAT1   },

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
{ 0x038  ,M_LIN8   ,1024 ,768,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,168 ,800 ,128,768 , 0 },/* ET4000 only */
{ 0x03D  ,M_LIN4   ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0xA000, 160 ,1152,160,1024, 0 },/* newer ET4000 */
{ 0x03E  ,M_LIN4   ,1280, 960,160,60 ,8 ,16 ,1 ,0xA0000 ,0xA000, 160 ,1024,160,960 , 0 },/* Definicon only */ 
{ 0x06A  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,663 ,100,600 , 0 },/* newer ET4000 */

// Sierra SC1148x Hi-Color DAC modes
{ 0x213  ,M_LIN15  ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , _VGA_PIXEL_DOUBLE | _DOUBLESCAN },
{ 0x22D  ,M_LIN15  ,640 ,350 ,80 ,25 ,8 ,14 ,1 ,0xA0000 ,0x10000,200 ,449 ,160,350 , 0 },
{ 0x22E  ,M_LIN15  ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,525 ,160,480 , 0 },
{ 0x22F  ,M_LIN15  ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,200 ,449 ,160,400 , 0 },
{ 0x230  ,M_LIN15  ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,264 ,628 ,200,600 , 0 },

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock ModeList_VGA_Paradise[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN | _REPEAT1},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,_REPEAT1 },

{ 0x054  ,M_TEXT   ,1056 ,688, 132,43, 8, 9, 1, 0xB0000, 0x4000, 192, 720, 132,688, 0 },
{ 0x055  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1, 0xB0000, 0x2000, 192, 449, 132,400, 0 },
{ 0x056  ,M_TEXT   ,1056 ,688, 132,43, 8, 9, 1, 0xB0000, 0x4000, 192, 720, 132,688, 0 },
{ 0x057  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1, 0xB0000, 0x2000, 192, 449, 132,400, 0 },
{ 0x058  ,M_LIN4   ,800 , 600, 100,37, 8, 16,1, 0xA0000, 0xA000, 128 ,663 ,100,600, 0 },
{ 0x05C  ,M_LIN8   ,800 , 600 ,100,37 ,8 ,16,1 ,0xA0000 ,0x10000,128 ,663 ,100,600, 0 },
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
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,60  ,262 ,40 ,200 ,_EGA_HALF_CLOCK	| _REPEAT1},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,60  ,262 ,40 ,200 ,_EGA_HALF_CLOCK	| _REPEAT1},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,120 ,262 ,80 ,200 ,_REPEAT1},
{ 0x007  ,M_TEXT   ,720 ,350 ,80 ,25 ,9 ,14 ,8 ,0xB0000 ,0x1000 ,120 ,440 ,80 ,350 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,60  ,262 ,40 ,200 ,_EGA_HALF_CLOCK	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,120 ,262 ,80 ,200 ,0 },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,96  ,366 ,80 ,350 ,0	},
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,96  ,366 ,80 ,350 ,0	},

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

VideoModeBlock ModeList_OTHER[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde ,special flags */
{ 0x000  ,M_TEXT   ,320 ,400 ,40 ,25 ,8 ,8  ,8 ,0xB8000 ,0x0800 ,56  ,31  ,40 ,25  ,0   },
{ 0x001  ,M_TEXT   ,320 ,400 ,40 ,25 ,8 ,8  ,8 ,0xB8000 ,0x0800 ,56  ,31  ,40 ,25  ,0	},
{ 0x002  ,M_TEXT   ,640 ,400 ,80 ,25 ,8 ,8  ,4 ,0xB8000 ,0x1000 ,113 ,31  ,80 ,25  ,0	},
{ 0x003  ,M_TEXT   ,640 ,400 ,80 ,25 ,8 ,8  ,4 ,0xB8000 ,0x1000 ,113 ,31  ,80 ,25  ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,4 ,0xB8000 ,0x0800 ,56  ,127 ,40 ,100 ,0   },
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,4 ,0xB8000 ,0x0800 ,56  ,127 ,40 ,100 ,0   },
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xB8000 ,0x0800 ,56  ,127 ,40 ,100 ,0   },
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
		if (modeblock[i].mode!=mode)
			i++;
		/* Hack for VBE 1.2 modes and 24/32bpp ambiguity */
		else if (modeblock[i].mode >= 0x100 && modeblock[i].mode <= 0x11F &&
			((modeblock[i].type == M_LIN32 && !vesa12_modes_32bpp) ||
			(modeblock[i].type == M_LIN24 && vesa12_modes_32bpp))) {
			/* ignore */
			i++;
		}
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

#if defined(WIN32) && !(C_DEBUG)
bool DISP2_Active(void);
#endif
bool INT10_SetCurMode(void) {
	bool mode_changed=false;
	Bit16u bios_mode=(Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE);
	if (CurMode->mode!=bios_mode) {
#if defined(WIN32) && !(C_DEBUG)
		if (bios_mode==7 && DISP2_Active()) {
			if ((real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0x30)!=0x30) return false;
			CurMode=&Hercules_Mode;
			return true;
		}
#endif
		switch (machine) {
		case MCH_CGA:
			if (bios_mode<7) mode_changed=SetCurMode(ModeList_OTHER,bios_mode);
			break;
		case TANDY_ARCH_CASE:
			if (bios_mode!=7 && bios_mode<=0xa) mode_changed=SetCurMode(ModeList_OTHER,bios_mode);
			break;
		case MCH_HERC:
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
			default:
				mode_changed=SetCurMode(ModeList_VGA,bios_mode);
				break;
			}
			if (mode_changed && bios_mode<=3) {
				switch (real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL)&0x90) {
				case 0x00:
					CurMode=&ModeList_VGA_Text_350lines[bios_mode];
					break;
				case 0x80:
					CurMode=&ModeList_VGA_Text_200lines[bios_mode];
					break;
				}
			}
			break;
		}
	}
	return mode_changed;
}

static void FinishSetMode(bool clearmem) {
	/* Clear video memory if needs be */
	if (clearmem) {
		switch (CurMode->type) {
		case M_TANDY16:
			if ((machine==MCH_PCJR) && (CurMode->mode >= 9)) {
				// PCJR cannot access the full 32k at 0xb800
				for (Bit16u ct=0;ct<16*1024;ct++) {
					// 0x1800 is the last 32k block in 128k, as set in the CRTCPU_PAGE register 
					real_writew(0x1800,ct*2,0x0000);
				}
				break;
			}
			// fall-through
		case M_CGA4:
		case M_CGA2:
			for (Bit16u ct=0;ct<16*1024;ct++) {
				real_writew( 0xb800,ct*2,0x0000);
			}
			break;
		case M_TEXT: {
			Bit16u max = (CurMode->ptotal*CurMode->plength)>>1;
			if (CurMode->mode == 7) {
				for (Bit16u ct=0;ct<max;ct++) real_writew(0xB000,ct*2,0x0720);
			}
			else {
				for (Bit16u ct=0;ct<max;ct++) real_writew(0xB800,ct*2,0x0720);
			}
			break;
		}
		case M_EGA:	
		case M_VGA:
		case M_LIN8:
		case M_LIN4:
		case M_LIN15:
		case M_LIN16:
		case M_LIN24:
		case M_LIN32:
			/* Hack we just access the memory directly */
			memset(vga.mem.linear,0,vga.vmemsize);
		}
	}
	/* Setup the BIOS */
	if (CurMode->mode<128) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,(Bit8u)CurMode->mode);
	else real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,(Bit8u)(CurMode->mode-0x98));	//Looks like the s3 bios
	real_writew(BIOSMEM_SEG,BIOSMEM_NB_COLS,(Bit16u)CurMode->twidth);
	real_writew(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,(Bit16u)CurMode->plength);
	real_writew(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS,((CurMode->mode==7 )|| (CurMode->mode==0x0f)) ? 0x3b4 : 0x3d4);
	real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,(Bit8u)(CurMode->theight-1));
	real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,(Bit16u)CurMode->cheight);
	real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,(0x60|(clearmem?0:0x80)));
	real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0x09);

	// this is an index into the dcc table:
#if defined(WIN32) && !(C_DEBUG)
	if (IS_VGA_ARCH) real_writeb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX,DISP2_Active()?0x0c:0x0b);
#else
	if (IS_VGA_ARCH) real_writeb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX,0x0b);
#endif
	real_writed(BIOSMEM_SEG,BIOSMEM_VS_POINTER,int10.rom.video_save_pointers);

	// Set cursor shape
	if (CurMode->type==M_TEXT) {
		INT10_SetCursorShape(0x06,07);
	}
	// Set cursor pos for page 0..7
	for (Bit8u ct=0;ct<8;ct++) INT10_SetCursorPos(0,0,ct);
	// Set active page 0
	INT10_SetActivePage(0);
	/* Set some interrupt vectors */
	if (CurMode->mode<=3 || CurMode->mode==7) {
		RealSetVec(0x43,int10.rom.font_8_first);
	} else {
		switch (CurMode->cheight) {
		case 8:RealSetVec(0x43,int10.rom.font_8_first);break;
		case 14:RealSetVec(0x43,int10.rom.font_14);break;
		case 16:RealSetVec(0x43,int10.rom.font_16);break;
		}
	}
	/* Tell mouse resolution change */
	Mouse_NewVideoMode();
}

extern bool en_int33;

bool INT10_SetVideoMode_OTHER(Bit16u mode,bool clearmem) {
	switch (machine) {
	case MCH_CGA:
	case MCH_AMSTRAD:
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
		// Only init the adapter if the equipment word is set to monochrome (Testdrive)
		if ((real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0x30)!=0x30) return false;
		CurMode=&Hercules_Mode;
		mode=7; // in case the video parameter table is modified
		break;
	}
	LOG(LOG_INT10,LOG_NORMAL)("Set Video Mode %X",mode);

	/* Setup the CRTC */
	Bitu crtc_base=machine==MCH_HERC ? 0x3b4 : 0x3d4;
	//Horizontal total
	IO_WriteW(crtc_base,0x00 | (CurMode->htotal) << 8);
	//Horizontal displayed
	IO_WriteW(crtc_base,0x01 | (CurMode->hdispend) << 8);
	//Horizontal sync position
	IO_WriteW(crtc_base,0x02 | (CurMode->hdispend+1) << 8);
	//Horizontal sync width, seems to be fixed to 0xa, for cga at least, hercules has 0xf
	// PCjr doubles sync width in high resolution modes, good for aspect correction
	// newer "compatible" CGA BIOS does the same
	// The IBM CGA card seems to limit retrace pulse widths
	Bitu syncwidth;
	if(machine==MCH_HERC) syncwidth = 0xf;
	else if(CurMode->hdispend==80) syncwidth = 0xc;
	else syncwidth = 0x6;
	
	IO_WriteW(crtc_base,0x03 | (syncwidth) << 8);
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
	case M_TEXT: // text mode character height
		if (machine==MCH_HERC) scanline=14;
		else scanline=8;
		break;
	case M_CGA2: // graphics mode: even/odd banks interleaved
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

		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x29); // attribute controls blinking
		break;
	case MCH_AMSTRAD:
		IO_WriteB( 0x3d9, 0x0f );
	case MCH_CGA:
		mode_control=mode_control_list[CurMode->mode];
		if (CurMode->mode == 0x6) color_select=0x3f;
		else color_select=0x30;
		IO_WriteB(0x3d8,mode_control);
		IO_WriteB(0x3d9,color_select);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		if (mono_cga) Mono_CGA_Palette();
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

	// Check if the program wants us to use a custom mode table
	RealPt vparams = RealGetVec(0x1d);
	if (vparams != 0 && (vparams != BIOS_VIDEO_TABLE_LOCATION) && (mode < 8)) {
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

	if (en_int33) INT10_SetCurMode();

	return true;
}


bool INT10_SetVideoMode(Bit16u mode) {
	//LOG_MSG("set mode %x",mode);
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
#if defined(WIN32) && !(C_DEBUG)
	if (mode==7 && DISP2_Active()) {
		if ((real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0x30)!=0x30) return false;
		CurMode=&Hercules_Mode;
		FinishSetMode(clearmem);
		// EGA/VGA inactive
		if (IS_EGAVGA_ARCH)	real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,(0x68|(clearmem?0:0x80)));
		INT10_SetCursorShape(0x0b,0x0c);
		return true;
	}
#endif
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
		// check for scanline backwards compatibility (VESA text modes??)
		if (CurMode->type==M_TEXT) {
			if ((modeset_ctl&0x90)==0x80) { // 200 lines emulation
				if (CurMode->mode <= 3) {
					CurMode = &ModeList_VGA_Text_200lines[CurMode->mode];
				}
			} else if ((modeset_ctl&0x90)==0x00) { // 350 lines emulation
				if (CurMode->mode <= 3) {
					CurMode = &ModeList_VGA_Text_350lines[CurMode->mode];
				}
			}
		}
	} else {
		if (!SetCurMode(ModeList_EGA,mode)){
			LOG(LOG_INT10,LOG_ERROR)("EGA:Trying to set illegal mode %X",mode);
			return false;
		}
	}

	/* Setup the VGA to the correct mode */
	// turn off video
	IO_Write(0x3c4,0); IO_Write(0x3c5,1); // reset
	IO_Write(0x3c4,1); IO_Write(0x3c5,0x20); // screen off

	Bit16u crtc_base;
	bool mono_mode=(mode == 7) || (mode==0xf);  
	if (mono_mode) crtc_base=0x3b4;
	else crtc_base=0x3d4;

	/* Setup MISC Output Register */
	Bit8u misc_output=0x2 | (mono_mode ? 0x0 : 0x1);

	if ((CurMode->type==M_TEXT) && (CurMode->cwidth==9)) {
		// 28MHz (16MHz EGA) clock for 9-pixel wide chars
		misc_output|=0x4;
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
	case 200:
	default:
		misc_output|=0x20;
	}
	IO_Write(0x3c2,misc_output);		//Setup for 3b4 or 3d4
	
	if (IS_VGA_ARCH && (svgaCard == SVGA_S3Trio)) {
	// unlock the S3 registers
		IO_Write(crtc_base,0x38);IO_Write(crtc_base+1,0x48);	//Register lock 1
		IO_Write(crtc_base,0x39);IO_Write(crtc_base+1,0xa5);	//Register lock 2
		IO_Write(0x3c4,0x8);IO_Write(0x3c5,0x06);
		// Disable MMIO here so we can read / write memory
		IO_Write(crtc_base,0x53);IO_Write(crtc_base+1,0x0);
	}
	
	/* Program Sequencer */
	Bit8u seq_data[SEQ_REGS];
	memset(seq_data,0,SEQ_REGS);
	
	seq_data[0] = 0x3;	// not reset
	seq_data[1] = 0x21;	// screen still disabled, will be enabled at end of setmode
	seq_data[4] = 0x04;	// odd/even disable
	
	if (CurMode->special & _EGA_HALF_CLOCK) seq_data[1]|=0x08; //Check for half clock
	if ((machine==MCH_EGA) && (CurMode->special & _EGA_HALF_CLOCK)) seq_data[1]|=0x02;
	seq_data[4]|=0x02;	//More than 64kb
	switch (CurMode->type) {
	case M_TEXT:
		if (CurMode->cwidth==9) seq_data[1] &= ~1;
		seq_data[2]|=0x3;				//Enable plane 0 and 1
		seq_data[4]|=0x01;				//Alpanumeric
		seq_data[4]&=~0x04;				//odd/even enable
		break;
	case M_CGA2:
		if (IS_EGAVGA_ARCH) {
			seq_data[2]|=0x1;			//Enable plane 0. Most VGA cards treat it as a 640x200 variant of the MCGA 2-color mode, with bit 13 remapped for interlace
		}
		break;
	case M_CGA4:
		if (IS_EGAVGA_ARCH) {
			seq_data[2]|=0x3;			//Enable plane 0 and 1
			seq_data[4]&=~0x04;			//odd/even enable
		}
		break;
	case M_LIN4:
	case M_EGA:
		seq_data[2]|=0xf;				//Enable all planes for writing
		break;
	case M_LIN8:						//Seems to have the same reg layout from testing
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32:
	case M_VGA:
		seq_data[2]|=0xf;				//Enable all planes for writing
		seq_data[4]|=0x8;				//Graphics - Chained
		break;
	}
	for (Bit8u ct=0;ct<SEQ_REGS;ct++) {
		IO_Write(0x3c4,ct);
		IO_Write(0x3c5,seq_data[ct]);
	}
	vga.config.compatible_chain4 = true; // this may be changed by SVGA chipset emulation

	if( machine==MCH_AMSTRAD )
	{
		vga.amstrad.mask_plane = 0x07070707;
		vga.amstrad.write_plane = 0x0F;
		vga.amstrad.read_plane = 0x00;
		vga.amstrad.border_color = 0x00;
	}

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
		else if (CurMode->special & _DOUBLESCAN) ret_end = (CurMode->htotal-18) & 0x1f;
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
	if (CurMode->special & _DOUBLESCAN) max_scanline|=0x80;
	if (CurMode->special & _REPEAT1) max_scanline|=0x01;

	switch (CurMode->type) {
	case M_TEXT:
		if(IS_VGA_ARCH) {
			switch(modeset_ctl & 0x90) {
			case 0x0: // 350-lines mode: 8x14 font
				max_scanline |= (14-1);
				break;
			default: // reserved
			case 0x10: // 400 lines 8x16 font
		max_scanline|=CurMode->cheight-1;
				break;
			case 0x80: // 200 lines: 8x8 font and doublescan
				max_scanline |= (8-1);
				max_scanline |= 0x80;
				break;
			}
		} else max_scanline |= CurMode->cheight-1;
		underline=mono_mode ? 0x0f : 0x1f; // mode 7 uses a diff underline position
		break;
	case M_VGA:
		underline=0x40;
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32:
		underline=0x60;			//Seems to enable the every 4th clock on my s3
		break;
	}
	if (CurMode->vdispend==350) underline=0x0f;

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
	case M_LIN24:
		offset = 3 * CurMode->swidth/8;
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
				mode_control=0xe3;
			}
		break;
	case M_TEXT:
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
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
			misc_output|=0xef;		//Select clock 3 
			Bitu clock=CurMode->vtotal*8*CurMode->htotal*70;
			if(CurMode->type==M_LIN15 || CurMode->type==M_LIN16) clock/=2;
			VGA_SetClock(3,clock/1000);
		}
		Bit8u misc_control_2;
		/* Setup Pixel format */
		switch (CurMode->type) {
		case M_LIN8:
		default:
			misc_control_2=0x00;
			break;
		case M_LIN15:
			misc_control_2=0x30;
			break;
		case M_LIN16:
			misc_control_2=0x50;
			break;
		case M_LIN24:
			misc_control_2=0x70; /* FIXME: Is this right? I have no other reference than comments in vga_s3.cpp and s3freak's patch */
			break;
		case M_LIN32:
			misc_control_2=0xd0;
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
		gfx_data[0x6]|=mono_mode ? 0x0a : 0x0e;		//Either b800 or b000, chain odd/even enable
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
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
		if (IS_EGAVGA_ARCH) gfx_data[0x5]|=0x10;
		break;
	case M_CGA2:
		gfx_data[0x6]|=0x0d;		//graphics mode at at 0xb800=0xbfff, chain odd/even disabled
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
			if (IS_VGA_ARCH) att_data[0x06]=0x14;		//Odd Color 6 yellow/brown.
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
	case M_LIN24:
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
		case M_LIN24:
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
			if (modeset_ctl & 2) INT10_PerformGrayScaleSumming(0,256);
		}
	} else {
		for (Bit8u ct=0x10;ct<ATT_REGS;ct++) {
			if (ct==0x11) continue;	// skip overscan register
			IO_Write(0x3c0,ct);
			IO_Write(0x3c0,att_data[ct]);
		}
		vga.config.pel_panning = 0;
	}
	/* Setup some special stuff for different modes */
	Bit8u feature=real_readb(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE);
	switch (CurMode->type) {
	case M_CGA2:
		feature=(feature&~0x30)|0x20;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x1e);
		break;
	case M_CGA4:
		feature=(feature&~0x30)|0x20;
		if (CurMode->mode==4) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2a);
		else if (CurMode->mode==5) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2e);
		else real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2);
		break;
	case M_TANDY16:
		feature=(feature&~0x30)|0x20;
		break;
	case M_TEXT:
		feature=(feature&~0x30)|0x20;
		switch (CurMode->mode) {
		case 0:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2c);break;
		case 1:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x28);break;
		case 2:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2d);break;
		case 3:
		case 7:real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x29);break;
		}
		break;
	case M_LIN4:
	case M_EGA:	
	case M_VGA:
		feature=(feature&~0x30);
		break;
	}
	// disabled, has to be set in bios.cpp exclusively
//	real_writeb(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE,feature);

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
			case 1600: reg_50|=S3_XGA_1600; break;
			default: break;
		}
		IO_WriteB(crtc_base,0x50); IO_WriteB(crtc_base+1,reg_50);

		Bit8u reg_31, reg_3a;
		switch (CurMode->type) {
			case M_LIN15:
			case M_LIN16:
			case M_LIN24:
			case M_LIN32:
				reg_3a=0x15;
				break;
			case M_LIN8:
				// S3VBE20 does it this way. The other double pixel bit does not
				// seem to have an effect on the Trio64.
				if(CurMode->special&_S3_PIXEL_DOUBLE) reg_3a=0x5;
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
		case M_LIN24:
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

	/* Load text mode font */
	if (CurMode->type==M_TEXT) {
		INT10_ReloadFont();
	}
	// Enable screen memory access
	IO_Write(0x3c4,1); IO_Write(0x3c5,seq_data[1] & ~0x20);
	//LOG_MSG("setmode end");

	if (en_int33) INT10_SetCurMode();

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
			/* Hack for VBE 1.2 modes and 24/32bpp ambiguity */
			if (modelist[i].mode >= 0x100 && modelist[i].mode <= 0x11F &&
				((modelist[i].type == M_LIN32 && !vesa12_modes_32bpp) ||
				 (modelist[i].type == M_LIN24 && vesa12_modes_32bpp))) {
				/* ignore */
			}
			else {
				vmodeBlock = &modelist[i];
				break;
			}
		}
		i++;
	}

	if (!vmodeBlock)
	        return ~0;

	switch(vmodeBlock->type) {
	case M_LIN4:
		if (mode >= 0x100 && !allow_vesa_4bpp) return ~0;
		return vmodeBlock->swidth*vmodeBlock->sheight/2;
	case M_LIN8:
		if (mode >= 0x100 && !allow_vesa_8bpp) return ~0;
		return vmodeBlock->swidth*vmodeBlock->sheight;
	case M_LIN15:
		if (mode >= 0x100 && !allow_vesa_15bpp) return ~0;
		return vmodeBlock->swidth*vmodeBlock->sheight*2;
	case M_LIN16:
		if (mode >= 0x100 && !allow_vesa_16bpp) return ~0;
		return vmodeBlock->swidth*vmodeBlock->sheight*2;
	case M_LIN24:
		if (mode >= 0x100 && !allow_vesa_24bpp) return ~0;
		return vmodeBlock->swidth*vmodeBlock->sheight*3;
	case M_LIN32:
		if (mode >= 0x100 && !allow_vesa_32bpp) return ~0;
		return vmodeBlock->swidth*vmodeBlock->sheight*4;
	case M_TEXT:
		if (mode >= 0x100 && !allow_vesa_tty) return ~0;
		return vmodeBlock->twidth*vmodeBlock->theight*2;
	}
	// Return 0 for all other types, those always fit in memory
	return 0;
}
