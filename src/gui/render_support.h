static void Render_Normal_8_None(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx;
	Bitu rem=dx&3;dx>>=2;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit32u temp=*(Bit32u *)src;src+=4;
			*(Bit32u *)dest=temp;
			dest+=4;
		}
		for (tempx=rem;tempx>0;tempx--) {
			*dest++=*src++;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_8_DoubleWidth(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x*2;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx*2;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			*dest=*src;*(dest+1)=*src;
			src++;dest+=2;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_8_DoubleHeight(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch+x;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx;
	Bitu rem=dx&3;dx>>=2;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit32u temp=*(Bit32u *)src;src+=4;
			*(Bit32u *)dest=temp;
			*(Bit32u *)(dest+render.op.pitch)=temp;
			dest+=4;
		}
		for (tempx=rem;tempx>0;tempx--) {
			*dest=*src;
			*(dest+render.op.pitch)=*src;
			dest++;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_8_DoubleBoth(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx*2;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit8u val=src[0];src++;
			dest[0]=val;dest[1]=val;
			dest[render.op.pitch]=val;dest[render.op.pitch+1]=val;
			dest+=2;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_16_None(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx*2;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit16u val=render.pal.lookup.bpp16[src[0]];src++;
			*(Bit16u *)dest=val;
			dest+=2;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_16_DoubleWidth(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x*4;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx*4;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit16u val=render.pal.lookup.bpp16[src[0]];src++;
			*(Bit16u *)dest=val;
			*(Bit16u *)(dest+2)=val;
			dest+=4;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_16_DoubleHeight(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch+x*2;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx*2;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit16u val=render.pal.lookup.bpp16[src[0]];src++;
			*(Bit16u *)dest=val;
			*(Bit16u *)(dest+render.op.pitch)=val;
			dest+=2;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_16_DoubleBoth(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch+x*4;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx*4;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit16u val=render.pal.lookup.bpp16[src[0]];src++;
			*(Bit16u *)(dest+0)=val;
			*(Bit16u *)(dest+2)=val;
			*(Bit16u *)(dest+render.op.pitch)=val;
			*(Bit16u *)(dest+render.op.pitch+2)=val;
			dest+=4;
		}
		src+=next_src;dest+=next_dest;
	}
}


static void Render_Normal_32_None(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x*4;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx*4;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit32u val=render.pal.lookup.bpp32[src[0]];src++;
			*(Bit32u *)dest=val;
			dest+=4;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_32_DoubleWidth(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x*8;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx*8;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit32u val=render.pal.lookup.bpp32[src[0]];src++;
			*(Bit32u *)dest=val;
			*(Bit32u *)(dest+4)=val;
			dest+=8;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_32_DoubleHeight(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch+x*4;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx*4;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit32u val=render.pal.lookup.bpp32[src[0]];src++;
			*(Bit32u *)dest=val;
			*(Bit32u *)(dest+render.op.pitch)=val;
			dest+=4;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Render_Normal_32_DoubleBoth(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch+x*8;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx*8;
	for (;dy>0;dy--) {
		for (Bitu tempx=dx;tempx>0;tempx--) {
			Bit32u val=render.pal.lookup.bpp32[src[0]];src++;
			*(Bit32u *)(dest+0)=val;
			*(Bit32u *)(dest+4)=val;
			*(Bit32u *)(dest+render.op.pitch)=val;
			*(Bit32u *)(dest+render.op.pitch+4)=val;
			dest+=8;
		}
		src+=next_src;dest+=next_dest;
	}
}


static RENDER_Part_Handler Render_Normal_8_Table[4]= {
	Render_Normal_8_None,Render_Normal_8_DoubleWidth,Render_Normal_8_DoubleHeight,Render_Normal_8_DoubleBoth
};

static RENDER_Part_Handler Render_Normal_16_Table[4]= {
	Render_Normal_16_None,Render_Normal_16_DoubleWidth,Render_Normal_16_DoubleHeight,Render_Normal_16_DoubleBoth
};

static RENDER_Part_Handler Render_Normal_32_Table[4]= {
	Render_Normal_32_None,Render_Normal_32_DoubleWidth,Render_Normal_32_DoubleHeight,Render_Normal_32_DoubleBoth
};


static void Render_Normal_CallBack(Bitu width,Bitu height,Bitu bpp,Bitu pitch,Bitu flags) {
	if (!(flags & MODE_SET)) return;
	render.op.width=width;
	render.op.height=height;
	render.op.bpp=bpp;
	render.op.pitch=pitch;
	switch (bpp) {
	case 8:	
		render.src.part_handler=Render_Normal_8_Table[render.src.flags];
		break;
	case 16:	
		render.src.part_handler=Render_Normal_16_Table[render.src.flags];
		break;
	case 32:	
		render.src.part_handler=Render_Normal_32_Table[render.src.flags];
		break;
	default:
		E_Exit("RENDER:Unsupported display depth of %d",bpp);
		break;
	}
	RENDER_ResetPal();
}