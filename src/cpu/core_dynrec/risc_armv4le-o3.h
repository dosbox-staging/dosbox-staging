/*
 *  Copyright (C) 2002-2008  The DOSBox Team
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

/* $Id: risc_armv4le-o3.h,v 1.1 2008-08-20 14:13:21 c2woody Exp $ */


/* ARMv4 (little endian) backend by M-HT (size-tweaked arm version) */


// temporary registers
#define temp1 HOST_ip
#define temp2 HOST_v5
#define temp3 HOST_v4

// register that holds function return values
#define FC_RETOP HOST_v3

// register used for address calculations,
#define FC_ADDR HOST_v1			// has to be saved across calls, see DRC_PROTECT_ADDR_REG

// register that holds the first parameter
#define FC_OP1 HOST_a1

// register that holds the second parameter
#define FC_OP2 HOST_a2

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 HOST_a1

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 HOST_a2

// temporary register for LEA
#define TEMP_REG_DRC HOST_v2

// helper macro
#define ROTATE_SCALE(x) ( (x)?(32 - x):(0) )

// move a full register from reg_src to reg_dst
static void gen_mov_regs(HostReg reg_dst,HostReg reg_src) {
	if(reg_src == reg_dst) return;
	cache_addd(0xe1a00000 + (reg_dst << 12) + reg_src);      // mov reg_dst, reg_src
}

// helper function
static Bits get_imm_gen_len(Bit32u imm) {
	Bits ret;
	if (imm == 0) {
		return 1;
	} else {
		ret = 0;
		while (imm) {
			while ((imm & 3) == 0) {
				imm>>=2;
			}
			ret++;
			imm>>=8;
		}
		return ret;
	}
}

