static Bitu dyn_helper_divb(Bit8u val) {
	if (!val) return 1;
	Bitu quo=reg_ax / val;
	reg_ah=(Bit8u)(reg_ax % val);
	reg_al=(Bit8u)quo;
	if (quo>0xff) return 1;
	return 0;
}

static Bitu dyn_helper_idivb(Bit8s val) {
	if (!val) return 1;
	Bits quo=(Bit16s)reg_ax / val;
	reg_ah=(Bit8s)((Bit16s)reg_ax % val);
	reg_al=(Bit8s)quo;
	if (quo!=(Bit8s)reg_al) return 1;
	return 0;
}

static Bitu dyn_helper_divw(Bit16u val) {
	if (!val) return 1;
	Bitu num=(reg_dx<<16)|reg_ax;
	Bitu quo=num/val;
	reg_dx=(Bit16u)(num % val);
	reg_ax=(Bit16u)quo;
	if (quo!=reg_ax) return 1;
	return 0;
}

static Bitu dyn_helper_idivw(Bit16s val) {
	if (!val) return 1;
	Bits num=(reg_dx<<16)|reg_ax;
	Bits quo=num/val;
	reg_dx=(Bit16s)(num % val);
	reg_ax=(Bit16s)quo;
	if (quo!=(Bit16s)reg_ax) return 1;
	return 0;
}

static Bitu dyn_helper_divd(Bit32u val) {
	if (!val) return 1;
	Bit64u num=(((Bit64u)reg_edx)<<32)|reg_eax;
	Bit64u quo=num/val;
	reg_edx=(Bit32u)(num % val);
	reg_eax=(Bit32u)quo;
	if (quo!=(Bit64u)reg_eax) return 1;
	return 0;
}

static Bitu dyn_helper_idivd(Bit32s val) {
	if (!val) return 1;
	Bit64s num=(((Bit64u)reg_edx)<<32)|reg_eax;
	Bit64s quo=num/val;
	reg_edx=(Bit32s)(num % val);
	reg_eax=(Bit32s)(quo);
	if (quo!=(Bit64s)((Bit32s)reg_eax)) return 1;
	return 0;
}
