


static EAPoint RMAddress(void) {
	EAPoint seg_base;
	Bit16u off;
	switch ((inst.rm_mod<<3)|inst.rm_eai) {
	case 0x00:
		off=reg_bx+reg_si;
		seg_base=SegBase(ds);
		break;
	case 0x01:
		off=reg_bx+reg_di;
		seg_base=SegBase(ds);
		break;
	case 0x02:
		off=reg_bp+reg_si;
		seg_base=SegBase(ss);
		break;
	case 0x03:
		off=reg_bp+reg_di;
		seg_base=SegBase(ss);
		break;
	case 0x04:
		off=reg_si;
		seg_base=SegBase(ds);
		break;
	case 0x05:
		off=reg_di;
		seg_base=SegBase(ds);
		break;
	case 0x06:
		off=Fetchw();
		seg_base=SegBase(ds);
		break;
	case 0x07:
		off=reg_bx;
		seg_base=SegBase(ds);
		break;

	case 0x08:
		off=reg_bx+reg_si+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x09:
		off=reg_bx+reg_di+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0a:
		off=reg_bp+reg_si+Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0b:
		off=reg_bp+reg_di+Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0c:
		off=reg_si+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0d:
		off=reg_di+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0e:
		off=reg_bp+Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0f:
		off=reg_bx+Fetchbs();
		seg_base=SegBase(ds);
		break;
	
	case 0x10:
		off=reg_bx+reg_si+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x11:
		off=reg_bx+reg_di+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x12:
		off=reg_bp+reg_si+Fetchws();
		seg_base=SegBase(ss);
		break;
	case 0x13:
		off=reg_bp+reg_di+Fetchws();
		seg_base=SegBase(ss);
		break;
	case 0x14:
		off=reg_si+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x15:
		off=reg_di+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x16:
		off=reg_bp+Fetchws();
		seg_base=SegBase(ss);
		break;
	case 0x17:
		off=reg_bx+Fetchws();
		seg_base=SegBase(ds);
		break;
	}
	inst.rm_off=off;
	if (inst.prefix & PREFIX_SEG) {
		return inst.seg.base+off;
	}else return seg_base+off;
}




