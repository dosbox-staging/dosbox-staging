/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

static void gen_init(void);

/* End of needed */

#define X86_REGS		7
#define X86_REG_EAX		0x00
#define X86_REG_ECX		0x01
#define X86_REG_EDX		0x02
#define X86_REG_EBX		0x03
#define X86_REG_EBP		0x04
#define X86_REG_ESI		0x05
#define X86_REG_EDI		0x06

#define X86_REG_MASK(_REG_) (1 << X86_REG_ ## _REG_)

static struct {
	Bitu last_used;
	Bitu stored_flags;
	GenReg * regs[X86_REGS];
} x86gen;

class GenReg {
public:
	GenReg(Bit8u _index,bool _protect) {
		index=_index;protect=_protect;
		notusable=false;dynreg=0;
	}
	DynReg  * dynreg;
	Bitu last_used;			//Keeps track of last assigned regs 
    Bit8u index;
	bool notusable;
	bool protect;
	void Load(DynReg * _dynreg) {
		if (!_dynreg) return;
		if (dynreg) Clear();
		dynreg=_dynreg;
		last_used=x86gen.last_used;
		dynreg->flags&=~DYNFLG_CHANGED;
		dynreg->genreg=this;
		if (dynreg->flags & (DYNFLG_LOAD|DYNFLG_ACTIVE)) {
			cache_addw(0x058b+(index << (8+3)));		//Mov reg,[data]
			cache_addd((Bit32u)dynreg->data);
		}
		dynreg->flags|=DYNFLG_ACTIVE;
	}
	void Save(void) {
		if (!dynreg) IllegalOption();
		dynreg->flags&=~DYNFLG_CHANGED;
		cache_addw(0x0589+(index << (8+3)));		//Mov [data],reg
		cache_addd((Bit32u)dynreg->data);
	}
	void Release(void) {
		if (!dynreg) return;
		if (dynreg->flags&DYNFLG_CHANGED && dynreg->flags&DYNFLG_SAVE) {
			Save();
		}
		dynreg->flags&=~(DYNFLG_CHANGED|DYNFLG_ACTIVE);
		dynreg->genreg=0;dynreg=0;
	}
	void Clear(void) {
		if (!dynreg) return;
		if (dynreg->flags&DYNFLG_CHANGED) {
			Save();
		}
		dynreg->genreg=0;dynreg=0;
	}
	

};

static BlockReturn gen_runcode(Bit8u * code) {
	BlockReturn retval;
#if defined (_MSC_VER)
	__asm {
/* Prepare the flags */
		mov		eax,[code]
		push	ebx
		push	ebp
		push	esi
		push	edi
		mov		ebx,[reg_flags]
		and		ebx,FMASK_TEST
		push	ebx
		popfd
		call	eax
/* Restore the flags */
		pushfd	
		and		dword ptr [reg_flags],~FMASK_TEST
		pop		ecx
		and		ecx,FMASK_TEST
		or		[reg_flags],ecx
		pop		edi
		pop		esi
		pop		ebp
		pop		ebx
		mov		[retval],eax
	}
#else
	__asm__ volatile (
		"movl %1,%%esi					\n"
		"andl %2,%%esi					\n"
		"pushl %%ebp					\n"
		"pushl %%esi					\n"
		"popfl							\n"
		"calll  %4						\n"
		"popl %%ebp						\n"
		"pushfl							\n"
		"andl %3,(%1)					\n"
		"popl %%esi						\n"
	    "andl %2,%%esi					\n"
		"orl %%esi,(%1)					\n"
		:"=a" (retval)
		:"m" (reg_flags), "n" (FMASK_TEST),"n" (~FMASK_TEST),"r" (code)
		:"%ecx","%edx","%ebx","%edi","%esi","cc","memory"
	);
#endif
	return retval;
}

