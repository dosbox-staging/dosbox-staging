/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define FPU_ESC_0 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC0_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC0_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_1 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC1_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC1_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_2 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC2_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC2_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_3 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC3_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC3_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_4 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC4_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC4_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_5 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC5_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC5_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_6 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC6_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC6_EA(rm,eaa);	\
			} \
		}

#define FPU_ESC_7 { \
			Bit8u rm=Fetchb(); \
			if (rm>=0xc0) { \
				FPU_ESC7_Normal(rm); \
			} else { \
				GetEAa;FPU_ESC7_EA(rm,eaa);	\
			} \
		}