// helper function
static Bits get_method_imm_gen_len(Bit32u imm, Bits preffer00, Bits *num) {
	Bits num00, num15, numadd, numsub, numret, ret;
	num00 = get_imm_gen_len(imm);
	num15 = get_imm_gen_len(~imm);
	numadd = get_imm_gen_len(imm - ((Bit32u)cache.pos+8));
	numsub = get_imm_gen_len(((Bit32u)cache.pos+8) - imm);
	if (numsub < numadd && numsub < num00 && numsub < num15) {
		ret = 0;
		numret = numsub;
	} else if (numadd < num00 && numadd < num15) {
		ret = 1;
		numret = numadd;
	} else if (num00 < num15 || (num00 == num15 && preffer00)) {
		ret = 2;
		numret = num00;
	} else {
		ret = 3;
		numret = num15;
	}
	if (num != NULL) *num = numret;
	return ret;
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,Bit32u imm) {
	Bits first, method, scale;
	Bit32u imm2, dist;
	if (imm == 0) {
		cache_addd(0xe3a00000 + (dest_reg << 12));      // mov dest_reg, #0
	} else if (imm == 0xffffffff) {
		cache_addd(0xe3e00000 + (dest_reg << 12));      // mvn dest_reg, #0
	} else {
		method = get_method_imm_gen_len(imm, 1, NULL);

		scale = 0;
		first = 1;
		if (method == 0) {
			dist = ((Bit32u)cache.pos+8) - imm;
			while (dist) {
				while ((dist & 3) == 0) {
					dist>>=2;
					scale+=2;
				}
				if (first) {
					cache_addd(0xe2400000 + (dest_reg << 12) + (HOST_pc << 16) + (ROTATE_SCALE(scale) << 7) + (dist & 0xff));      // sub dest_reg, pc, #((dist & 0xff) << scale)
					first = 0;
				} else {
					cache_addd(0xe2400000 + (dest_reg << 12) + (dest_reg << 16) + (ROTATE_SCALE(scale) << 7) + (dist & 0xff));      // sub dest_reg, dest_reg, #((dist & 0xff) << scale)
				}
				dist>>=8;
				scale+=8;
			}
		} else if (method == 1) {
			dist = imm - ((Bit32u)cache.pos+8);
			if (dist == 0) {
				cache_addd(0xe1a00000 + (dest_reg << 12) + HOST_pc);      // mov dest_reg, pc
			} else {
				while (dist) {
					while ((dist & 3) == 0) {
						dist>>=2;
						scale+=2;
					}
					if (first) {
						cache_addd(0xe2800000 + (dest_reg << 12) + (HOST_pc << 16) + (ROTATE_SCALE(scale) << 7) + (dist & 0xff));      // add dest_reg, pc, #((dist & 0xff) << scale)
						first = 0;
					} else {
						cache_addd(0xe2800000 + (dest_reg << 12) + (dest_reg << 16) + (ROTATE_SCALE(scale) << 7) + (dist & 0xff));      // add dest_reg, dest_reg, #((dist & 0xff) << scale)
					}
					dist>>=8;
					scale+=8;
				}
			}
		} else if (method == 2) {
			while (imm) {
				while ((imm & 3) == 0) {
					imm>>=2;
					scale+=2;
				}
				if (first) {
					cache_addd(0xe3a00000 + (dest_reg << 12) + (ROTATE_SCALE(scale) << 7) + (imm & 0xff));      // mov dest_reg, #((imm & 0xff) << scale)
					first = 0;
				} else {
					cache_addd(0xe3800000 + (dest_reg << 12) + (dest_reg << 16) + (ROTATE_SCALE(scale) << 7) + (imm & 0xff));      // orr dest_reg, dest_reg, #((imm & 0xff) << scale)
				}
				imm>>=8;
				scale+=8;
			}
		} else {
			imm2 = ~imm;
			while (imm2) {
				while ((imm2 & 3) == 0) {
					imm2>>=2;
					scale+=2;
				}
				if (first) {
					cache_addd(0xe3e00000 + (dest_reg << 12) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // mvn dest_reg, #((imm2 & 0xff) << scale)
					first = 0;
				} else {
					cache_addd(0xe3c00000 + (dest_reg << 12) + (dest_reg << 16) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // bic dest_reg, dest_reg, #((imm2 & 0xff) << scale)
				}
				imm2>>=8;
				scale+=8;
			}
		}
	}
}

// helper function for gen_mov_word_to_reg
static void gen_mov_word_to_reg_helper(HostReg dest_reg,void* data,bool dword,HostReg data_reg) {
	// alignment....
	if (dword) {
		if ((Bit32u)data & 3) {
			if ( ((Bit32u)data & 3) == 2 ) {
				cache_addd(0xe1d000b0 + (dest_reg << 12) + (data_reg << 16));      // ldrh dest_reg, [data_reg]
				cache_addd(0xe1d000b2 + (temp2    << 12) + (data_reg << 16));      // ldrh temp2, [data_reg, #2]
				cache_addd(0xe1800800 + (dest_reg << 12) + (dest_reg << 16) + (temp2));      // orr dest_reg, dest_reg, temp2, lsl #16
			} else {
				cache_addd(0xe5d00000 + (dest_reg << 12) + (data_reg << 16));      // ldrb dest_reg, [data_reg]
				cache_addd(0xe1d000b1 + (temp2    << 12) + (data_reg << 16));      // ldrh temp2, [data_reg, #1]
				cache_addd(0xe1800400 + (dest_reg << 12) + (dest_reg << 16) + (temp2));      // orr dest_reg, dest_reg, temp2, lsl #8
				cache_addd(0xe5d00003 + (temp2    << 12) + (data_reg << 16));      // ldrb temp2, [data_reg, #3]
				cache_addd(0xe1800c00 + (dest_reg << 12) + (dest_reg << 16) + (temp2));      // orr dest_reg, dest_reg, temp2, lsl #24
			}
		} else {
			cache_addd(0xe5900000 + (dest_reg << 12) + (data_reg << 16));      // ldr dest_reg, [data_reg]
		}
	} else {
		if ((Bit32u)data & 1) {
			cache_addd(0xe5d00000 + (dest_reg << 12) + (data_reg << 16));      // ldrb dest_reg, [data_reg]
			cache_addd(0xe5d00001 + (temp2    << 12) + (data_reg << 16));      // ldrb temp2, [data_reg, #1]
			cache_addd(0xe1800400 + (dest_reg << 12) + (dest_reg << 16) + (temp2));      // orr dest_reg, dest_reg, temp2, lsl #8
		} else {
			cache_addd(0xe1d000b0 + (dest_reg << 12) + (data_reg << 16));      // ldrh dest_reg, [data_reg]
		}
	}
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)data);
	gen_mov_word_to_reg_helper(dest_reg, data, dword, temp1);
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void gen_mov_word_to_reg_imm(HostReg dest_reg,Bit16u imm) {
	Bits first, scale;
	Bit32u imm2;
	if (imm == 0) {
		cache_addd(0xe3a00000 + (dest_reg << 12));      // mov dest_reg, #0
	} else {
		scale = 0;
		first = 1;
		imm2 = (Bit32u)imm;
		while (imm2) {
			while ((imm2 & 3) == 0) {
				imm2>>=2;
				scale+=2;
			}
			if (first) {
				cache_addd(0xe3a00000 + (dest_reg << 12) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // mov dest_reg, #((imm2 & 0xff) << scale)
				first = 0;
			} else {
				cache_addd(0xe3800000 + (dest_reg << 12) + (dest_reg << 16) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // orr dest_reg, dest_reg, #((imm2 & 0xff) << scale)
			}
			imm2>>=8;
			scale+=8;
		}
	}
}

// helper function for gen_mov_word_from_reg
static void gen_mov_word_from_reg_helper(HostReg src_reg,void* dest,bool dword, HostReg data_reg) {
	// alignment....
	if (dword) {
		if ((Bit32u)dest & 3) {
			if ( ((Bit32u)dest & 3) == 2 ) {
				cache_addd(0xe1c000b0 + (src_reg << 12) + (data_reg << 16));      // strh src_reg, [data_reg]
				cache_addd(0xe1a00820 + (temp2   << 12) + (src_reg));      // mov temp2, src_reg, lsr #16
				cache_addd(0xe1c000b2 + (temp2   << 12) + (data_reg << 16));      // strh temp2, [data_reg, #2]
			} else {
				cache_addd(0xe5c00000 + (src_reg << 12) + (data_reg << 16));      // strb src_reg, [data_reg]
				cache_addd(0xe1a00420 + (temp2   << 12) + (src_reg));      // mov temp2, src_reg, lsr #8
				cache_addd(0xe1c000b1 + (temp2   << 12) + (data_reg << 16));      // strh temp2, [data_reg, #1]
				cache_addd(0xe1a00820 + (temp2   << 12) + (temp2));      // mov temp2, temp2, lsr #16
				cache_addd(0xe5c00003 + (temp2   << 12) + (data_reg << 16));      // strb temp2, [data_reg, #3]
			}
		} else {
			cache_addd(0xe5800000 + (src_reg << 12) + (data_reg << 16));      // str src_reg, [data_reg]
		}
	} else {
		if ((Bit32u)dest & 1) {
			cache_addd(0xe5c00000 + (src_reg << 12) + (data_reg << 16));      // strb src_reg, [data_reg]
			cache_addd(0xe1a00420 + (temp2   << 12) + (src_reg));      // mov temp2, src_reg, lsr #8
			cache_addd(0xe5c00001 + (temp2   << 12) + (data_reg << 16));      // strb temp2, [data_reg, #1]
		} else {
			cache_addd(0xe1c000b0 + (src_reg << 12) + (data_reg << 16));      // strh src_reg, [data_reg]
		}
	}
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
	gen_mov_word_from_reg_helper(src_reg, dest, dword, temp1);
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)data);
	cache_addd(0xe5d00000 + (dest_reg << 12) + (temp1 << 16));      // ldrb dest_reg, [temp1]
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg,void* data) {
	gen_mov_byte_to_reg_low(dest_reg, data);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg,Bit8u imm) {
	cache_addd(0xe3a00000 + (dest_reg << 12) + (imm));      // mov dest_reg, #(imm)
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,Bit8u imm) {
	gen_mov_byte_to_reg_low_imm(dest_reg, imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
	cache_addd(0xe5c00000 + (src_reg << 12) + (temp1 << 16));      // strb src_reg, [temp1]
}



// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign,HostReg reg) {
	if (sign) {
		cache_addd(0xe1a00c00 + (reg << 12) + (reg));      // mov reg, reg, lsl #24
		cache_addd(0xe1a00c40 + (reg << 12) + (reg));      // mov reg, reg, asr #24
	} else {
		cache_addd(0xe20000ff + (reg << 12) + (reg << 16));      // and reg, reg, #0xff
	}
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign,HostReg reg) {
	if (sign) {
		cache_addd(0xe1a00800 + (reg << 12) + (reg));      // mov reg, reg, lsl #16
		cache_addd(0xe1a00840 + (reg << 12) + (reg));      // mov reg, reg, asr #16
	} else {
		cache_addd(0xe1a00800 + (reg << 12) + (reg));      // mov reg, reg, lsl #16
		cache_addd(0xe1a00820 + (reg << 12) + (reg));      // mov reg, reg, lsr #16
	}
}

// add a 32bit value from memory to a full register
static void gen_add(HostReg reg,void* op) {
	gen_mov_word_to_reg(temp3, op, 1);
	cache_addd(0xe0800000 + (reg << 12) + (reg << 16) + (temp3));      // add reg, reg, temp3
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,Bit32u imm) {
	Bits method1, method2, num1, num2, scale, sub;
	if(!imm) return;
	if (imm == 1) {
		cache_addd(0xe2800001 + (reg << 12) + (reg << 16));      // add reg, reg, #1
	} else if (imm == 0xffffffff) {
		cache_addd(0xe2400001 + (reg << 12) + (reg << 16));      // sub reg, reg, #1
	} else {
		method1 = get_method_imm_gen_len(imm, 1, &num1);
		method2 = get_method_imm_gen_len(-((Bit32s)imm), 1, &num2);
		if (num2 < num1) {
			method1 = method2;
			imm = (Bit32u)(-((Bit32s)imm));
			sub = 1;
		} else sub = 0;

		if (method1 != 2) {
			gen_mov_dword_to_reg_imm(temp3, imm);
			if (sub) {
				cache_addd(0xe0400000 + (reg << 12) + (reg << 16) + (temp3));      // sub reg, reg, temp3
			} else {
				cache_addd(0xe0800000 + (reg << 12) + (reg << 16) + (temp3));      // add reg, reg, temp3
			}
		} else {
			scale = 0;
			while (imm) {
				while ((imm & 3) == 0) {
					imm>>=2;
					scale+=2;
				}
				if (sub) {
					cache_addd(0xe2400000 + (reg << 12) + (reg << 16) + (ROTATE_SCALE(scale) << 7) + (imm & 0xff));      // sub reg, reg, #((imm & 0xff) << scale)
				} else {
					cache_addd(0xe2800000 + (reg << 12) + (reg << 16) + (ROTATE_SCALE(scale) << 7) + (imm & 0xff));      // add reg, reg, #((imm & 0xff) << scale)
				}
				imm>>=8;
				scale+=8;
			}
		}
	}
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,Bit32u imm) {
	Bits method, scale;
	Bit32u imm2;
	imm2 = ~imm;
	if(!imm2) return;
	if (!imm) {
		cache_addd(0xe3a00000 + (reg << 12));      // mov reg, #0
	} else {
		method = get_method_imm_gen_len(imm, 0, NULL);
		if (method != 3) {
			gen_mov_dword_to_reg_imm(temp3, imm);
			cache_addd(0xe0000000 + (reg << 12) + (reg << 16) + (temp3));      // and reg, reg, temp3
		} else {
			scale = 0;
			while (imm2) {
				while ((imm2 & 3) == 0) {
					imm2>>=2;
					scale+=2;
				}
				cache_addd(0xe3c00000 + (reg << 12) + (reg << 16) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // bic reg, reg, #((imm2 & 0xff) << scale)
				imm2>>=8;
				scale+=8;
			}
		}
	}
}


// move a 32bit constant value into memory
static void gen_mov_direct_dword(void* dest,Bit32u imm) {
	gen_mov_dword_to_reg_imm(temp3, imm);
	gen_mov_word_from_reg(temp3, dest, 1);
}

// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,DRC_PTR_SIZE_IM imm) {
	gen_mov_direct_dword(dest,(Bit32u)imm);
}

// add an 8bit constant value to a dword memory value
static void gen_add_direct_byte(void* dest,Bit8s imm) {
	if(!imm) return;
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
	gen_mov_word_to_reg_helper(temp3, dest, 1, temp1);
	if (imm >= 0) {
		cache_addd(0xe2800000 + (temp3 << 12) + (temp3 << 16) + ((Bit32s)imm));      // add temp3, temp3, #(imm)
	} else {
		cache_addd(0xe2400000 + (temp3 << 12) + (temp3 << 16) + (-((Bit32s)imm)));      // sub temp3, temp3, #(-imm)
	}
	gen_mov_word_from_reg_helper(temp3, dest, 1, temp1);
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,Bit32u imm,bool dword) {
	if(!imm) return;
	if (dword && ( (imm<128) || (imm>=0xffffff80) ) ) {
		gen_add_direct_byte(dest,(Bit8s)imm);
		return;
	}
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
	gen_mov_word_to_reg_helper(temp3, dest, dword, temp1);
	// maybe use function gen_add_imm
	if (dword) {
		gen_mov_dword_to_reg_imm(temp2, imm);
	} else {
		gen_mov_word_to_reg_imm(temp2, (Bit16u)imm);
	}
	cache_addd(0xe0800000 + (temp3 << 12) + (temp3 << 16) + (temp2));      // add temp3, temp3, temp2
	gen_mov_word_from_reg_helper(temp3, dest, dword, temp1);
}

// subtract an 8bit constant value from a dword memory value
static void gen_sub_direct_byte(void* dest,Bit8s imm) {
	if(!imm) return;
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
	gen_mov_word_to_reg_helper(temp3, dest, 1, temp1);
	if (imm >= 0) {
		cache_addd(0xe2400000 + (temp3 << 12) + (temp3 << 16) + ((Bit32s)imm));      // sub temp3, temp3, #(imm)
	} else {
		cache_addd(0xe2800000 + (temp3 << 12) + (temp3 << 16) + (-((Bit32s)imm)));      // add temp3, temp3, #(-imm)
	}
	gen_mov_word_from_reg_helper(temp3, dest, 1, temp1);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void gen_sub_direct_word(void* dest,Bit32u imm,bool dword) {
	if(!imm) return;
	if (dword && ( (imm<128) || (imm>=0xffffff80) ) ) {
		gen_sub_direct_byte(dest,(Bit8s)imm);
		return;
	}
	gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
	gen_mov_word_to_reg_helper(temp3, dest, dword, temp1);
	// maybe use function gen_add_imm/gen_sub_imm
	if (dword) {
		gen_mov_dword_to_reg_imm(temp2, imm);
	} else {
		gen_mov_word_to_reg_imm(temp2, (Bit16u)imm);
	}
	cache_addd(0xe0400000 + (temp3 << 12) + (temp3 << 16) + (temp2));      // sub temp3, temp3, temp2
	gen_mov_word_from_reg_helper(temp3, dest, dword, temp1);
}

// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,HostReg scale_reg,Bitu scale,Bits imm) {
	cache_addd(0xe0800000 + (dest_reg << 12) + (dest_reg << 16) + (scale_reg) + (scale << 7));      // add dest_reg, dest_reg, scale_reg, lsl #(scale)
	gen_add_imm(dest_reg, imm);
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_addd(0xe1a00000 + (dest_reg << 12) + (dest_reg) + (scale << 7));      // mov dest_reg, dest_reg, lsl #(scale)
	}
	gen_add_imm(dest_reg, imm);
}

