#if DBPP == 8
#define PSIZE 1
#define PTYPE Bit8u
#define WC write_cache.b8
#define LC line_cache.b8
#define	redblueMask 0
#define	greenMask 0
#elif DBPP == 15 || DBPP == 16
#define PSIZE 2
#define PTYPE Bit16u
#define WC write_cache.b16
#define LC line_cache.b16
#if DBPP == 15
#define	redblueMask 0x7C1F
#define	greenMask 0x03E0
#elif DBPP == 16
#define redblueMask 0xF81F
#define greenMask 0x07E0
#endif
#elif DBPP == 32
#define PSIZE 4
#define PTYPE Bit32u
#define WC write_cache.b32
#define LC line_cache.b32
#define redblueMask 0xff00ff
#define greenMask 0xff00
#endif

#if SBPP == 8
#if DBPP == 8
#define PMAKE(_VAL) _VAL
#elif DBPP == 15
#define PMAKE(_VAL) Scaler_PaletteLut.b16[_VAL]
#elif DBPP == 16
#define PMAKE(_VAL) Scaler_PaletteLut.b16[_VAL]
#elif DBPP == 32
#define PMAKE(_VAL) Scaler_PaletteLut.b32[_VAL]
#endif
#endif

#define C0 LC[0][x-1]
#define C1 LC[0][x+0]
#define C2 LC[0][x+1]
#define C3 LC[1][x-1]
#define C4 LC[1][x+0]
#define C5 LC[1][x+1]
#define C6 LC[2][x-1]
#define C7 LC[2][x+0]
#define C8 LC[2][x+1]