static GenReg * FindDynReg(DynReg * dynreg) {
	x86gen.last_used++;
	if (dynreg->genreg) {
		dynreg->genreg->last_used=x86gen.last_used;
		return dynreg->genreg;
	}
	/* Find best match for selected global reg */
	Bits i;
	Bits first_used,first_index;
	first_used=-1;
	if (dynreg->flags & DYNFLG_HAS8) {
		/* Has to be eax,ebx,ecx,edx */
		for (i=first_index=0;i<=X86_REG_EDX;i++) {
			GenReg * genreg=x86gen.regs[i];
			if (genreg->notusable) continue;
			if (!(genreg->dynreg)) {
				genreg->Load(dynreg);
				return genreg;
			}
			if (genreg->last_used<first_used) {
				first_used=genreg->last_used;
				first_index=i;
			}
		}
		/* No free register found use earliest assigned one */
		GenReg * newreg=x86gen.regs[first_index];
		newreg->Load(dynreg);
		return newreg;
	} else {
		for (i=first_index=X86_REGS-1;i>=0;i--) {
			GenReg * genreg=x86gen.regs[i];
			if (genreg->notusable) continue;
			if (!(genreg->dynreg)) {
				genreg->Load(dynreg);
				return genreg;
			}
			if (genreg->last_used<first_used) {
				first_used=genreg->last_used;
				first_index=i;
			}
		}
		/* No free register found use earliest assigned one */
		GenReg * newreg=x86gen.regs[first_index];
		newreg->Load(dynreg);
		return newreg;
	}
}

static GenReg * ForceDynReg(GenReg * genreg,DynReg * dynreg) {
	genreg->last_used=++x86gen.last_used;
	if (dynreg->genreg==genreg) return genreg;
	if (genreg->dynreg) genreg->Clear();
	if (dynreg->genreg) dynreg->genreg->Clear();
	genreg->Load(dynreg);
	return genreg;
}

static void gen_preloadreg(DynReg * dynreg) {
	FindDynReg(dynreg);
}

static void gen_releasereg(DynReg * dynreg) {
	GenReg * genreg=dynreg->genreg;
	if (genreg) genreg->Release();
	else dynreg->flags&=~(DYNFLG_ACTIVE|DYNFLG_CHANGED);
}

static void gen_setupreg(DynReg * dnew,DynReg * dsetup) {
	dnew->flags=dsetup->flags;
	if (dnew->genreg==dsetup->genreg) return;
	/* Not the same genreg must be wrong */
	if (dnew->genreg) {
		/* Check if the genreg i'm changing is actually linked to me */
		if (dnew->genreg->dynreg==dnew) dnew->genreg->dynreg=0;
	}
	dnew->genreg=dsetup->genreg;
	if (dnew->genreg) dnew->genreg->dynreg=dnew;
}

static void gen_synchreg(DynReg * dnew,DynReg * dsynch) {
	/* First make sure the registers match */
	if (dnew->genreg!=dsynch->genreg) {
		if (dnew->genreg) dnew->genreg->Clear();
		if (dsynch->genreg) {
			dsynch->genreg->Load(dnew);
		}
	}
	/* Always use the loadonce flag from either state */
	dnew->flags|=(dsynch->flags & dnew->flags&DYNFLG_ACTIVE);
	if ((dnew->flags ^ dsynch->flags) & DYNFLG_CHANGED) {
		/* Ensure the changed value gets saved */	
		if (dnew->flags & DYNFLG_CHANGED) {
			dnew->genreg->Save();
		} else dnew->flags|=DYNFLG_CHANGED;
	}
}

static void gen_storeflags(void) {
	if (!x86gen.stored_flags) {
		cache_addb(0x9c);		//PUSHFD
	}
	x86gen.stored_flags++;
}

static void gen_restoreflags(bool noreduce=false) {
	if (noreduce) {
		cache_addb(0x9d);
		return;
	}
	if (x86gen.stored_flags) {
		x86gen.stored_flags--;
		if (!x86gen.stored_flags) 
			cache_addb(0x9d);		//POPFD
	} else IllegalOption();
}

static void gen_reinit(void) {
	x86gen.last_used=0;
	x86gen.stored_flags=0;
	for (Bitu i=0;i<X86_REGS;i++) {
		x86gen.regs[i]->dynreg=0;
	}
}