// generate a call to a parameterless function
static void INLINE gen_call_function_raw(void * func) {
	cache_addd(0xe5900004 + (temp1   << 12) + (HOST_pc << 16));      // ldr temp1, [pc, #4]
	cache_addd(0xe2800004 + (HOST_lr << 12) + (HOST_pc << 16));      // add lr, pc, #4
	cache_addd(0xe12fff10 + (temp1));      // bx temp1
	cache_addd((Bit32u)func);      // .int func
	cache_addd(0xe1a00000 + (FC_RETOP << 12) + HOST_a1);      // mov FC_RETOP, a1
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
static Bit32u INLINE gen_call_function_setup(void * func,Bitu paramcount,bool fastcall=false) {
	Bit32u proc_addr = (Bit32u)cache.pos;
	gen_call_function_raw(func);
	return proc_addr;
}

#if (1)
// max of 4 parameters in a1-a4

// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param) {
	gen_mov_dword_to_reg_imm(param, imm);
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(Bitu addr,Bitu param) {
	gen_mov_dword_to_reg_imm(param, addr);
}

// load a host-register as param'th function parameter
static void INLINE gen_load_param_reg(Bitu reg,Bitu param) {
	gen_mov_regs(param, reg);
}

// load a value from memory as param'th function parameter
static void INLINE gen_load_param_mem(Bitu mem,Bitu param) {
	gen_mov_word_to_reg(param, (void *)mem, 1);
}
#else
	other arm abis
#endif

// jump to an address pointed at by ptr, offset is in imm
static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	Bits num1, num2, scale, sub;
	Bitu imm2;
	gen_mov_word_to_reg(temp3, ptr, 1);

	if (imm) {
		num1 = get_imm_gen_len(imm);
		num2 = get_imm_gen_len(-imm);

		if (num2 < num1) {
			imm = -imm;
			sub = 1;
		} else sub = 0;

		scale = 0;
		imm2 = (Bitu)imm;
		while (imm2) {
			while ((imm2 & 3) == 0) {
				imm2>>=2;
				scale+=2;
			}
			if (sub) {
				cache_addd(0xe2400000 + (temp3 << 12) + (temp3 << 16) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // sub temp3, temp3, #((imm2 & 0xff) << scale)
			} else {
				cache_addd(0xe2800000 + (temp3 << 12) + (temp3 << 16) + (ROTATE_SCALE(scale) << 7) + (imm2 & 0xff));      // add temp3, temp3, #((imm2 & 0xff) << scale)
			}
			imm2>>=8;
			scale+=8;
		}
	}

#if (1)
// (*ptr) should be word aligned 
	if ((imm & 0x03) == 0) {
		cache_addd(0xe5900000 + (temp1 << 12) + (temp3 << 16));      // ldr temp1, [temp3]
	} else
#endif
	{
		cache_addd(0xe5d00000 + (temp1 << 12) + (temp3 << 16));      // ldrb temp1, [temp3]
		cache_addd(0xe5d00001 + (temp2 << 12) + (temp3 << 16));      // ldrb temp2, [temp3, #1]
		cache_addd(0xe1800400 + (temp1 << 12) + (temp1 << 16) + (temp2));      // orr temp1, temp1, temp2, lsl #8
		cache_addd(0xe5d00002 + (temp2 << 12) + (temp3 << 16));      // ldrb temp2, [temp3, #2]
		cache_addd(0xe1800800 + (temp1 << 12) + (temp1 << 16) + (temp2));      // orr temp1, temp1, temp2, lsl #16
		cache_addd(0xe5d00003 + (temp2 << 12) + (temp3 << 16));      // ldrb temp2, [temp3, #3]
		cache_addd(0xe1800c00 + (temp1 << 12) + (temp1 << 16) + (temp2));      // orr temp1, temp1, temp2, lsl #24
	}

	cache_addd(0xe12fff10 + (temp1));      // bx temp1
}

// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static Bit32u INLINE gen_create_branch_on_zero(HostReg reg,bool dword) {
	if (dword) {
		cache_addd(0xe3500000 + (reg << 16));      // cmp reg, #0
	} else {
		cache_addd(0xe1b00800 + (temp1 << 12) + (reg));      // movs temp1, reg, lsl #16
	}
	cache_addd(0x0a000000);      // beq j
	return ((Bit32u)cache.pos-4);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static Bit32u INLINE gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	if (dword) {
		cache_addd(0xe3500000 + (reg << 16));      // cmp reg, #0
	} else {
		cache_addd(0xe1b00800 + (temp1 << 12) + (reg));      // movs temp1, reg, lsl #16
	}
	cache_addd(0x1a000000);      // bne j
	return ((Bit32u)cache.pos-4);
}

