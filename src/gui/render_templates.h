#ifdef __GNUC__
template <Bitu dbpp> static INLINE void AddDst(Bit8u * & dst,Bitu val) __attribute__ ((always_inline));
template <Bitu bpp>  static INLINE Bitu LineSize(Bitu pixels) __attribute__ ((always_inline));
template <Bitu sbpp> static INLINE Bitu LoadSrc(Bit8u * & src) __attribute__ ((always_inline));
template <Bitu sbpp,Bitu dbpp> static INLINE Bitu ConvBPP(Bitu val) __attribute__ ((always_inline));
#endif

template <Bitu dbpp> static INLINE void AddDst(Bit8u * & dst,Bitu val) {
	switch (dbpp) {
	case 8: *(Bit8u*)dst=val;dst+=1;break;
	case 16:*(Bit16u*)dst=val;dst+=2;break;
	case 24:*(Bit32u*)dst=val;dst+=3;break;
	case 32:*(Bit32u*)dst=val;dst+=4;break;
	}
}

template <Bitu bpp>
static INLINE Bitu LineSize(Bitu pixels) {
	switch (bpp) {
	case 8:return pixels;
	case 16:return pixels*2;
	case 24:return pixels*3;
	case 32:return pixels*4;
	}
	return 0;
}

static INLINE void BituMove(Bit8u * dst,Bit8u * src,Bitu pixels) {
	pixels/=sizeof(Bitu);
	while (pixels--) {
		*(Bitu*)dst=*(Bitu*)src;
		src+=sizeof(Bitu);
		dst+=sizeof(Bitu);
	}
}

template <Bitu sbpp>
static INLINE Bitu LoadSrc(Bit8u * & src) {
	Bitu val;
	switch (sbpp) {
	case 8:val=*(Bit8u *) src;src+=1;break;
	case 16:val=*(Bit16u *) src;src+=2;break;
	case 24:val=(*(Bit32u *)src)&0xffffff;src+=3;break;
	case 32:val=*(Bit32u *)src;src+=4;break;
	}
	return val;
}


template <Bitu sbpp,Bitu dbpp>
static INLINE Bitu ConvBPP(Bitu val) {
	if (sbpp==8) switch (dbpp) {
		case 8:return val;
		case 16:return render.pal.lookup.bpp16[val];
		case 24:return render.pal.lookup.bpp32[val];
		case 32:return render.pal.lookup.bpp32[val];
	}
	return 0;
}