static void gen_dop_byte(DualOps op,DynReg * dr1,Bit8u di1,DynReg * dr2,Bit8u di2) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	switch (op) {
	case DOP_ADD:cache_addb(0x02);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_OR: cache_addb(0x0a);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_ADC:cache_addb(0x12);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_SBB:cache_addb(0x1a);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_AND:cache_addb(0x22);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_SUB:cache_addb(0x2a);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_XOR:cache_addb(0x32);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_CMP:cache_addb(0x3a);break;
	case DOP_MOV:cache_addb(0x8a);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_XCHG:cache_addb(0x86);dr1->flags|=DYNFLG_CHANGED;dr2->flags|=DYNFLG_CHANGED;break;
	case DOP_TEST:cache_addb(0x84);break;
	default:
		IllegalOption();
	}
	cache_addb(0xc0+((gr1->index+di1)<<3)+gr2->index+di2);
}

static void gen_dop_byte_imm(DualOps op,DynReg * dr1,Bit8u di1,Bitu imm) {
	GenReg * gr1=FindDynReg(dr1);
	switch (op) {
	case DOP_ADD:
		cache_addw(0xc080+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_OR:
		cache_addw(0xc880+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_ADC:
		cache_addw(0xd080+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_SBB:
		cache_addw(0xd880+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_AND:
		cache_addw(0xe080+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_SUB:
		cache_addw(0xe880+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_XOR:
		cache_addw(0xf080+((gr1->index+di1)<<8));
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_CMP:
		cache_addw(0xf880+((gr1->index+di1)<<8));
		break;//Doesn't change
	case DOP_MOV:
		cache_addb(0xb0+gr1->index+di1);
		dr1->flags|=DYNFLG_CHANGED;
		break;
	case DOP_TEST:
		cache_addw(0xc0f6+((gr1->index+di1)<<8));
		break;//Doesn't change
	default:
		IllegalOption();
	}
	cache_addb(imm);
}

static void gen_sop_byte(SingleOps op,DynReg * dr1,Bit8u di1) {
	GenReg * gr1=FindDynReg(dr1);
	switch (op) {
	case SOP_INC:cache_addw(0xc0FE + ((gr1->index+di1)<<8));break;
	case SOP_DEC:cache_addw(0xc8FE + ((gr1->index+di1)<<8));break;
	case SOP_NOT:cache_addw(0xd0f6 + ((gr1->index+di1)<<8));break;
	case SOP_NEG:cache_addw(0xd8f6 + ((gr1->index+di1)<<8));break;
	default:
		IllegalOption();
	}
	dr1->flags|=DYNFLG_CHANGED;
}


static void gen_extend_word(bool sign,DynReg * ddr,DynReg * dsr) {
	GenReg * gdr=FindDynReg(ddr);GenReg * gsr=FindDynReg(dsr);
	if (sign) cache_addw(0xbf0f);
	else cache_addw(0xb70f);
	cache_addb(0xc0+(gdr->index<<3)+(gsr->index));
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_extend_byte(bool sign,bool dword,DynReg * ddr,DynReg * dsr,Bit8u dsi) {
	GenReg * gdr=FindDynReg(ddr);GenReg * gsr=FindDynReg(dsr);
	if (!dword) cache_addb(0x66);
	if (sign) cache_addw(0xbe0f);
	else cache_addw(0xb60f);
	cache_addb(0xc0+(gdr->index<<3)+(gsr->index+dsi));
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_lea(DynReg * ddr,DynReg * dsr1,DynReg * dsr2,Bitu scale,Bits imm) {
	GenReg * gdr=FindDynReg(ddr);
	Bitu imm_size;
	Bit8u rm_base=(gdr->index << 3);
	if (dsr1) {
		GenReg * gsr1=FindDynReg(dsr1);
		if (!imm && (gsr1->index!=0x5)) {
			imm_size=0;	rm_base+=0x0;			//no imm
		} else if ((imm>=-128 && imm<=127)) {
			imm_size=1;rm_base+=0x40;			//Signed byte imm
		} else {
			imm_size=4;rm_base+=0x80;			//Signed dword imm
		}
		if (dsr2) {
			GenReg * gsr2=FindDynReg(dsr2);
			cache_addb(0x8d);		//LEA
			cache_addb(rm_base+0x4);			//The sib indicator
			Bit8u sib=(gsr1->index)+(gsr2->index<<3)+(scale<<6);
			cache_addb(sib);
		} else {
			cache_addb(0x8d);		//LEA
			cache_addb(rm_base+gsr1->index);
		}
	} else {
		if (dsr2) {
			GenReg * gsr2=FindDynReg(dsr2);
			cache_addb(0x8d);			//LEA
			cache_addb(rm_base+0x4);	//The sib indicator
			Bit8u sib=(5+(gsr2->index<<3)+(scale<<6));
			cache_addb(sib);
			imm_size=4;
		} else {
			cache_addb(0x8d);			//LEA
			cache_addb(rm_base+0x05);	//dword imm
			imm_size=4;
		}
	}
	switch (imm_size) {
	case 0:	break;
	case 1:cache_addb(imm);break;
	case 4:cache_addd(imm);break;
	}
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_dop_word(DualOps op,bool dword,DynReg * dr1,DynReg * dr2) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	switch (op) {
	case DOP_ADD:cache_addb(0x03);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_OR: cache_addb(0x0b);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_ADC:cache_addb(0x13);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_SBB:cache_addb(0x1b);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_AND:cache_addb(0x23);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_SUB:cache_addb(0x2b);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_XOR:cache_addb(0x33);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_CMP:cache_addb(0x3b);break;
	case DOP_MOV:cache_addb(0x8b);dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_XCHG:cache_addb(0x87);dr1->flags|=DYNFLG_CHANGED;dr2->flags|=DYNFLG_CHANGED;break;
	case DOP_TEST:cache_addb(0x85);break;
	default:
		IllegalOption();
	}
	cache_addb(0xc0+(gr1->index<<3)+gr2->index);
}

static void gen_dop_word_imm(DualOps op,bool dword,DynReg * dr1,Bits imm) {
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	switch (op) {
	case DOP_ADD:cache_addw(0xc081+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_OR: cache_addw(0xc881+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_ADC:cache_addw(0xd081+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_SBB:cache_addw(0xd881+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_AND:cache_addw(0xe081+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_SUB:cache_addw(0xe881+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_XOR:cache_addw(0xf081+(gr1->index<<8));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_CMP:cache_addw(0xf881+(gr1->index<<8));break;//Doesn't change
	case DOP_MOV:cache_addb(0xb8+(gr1->index));dr1->flags|=DYNFLG_CHANGED;break;
	case DOP_TEST:cache_addw(0xc0f7+(gr1->index<<8));break;//Doesn't change
	default:
		IllegalOption();
	}
	if (dword) cache_addd(imm);
	else cache_addw(imm);
}

static void gen_imul_word(bool dword,DynReg * dr1,DynReg * dr2) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	cache_addw(0xaf0f);
	cache_addb(0xc0+(gr1->index<<3)+gr2->index);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_imul_word_imm(bool dword,DynReg * dr1,DynReg * dr2,Bits imm) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	 if ((imm>=-128 && imm<=127)) {
		cache_addb(0x6b);
		cache_addb(0xc0+(gr1->index<<3)+gr2->index);
		cache_addb(imm);
	} else {
		cache_addb(0x69);
		cache_addb(0xc0+(gr1->index<<3)+gr2->index);
		if (dword) cache_addd(imm);
		else cache_addw(imm);
	}
	dr1->flags|=DYNFLG_CHANGED;
}


static void gen_sop_word(SingleOps op,bool dword,DynReg * dr1) {
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	switch (op) {
	case SOP_INC:cache_addb(0x40+gr1->index);break;
	case SOP_DEC:cache_addb(0x48+gr1->index);break;
	case SOP_NOT:cache_addw(0xd0f7+(gr1->index<<8));break;
	case SOP_NEG:cache_addw(0xd8f7+(gr1->index<<8));break;
	default:
		IllegalOption();
	}
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_byte(ShiftOps op,DynReg * drecx,DynReg * dr1,Bit8u di1) {
	ForceDynReg(x86gen.regs[X86_REG_ECX],drecx);
	GenReg * gr1=FindDynReg(dr1);
	switch (op) {
	case SHIFT_ROL:cache_addw(0xc0d2+((gr1->index+di1)<<8));break;
	case SHIFT_ROR:cache_addw(0xc8d2+((gr1->index+di1)<<8));break;
	case SHIFT_RCL:cache_addw(0xd0d2+((gr1->index+di1)<<8));break;
	case SHIFT_RCR:cache_addw(0xd8d2+((gr1->index+di1)<<8));break;
	case SHIFT_SHL:cache_addw(0xe0d2+((gr1->index+di1)<<8));break;
	case SHIFT_SHR:cache_addw(0xe8d2+((gr1->index+di1)<<8));break;
	case SHIFT_SAR:cache_addw(0xf8d2+((gr1->index+di1)<<8));break;
	default:
		IllegalOption();
	}
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_word(ShiftOps op,DynReg * drecx,bool dword,DynReg * dr1) {
	ForceDynReg(x86gen.regs[X86_REG_ECX],drecx);
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	switch (op) {
	case SHIFT_ROL:cache_addw(0xc0d3+((gr1->index)<<8));break;
	case SHIFT_ROR:cache_addw(0xc8d3+((gr1->index)<<8));break;
	case SHIFT_RCL:cache_addw(0xd0d3+((gr1->index)<<8));break;
	case SHIFT_RCR:cache_addw(0xd8d3+((gr1->index)<<8));break;
	case SHIFT_SHL:cache_addw(0xe0d3+((gr1->index)<<8));break;
	case SHIFT_SHR:cache_addw(0xe8d3+((gr1->index)<<8));break;
	case SHIFT_SAR:cache_addw(0xf8d3+((gr1->index)<<8));break;
	default:
		IllegalOption();
	}
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_word_imm(ShiftOps op,bool dword,DynReg * dr1,Bit8u imm) {
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	switch (op) {
	case SHIFT_ROL:cache_addw(0xc0c1+((gr1->index)<<8));break;
	case SHIFT_ROR:cache_addw(0xc8c1+((gr1->index)<<8));break;
	case SHIFT_RCL:cache_addw(0xd0c1+((gr1->index)<<8));break;
	case SHIFT_RCR:cache_addw(0xd8c1+((gr1->index)<<8));break;
	case SHIFT_SHL:cache_addw(0xe0c1+((gr1->index)<<8));break;
	case SHIFT_SHR:cache_addw(0xe8c1+((gr1->index)<<8));break;
	case SHIFT_SAR:cache_addw(0xf8c1+((gr1->index)<<8));break;
	default:
		IllegalOption();
	}
	cache_addb(imm);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_cbw(bool dword,DynReg * dyn_ax) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	if (!dword) cache_addb(0x66);
	cache_addb(0x98);
	dyn_ax->flags|=DYNFLG_CHANGED;
}

static void gen_cwd(bool dword,DynReg * dyn_ax,DynReg * dyn_dx) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	ForceDynReg(x86gen.regs[X86_REG_EDX],dyn_dx);
	if (!dword) cache_addb(0x66);
	cache_addb(0x99);
	dyn_ax->flags|=DYNFLG_CHANGED;
	dyn_dx->flags|=DYNFLG_CHANGED;
}

static void gen_mul_byte(bool imul,DynReg * dyn_ax,DynReg * dr1,Bit8u di1) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	GenReg * gr1=FindDynReg(dr1);
	if (imul) cache_addw(0xe8f6+((gr1->index+di1)<<8));
	else cache_addw(0xe0f6+((gr1->index+di1)<<8));
	dyn_ax->flags|=DYNFLG_CHANGED;
}

static void gen_mul_word(bool imul,DynReg * dyn_ax,DynReg * dyn_dx,bool dword,DynReg * dr1) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	ForceDynReg(x86gen.regs[X86_REG_EDX],dyn_dx);
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	if (imul) cache_addw(0xe8f7+(gr1->index<<8));
	else cache_addw(0xe0f7+(gr1->index<<8));
	dyn_ax->flags|=DYNFLG_CHANGED;
	dyn_dx->flags|=DYNFLG_CHANGED;
}

static void gen_dshift_imm(bool dword,bool left,DynReg * dr1,DynReg * dr2,Bitu imm) {
	GenReg * gr1=FindDynReg(dr1);
	GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	if (left) cache_addw(0xa40f);		//SHLD IMM
	else  cache_addw(0xac0f);			//SHRD IMM
	cache_addb(0xc0+gr1->index+(gr2->index<<3));
	cache_addb(imm);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_dshift_cl(bool dword,bool left,DynReg * dr1,DynReg * dr2,DynReg * drecx) {
	ForceDynReg(x86gen.regs[X86_REG_ECX],drecx);
	GenReg * gr1=FindDynReg(dr1);
	GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	if (left) cache_addw(0xa50f);		//SHLD CL
	else  cache_addw(0xad0f);			//SHRD CL
	cache_addb(0xc0+gr1->index+(gr2->index<<3));
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_call_function(void * func,char * ops,...) {
	Bits paramcount=0;
	struct ParamInfo {
		char * line;
		Bitu value;
	} pinfo[32];
	ParamInfo * retparam=0;
	/* Clear the EAX Genreg for usage */
	x86gen.regs[X86_REG_EAX]->Clear();
	x86gen.regs[X86_REG_EAX]->notusable=true;;
	/* Save the flags */
	gen_storeflags();
	/* Scan for the amount of params */
	if (ops) {
		va_list params;
		va_start(params,ops);
		Bits pindex=0;
		while (*ops) {
			if (*ops=='%') {
                pinfo[pindex].line=ops+1;
				pinfo[pindex].value=va_arg(params,Bitu);
				pindex++;
			}
			ops++;
		}
		paramcount=0;
		while (pindex) {
			pindex--;
			char * scan=pinfo[pindex].line;
			switch (*scan++) {
			case 'I':				/* immediate value */
				paramcount++;
				cache_addb(0x68);			//Push immediate
				cache_addd(pinfo[pindex].value);	//Push value
				break;
			case 'D':				/* Dynamic register */
				{
					bool release=false;
					paramcount++;
					DynReg * dynreg=(DynReg *)pinfo[pindex].value;
					GenReg * genreg=FindDynReg(dynreg);
					scanagain:
					switch (*scan++) {
					case 'd':
						cache_addb(0x50+genreg->index);		//Push reg
						break;
					case 'w':
						cache_addw(0xb70f);					//MOVZX EAX,reg
						cache_addb(0xc0+genreg->index);
						cache_addb(0x50);					//Push EAX
						break;
					case 'l':
						cache_addw(0xb60f);					//MOVZX EAX,reg[0]
						cache_addb(0xc0+genreg->index);
						cache_addb(0x50);					//Push EAX
						break;
					case 'h':
						cache_addw(0xb60f);					//MOVZX EAX,reg[1]
						cache_addb(0xc4+genreg->index);
						cache_addb(0x50);					//Push EAX
						break;
					case 'r':								/* release the reg afterwards */
						release=true;
						goto scanagain;
					default:
						IllegalOption();
					}
					if (release) gen_releasereg(dynreg);
				}
				break;
			case 'R':				/* Dynamic register to get the return value */
				retparam =&pinfo[pindex];
				pinfo[pindex].line=scan;
				break;
			default:
				IllegalOption();
			}
		}
	}
	/* Clear some unprotected registers */
	x86gen.regs[X86_REG_ECX]->Clear();
	x86gen.regs[X86_REG_EDX]->Clear();
	/* Do the actual call to the procedure */
	cache_addb(0xe8);
	cache_addd((Bit32u)func - (Bit32u)cache.pos-4);
	/* Restore the params of the stack */
	if (paramcount) {
		cache_addw(0xc483);				//add ESP,imm byte
		cache_addb(paramcount*4);
	}
	/* Save the return value in correct register */
	if (retparam) {
		DynReg * dynreg=(DynReg *)retparam->value;
		GenReg * genreg=FindDynReg(dynreg);
		switch (*retparam->line) {
		case 'd':
			cache_addw(0xc08b+(genreg->index <<(8+3)));	//mov reg,eax
			break;
		case 'w':
			cache_addb(0x66);							
			cache_addw(0xc08b+(genreg->index <<(8+3)));	//mov reg,eax
			break;
		case 'l':
			cache_addw(0xc08a+(genreg->index <<(8+3)));	//mov reg,eax
			break;
		case 'h':
			cache_addw(0xc08a+((genreg->index+4) <<(8+3)));	//mov reg,eax
			break;
		}
		dynreg->flags|=DYNFLG_CHANGED;
	}
	gen_restoreflags();
	/* Restore EAX registers to be used again */
	x86gen.regs[X86_REG_EAX]->notusable=false;
}

static Bit8u * gen_create_branch(BranchTypes type) {
	/* First free all registers */
	cache_addb(0x70+type);
	cache_addb(0);
	return (cache.pos-1);
}

static void gen_fill_branch(Bit8u * data,Bit8u * from=cache.pos) {
	*data=(from-data-1);
}

static Bit8u * gen_create_jump(Bit8u * to=0) {
	/* First free all registers */
	cache_addb(0xe9);
	cache_addd(to-(cache.pos+4));
	return (cache.pos-4);
}

static void gen_fill_jump(Bit8u * data,Bit8u * to=cache.pos) {
	*(Bit32u*)data=(to-data-4);
}


static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	cache_addb(0xa1);
	cache_addd((Bit32u)ptr);
	cache_addb(0xff);		//JMP EA
	if (!imm) {			//NO EBP
		cache_addb(0x20);
    } else if ((imm>=-128 && imm<=127)) {
		cache_addb(0x60);
		cache_addb(imm);
	} else {
		cache_addb(0xa0);
		cache_addd(imm);
	}
}

static void gen_save_flags(DynReg * dynreg,bool stored) {
	GenReg * genreg=FindDynReg(dynreg);
	if (!stored) cache_addb(0x9c);		//Pushfd
	cache_addb(0x58+genreg->index);		//POP 32 REG
	dynreg->flags|=DYNFLG_CHANGED;
}

static void gen_load_flags(DynReg * dynreg) {
	GenReg * genreg=FindDynReg(dynreg);
	cache_addb(0x50+genreg->index);		//PUSH 32
	cache_addb(0x9d);					//POPFD
}

static void gen_save_host_direct(void * data,Bits imm) {
	cache_addw(0x05c7);		//MOV [],dword
	cache_addd((Bit32u)data);
	cache_addd(imm);
}

static void gen_load_host(void * data,DynReg * dr1,Bitu size) {
	GenReg * gr1=FindDynReg(dr1);
	switch (size) {
	case 1:cache_addw(0xb60f);break;	//movzx byte
	case 2:cache_addw(0xb70f);break;	//movzx word
	case 4:cache_addb(0x8b);break;	//mov
	default:
		IllegalOption();
	}
	cache_addb(0x5+(gr1->index<<3));
	cache_addd((Bit32u)data);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_return(BlockReturn retcode) {
	cache_addb(0xb8);
	cache_addd(retcode);
	cache_addb(0xc3);
}

static void gen_init(void) {
	x86gen.regs[X86_REG_EAX]=new GenReg(0,false);
	x86gen.regs[X86_REG_ECX]=new GenReg(1,false);
	x86gen.regs[X86_REG_EDX]=new GenReg(2,false);
	x86gen.regs[X86_REG_EBX]=new GenReg(3,true);
	x86gen.regs[X86_REG_EBP]=new GenReg(5,true);
	x86gen.regs[X86_REG_ESI]=new GenReg(6,true);
	x86gen.regs[X86_REG_EDI]=new GenReg(7,true);
}