// calculate relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bits len=(Bit32u)cache.pos-(data+8);
	if (len<0) len=-len;
	if (len>0x02000000) LOG_MSG("Big jump %d",len);
#endif
	*(Bit32u*)data=( (*(Bit32u*)data) & 0xff000000 ) | ( ( ((Bit32u)cache.pos - (data+8)) >> 2 ) & 0x00ffffff );
}

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static Bit32u gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	if (isdword) {
		cache_addd(0xe3500000 + (reg << 16));      // cmp reg, #0
	} else {
		cache_addd(0xe31000ff + (reg << 16));      // tst reg, #0xff
	}
	cache_addd(0x0a000002);      // beq nobranch
	cache_addd(0xe5900000 + (temp1 << 12) + (HOST_pc << 16));      // ldr temp1, [pc, #0]
	cache_addd(0xe12fff10 + (temp1));      // bx temp1
	cache_addd(0);      // fill j
	// nobranch:
	return ((Bit32u)cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static Bit32u INLINE gen_create_branch_long_leqzero(HostReg reg) {
	cache_addd(0xe3500000 + (reg << 16));      // cmp reg, #0
	cache_addd(0xca000002);      // bgt nobranch
	cache_addd(0xe5900000 + (temp1 << 12) + (HOST_pc << 16));      // ldr temp1, [pc, #0]
	cache_addd(0xe12fff10 + (temp1));      // bx temp1
	cache_addd(0);      // fill j
	// nobranch:
	return ((Bit32u)cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch_long(Bit32u data) {
	// this is an absolute branch
	*(Bit32u*)data=(Bit32u)cache.pos;
}

static void gen_run_code(void) {
	cache_addd(0xe92d4000);			// stmfd sp!, {lr}
	cache_addd(0xe92d01f0);			// stmfd sp!, {v1-v5}
	cache_addd(0xe28fe004);			// add lr, pc, #4
	cache_addd(0xe92d4000);			// stmfd sp!, {lr}
	cache_addd(0xe12fff10);			// bx r0
	cache_addd(0xe8bd01f0);			// ldmfd sp!, {v1-v5}

	cache_addd(0xe8bd4000);			// ldmfd sp!, {lr}
	cache_addd(0xe12fff1e);			// bx lr
}

// return from a function
static void gen_return_function(void) {
	cache_addd(0xe1a00000 + (HOST_a1 << 12) + FC_RETOP);	// mov a1, FC_RETOP
	cache_addd(0xe8bd4000);			// ldmfd sp!, {lr}
	cache_addd(0xe12fff1e);			// bx lr
}

#ifdef DRC_FLAGS_INVALIDATION

// called when a call to a function can be replaced by a
// call to a simpler function
static void gen_fill_function_ptr(Bit8u * pos,void* fct_ptr,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION_DCODE
	// try to avoid function calls but rather directly fill in code
	switch (flags_type) {
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			*(Bit32u*)pos=0xe0800000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (HOST_a2);	// add FC_RETOP, a1, a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*(Bit32u*)pos=0xe1800000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (HOST_a2);	// orr FC_RETOP, a1, a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*(Bit32u*)pos=0xe0000000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (HOST_a2);	// and FC_RETOP, a1, a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*(Bit32u*)pos=0xe0400000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (HOST_a2);	// sub FC_RETOP, a1, a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*(Bit32u*)pos=0xe0200000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (HOST_a2);	// eor FC_RETOP, a1, a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			*(Bit32u*)pos=0xea000000 + (3);				// b (pc+3*4)
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*(Bit32u*)pos=0xe2800000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (1);	// add FC_RETOP, a1, #1
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*(Bit32u*)pos=0xe2400000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (1);	// sub FC_RETOP, a1, #1
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_SHLb:
		case t_SHLw:
		case t_SHLd:
			*(Bit32u*)pos=0xe1a00010 + (FC_RETOP << 12) + (HOST_a1) + (HOST_a2 << 8);	// mov FC_RETOP, a1, lsl a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_SHRb:
			*(Bit32u*)pos=0xe2000000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (0xff);			// and FC_RETOP, a1, #0xff
			*(Bit32u*)(pos+4)=0xe1a00030 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);	// mov FC_RETOP, FC_RETOP, lsr a2
			*(Bit32u*)(pos+8)=0xe1a00000;				// nop
			*(Bit32u*)(pos+12)=0xe1a00000;				// nop
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_SHRw:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (16 << 7);			// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+4)=0xe1a00020 + (FC_RETOP << 12) + (FC_RETOP) + (16 << 7);		// mov FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+8)=0xe1a00030 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);	// mov FC_RETOP, FC_RETOP, lsr a2
			*(Bit32u*)(pos+12)=0xe1a00000;				// nop
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_SHRd:
			*(Bit32u*)pos=0xe1a00030 + (FC_RETOP << 12) + (HOST_a1) + (HOST_a2 << 8);	// mov FC_RETOP, a1, lsr a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_SARb:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (24 << 7);			// mov FC_RETOP, a1, lsl #24
			*(Bit32u*)(pos+4)=0xe1a00040 + (FC_RETOP << 12) + (FC_RETOP) + (24 << 7);		// mov FC_RETOP, FC_RETOP, asr #24
			*(Bit32u*)(pos+8)=0xe1a00050 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);	// mov FC_RETOP, FC_RETOP, asr a2
			*(Bit32u*)(pos+12)=0xe1a00000;				// nop
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_SARw:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (16 << 7);			// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+4)=0xe1a00040 + (FC_RETOP << 12) + (FC_RETOP) + (16 << 7);		// mov FC_RETOP, FC_RETOP, asr #16
			*(Bit32u*)(pos+8)=0xe1a00050 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);	// mov FC_RETOP, FC_RETOP, asr a2
			*(Bit32u*)(pos+12)=0xe1a00000;				// nop
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_SARd:
			*(Bit32u*)pos=0xe1a00050 + (FC_RETOP << 12) + (HOST_a1) + (HOST_a2 << 8);	// mov FC_RETOP, a1, asr a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_RORb:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (24 << 7);							// mov FC_RETOP, a1, lsl #24
			*(Bit32u*)(pos+4)=0xe1800020 + (FC_RETOP << 12) + (FC_RETOP << 16) + (FC_RETOP) + (8 << 7);		// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #8
			*(Bit32u*)(pos+8)=0xe1800020 + (FC_RETOP << 12) + (FC_RETOP << 16) + (FC_RETOP) + (16 << 7);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+12)=0xe1a00070 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);					// mov FC_RETOP, FC_RETOP, ror a2
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_RORw:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (16 << 7);							// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+4)=0xe1800020 + (FC_RETOP << 12) + (FC_RETOP << 16) + (FC_RETOP) + (16 << 7);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+8)=0xe1a00070 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);					// mov FC_RETOP, FC_RETOP, ror a2
			*(Bit32u*)(pos+12)=0xe1a00000;				// nop
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_RORd:
			*(Bit32u*)pos=0xe1a00070 + (FC_RETOP << 12) + (HOST_a1) + (HOST_a2 << 8);	// mov FC_RETOP, a1, ror a2
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		case t_ROLb:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (24 << 7);							// mov FC_RETOP, a1, lsl #24
			*(Bit32u*)(pos+4)=0xe2600000 + (HOST_a2 << 12) + (HOST_a2 << 16) + (32);						// rsb a2, a2, #32
			*(Bit32u*)(pos+8)=0xe1800020 + (FC_RETOP << 12) + (FC_RETOP << 16) + (FC_RETOP) + (8 << 7);		// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #8
			*(Bit32u*)(pos+12)=0xe1800020 + (FC_RETOP << 12) + (FC_RETOP << 16) + (FC_RETOP) + (16 << 7);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+16)=0xe1a00070 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);					// mov FC_RETOP, FC_RETOP, ror a2
			break;
		case t_ROLw:
			*(Bit32u*)pos=0xe1a00000 + (FC_RETOP << 12) + (HOST_a1) + (16 << 7);							// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+4)=0xe2600000 + (HOST_a2 << 12) + (HOST_a2 << 16) + (32);						// rsb a2, a2, #32
			*(Bit32u*)(pos+8)=0xe1800020 + (FC_RETOP << 12) + (FC_RETOP << 16) + (FC_RETOP) + (16 << 7);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+12)=0xe1a00070 + (FC_RETOP << 12) + (FC_RETOP) + (HOST_a2 << 8);					// mov FC_RETOP, FC_RETOP, ror a2
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_ROLd:
			*(Bit32u*)pos=0xe2600000 + (HOST_a2 << 12) + (HOST_a2 << 16) + (32);			// rsb a2, a2, #32
			*(Bit32u*)(pos+4)=0xe1a00070 + (FC_RETOP << 12) + (HOST_a1) + (HOST_a2 << 8);	// mov FC_RETOP, a1, ror a2
			*(Bit32u*)(pos+8)=0xe1a00000;				// nop
			*(Bit32u*)(pos+12)=0xe1a00000;				// nop
			*(Bit32u*)(pos+16)=0xe1a00000;				// nop
			break;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*(Bit32u*)pos=0xe2600000 + (FC_RETOP << 12) + (HOST_a1 << 16) + (0);	// rsb FC_RETOP, a1, #0
			*(Bit32u*)(pos+4)=0xea000000 + (2);			// b (pc+2*4)
			break;
		default:
			*(Bit32u*)(pos+12)=(Bit32u)fct_ptr;		// simple_func
			break;

	}
#else
	*(Bit32u*)(pos+12)=(Bit32u)fct_ptr;		// simple_func
#endif
}
#endif