static void conc3d(Normal,SBPP,DBPP) (const Bit8u * src) {
	const Bit8u * line;
#if SBPP == DBPP
	line=src;
	BituMove(Scaler_DstWrite,line,Scaler_SrcWidth*PSIZE);
#else
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	line=line_cache.b8[0];
	for (Bitu x=0;x<Scaler_SrcWidth;x++) {
		PTYPE pixel=PMAKE(src[x]);
		dst[x]=pixel;LC[0][x]=pixel;
	}
#endif
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines;lines--) {
		BituMove(Scaler_DstWrite,line,Scaler_SrcWidth*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
}

static void conc3d(Normal2x,SBPP,DBPP) (const Bit8u * src) {
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	for (Bitu x=0;x<Scaler_SrcWidth;x++) {
		Bitu pixel=PMAKE(src[x]);
		dst[x*2+0]=dst[x*2+1]=pixel;
		LC[0][x*2+0]=LC[0][x*2+1]=pixel;
	}
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines;lines--) {
		BituMove(Scaler_DstWrite,LC[0],Scaler_SrcWidth*2*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
}

#if (DBPP > 8)
static void conc3d(TV2x,SBPP,DBPP) (const Bit8u * src) {
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	for (Bitu x=0;x<Scaler_SrcWidth;x++) {
		Bitu pixel=PMAKE(src[x]);
		Bitu halfpixel=(((pixel & redblueMask) * 7) >> 3) & redblueMask;
		halfpixel|=(((pixel & greenMask) * 7) >> 3) & greenMask;
		dst[x*2+0]=dst[x*2+1]=halfpixel;
		LC[0][x*2+0]=LC[0][x*2+1]=pixel;
	}
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines;lines--) {
		BituMove(Scaler_DstWrite,LC[0],Scaler_SrcWidth*2*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
}

static void conc3d(Interp2x,SBPP,DBPP) (const Bit8u * src) {
	if (!Scaler_Line) {
		Scaler_Line++;	
		for (Bitu tempx=Scaler_SrcWidth;tempx>0;tempx--) {
			LC[1][tempx] = LC[2][tempx] = PMAKE(src[tempx]);
		}
		return;
	}
lastagain:
	Bitu x=0;
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	C1=C4;C4=C7;C7=PMAKE(src[x]);
	C2=C5;C5=C8;C8=PMAKE(src[x+1]);
	dst[0]   = interp_w2(C4,C1,3,1);
	dst[1]   = interp_w4(C4,C1,C2,C5,5,1,1,1);
	WC[1][0] = interp_w2(C4,C7,3,1);
	WC[1][1] = interp_w4(C4,C5,C8,C7,5,1,1,1);
	for (x=1;x<Scaler_SrcWidth-1;x++) {
		C2=C5;C5=C8;C8=PMAKE(src[x+1]);
		dst[x*2+0]   = interp_w4(C4,C3,C0,C1,5,1,1,1);
		dst[x*2+1]   = interp_w4(C4,C1,C2,C5,5,1,1,1);
		WC[1][x*2+0] = interp_w4(C4,C7,C6,C3,5,1,1,1);
		WC[1][x*2+1] = interp_w4(C4,C5,C8,C7,5,1,1,1);
	}
	dst[x*2]     = interp_w4(C4,C3,C0,C1,5,1,1,1);
	dst[x*2+1]   = interp_w2(C4,C1,3,1);
	WC[1][x*2+0] = interp_w4(C4,C7,C6,C3,5,1,1,1);
	WC[1][x*2+1] = interp_w2(C4,C7,3,1);
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines;lines--) {
		BituMove(Scaler_DstWrite,&WC[1],Scaler_SrcWidth*2*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
	if (++Scaler_Line==Scaler_SrcHeight) goto lastagain;
}

static void conc3d(AdvInterp2x,SBPP,DBPP) (const Bit8u * src) {
	if (!Scaler_Line) {
		Scaler_Line++;	
		for (Bitu tempx=Scaler_SrcWidth;tempx>0;tempx--) {
			LC[1][tempx]=LC[2][tempx]=PMAKE(src[tempx]);
		}
		return;
	}
lastagain:
	Bitu x=0;
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	C1=C4;C4=C7;C7=PMAKE(src[x]);
	C2=C5;C5=C8;C8=PMAKE(src[x+1]);
	dst[0]=C4;
	WC[1][0]=C4;
	dst[1]	= C5 == C1 && C7 != C1 ? C2 : C4;
	WC[1][1]= C5 == C7 && C1 != C7 ? C7 : C4;
	for (x=1;x<Scaler_SrcWidth-1;x++) {
		C2=C5;C5=C8;C8=PMAKE(src[x+1]);
		if (C1 != C7 && C3 != C5) {
			dst[x*2+0]   = C3 == C1 ? interp_w2(C3,C4,5,3) : C4;
			dst[x*2+1]   = C1 == C5 ? interp_w2(C5,C4,5,3) : C4;
			WC[1][x*2+0] = C3 == C7 ? interp_w2(C3,C4,5,3) : C4;
			WC[1][x*2+1] = C7 == C5 ? interp_w2(C5,C4,5,3) : C4;
		} else {
			dst[x*2+0]   = dst[x*2+1] =
			WC[1][x*2+0] = WC[1][x*2+1]= C4;
		}
	}
	x=Scaler_SrcWidth-1;
	dst[x*2]     = (C3 == C1 && C7 != C1) ? C1 : C4;
	WC[1][x*2+0] = (C3 == C7 && C1 != C7) ? C7 : C4;
	dst[x*2+1]   = C4;
	WC[1][x*2+1] = C4;
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines>0;lines--) {
		BituMove(Scaler_DstWrite,&WC[1],2*Scaler_SrcWidth*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
	if (++Scaler_Line==Scaler_SrcHeight) goto lastagain;
}

#endif		//DBPP > 8

static void conc3d(AdvMame2x,SBPP,DBPP) (const Bit8u * src) {
	if (!Scaler_Line) {
		Scaler_Line++;	
		for (Bitu tempx=Scaler_SrcWidth;tempx>0;tempx--) {
			LC[1][tempx]=LC[2][tempx]=PMAKE(src[tempx]);
		}
		return;
	}
lastagain:
	Bitu x=0;
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	C1=C4;C4=C7;C7=PMAKE(src[x]);
	C2=C5;C5=C8;C8=PMAKE(src[x+1]);
	dst[0]=C4;
	WC[1][0]=C4;
	dst[1]	= C5 == C1 && C7 != C1 ? C2 : C4;
	WC[1][1]= C5 == C7 && C1 != C7 ? C7 : C4;
	for (x=1;x<Scaler_SrcWidth-1;x++) {
		C2=C5;C5=C8;C8=PMAKE(src[x+1]);
		if (C1 != C7 && C3 != C5) {
			dst[x*2+0]   = C3 == C1 ? C3 : C4;
			dst[x*2+1]   = C1 == C5 ? C5 : C4;
			WC[1][x*2+0] = C3 == C7 ? C3 : C4;
			WC[1][x*2+1] = C7 == C5 ? C5 : C4;
		} else {
			dst[x*2+0]   = dst[x*2+1] =
			WC[1][x*2+0] = WC[1][x*2+1]= C4;
		}
	}
	x=Scaler_SrcWidth-1;
	dst[x*2]     = (C3 == C1 && C7 != C1) ? C1 : C4;
	WC[1][x*2+0] = (C3 == C7 && C1 != C7) ? C7 : C4;
	dst[x*2+1]   = C4;
	WC[1][x*2+1] = C4;
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines>0;lines--) {
		BituMove(Scaler_DstWrite,&WC[1],2*Scaler_SrcWidth*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
	if (++Scaler_Line==Scaler_SrcHeight) goto lastagain;
}

static void conc3d(AdvMame3x,SBPP,DBPP) (const Bit8u * src) {
	if (!Scaler_Line) {
		Scaler_Line++;	
		for (Bitu tempx=Scaler_SrcWidth;tempx>0;tempx--) {
			LC[1][tempx]=LC[2][tempx]=PMAKE(src[tempx]);
		}
		return;
	}
lastagain:
	PTYPE * dst=(PTYPE *)Scaler_DstWrite;
	LC[0][0]=LC[1][0];LC[1][0]=LC[2][0];LC[2][0]=PMAKE(src[0]);
	LC[0][1]=LC[1][1];LC[1][1]=LC[2][1];LC[2][1]=PMAKE(src[1]);
	/* first pixel */
	dst[0]   = LC[1][0];
	dst[1]   = LC[1][0];
	dst[2]   = (LC[1][1] == LC[0][0] && LC[2][0] != LC[0][0]) ? LC[0][0] : LC[1][0];
	WC[1][0] = LC[1][0];
	WC[1][1] = LC[1][0];
	WC[1][2] = (LC[0][0] != LC[2][0]) ? (((LC[1][1] == LC[0][0] && LC[1][0] != LC[2][1]) || (LC[1][1] == LC[2][0] && LC[1][0] != LC[0][1])) ? LC[1][1] : LC[1][0]) : LC[1][0];
	WC[2][0] = LC[1][0];
	WC[2][1] = LC[1][0];
	WC[2][2] = (LC[1][1] == LC[2][0] && LC[0][0] != LC[2][0]) ? LC[2][0] : LC[1][0];
	Bitu x;
	for (x=1;x<Scaler_SrcWidth-1;x++) {
		C2=C5;C5=C8;C8=PMAKE(src[x+1]);
		if ((C1 != C7) && (C3 != C5)) {
			dst[x*3+0]   = C3 == C1 ?  C3 : C4;
			dst[x*3+1]   = (C3 == C1 && C4 != C2) || (C5 == C1 && C4 != C0) ? C1 : C4;
			dst[x*3+2]   = C5 == C1 ?  C5 : C4;
			WC[1][x*3+0] = (C3 == C1 && C4 != C6) || (C3 == C7 && C4 != C0) ? C3 : C4;
			WC[1][x*3+1] = C4;
			WC[1][x*3+2] = (C5 == C1 && C4 != C8) || (C5 == C7 && C4 != C2) ? C5 : C4;
			WC[2][x*3+0] = C3 == C7 ?  C3 : C4;
			WC[2][x*3+1] = (C3 == C7 && C4 != C8) || (C5 == C7 && C4 != C6) ? C7 : C4;
			WC[2][x*3+2] = C5 == C7 ?  C5 : C4;
		} else {
			dst[x*3+0]   = dst[x*3+1]   = dst[x*3+2]   =
			WC[1][x*3+0] = WC[1][x*3+1] = WC[1][x*3+2] = 
			WC[2][x*3+0] = WC[2][x*3+1] = WC[2][x*3+2] = LC[1][x];
		}
	}
	x=Scaler_SrcWidth-1;
	/* last pixel */
	dst[x*3+0]   = (C3 == C1 && C7 != C1) ? C1 : C4;
	dst[x*3+1]   = C4;
	dst[x*3+2]   = C4;
	WC[1][x*3+0] = (C1 != C7) ? ((C3 == C1 && C4 != C6) || (C3 == C7 && C4 != C0) ? C3 : C4) : C4;
	WC[1][x*3+1] = C4;
	WC[1][x*3+2] = C4;
	WC[2][x*3+0] = (C3 == C7 && C1 != C7) ? C7 : C4;
	WC[2][x*3+1] = C4;
	WC[2][x*3+2] = C4;
	Scaler_DstWrite+=Scaler_DstPitch;
	BituMove(Scaler_DstWrite,&WC[1],3*Scaler_SrcWidth*PSIZE);
	Scaler_DstWrite+=Scaler_DstPitch;
	for (Bitu lines=*Scaler_Index++;lines>0;lines--) {
		BituMove(Scaler_DstWrite,&WC[2],3*Scaler_SrcWidth*PSIZE);
		Scaler_DstWrite+=Scaler_DstPitch;
	}
	if (++Scaler_Line==Scaler_SrcHeight) goto lastagain;
}

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef WC
#undef LC
#undef greenMask
#undef redblueMask


