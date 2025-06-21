// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/* State Management */
	CASE_0F_MMX(0x77) // EMMS
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		setFPUTagEmpty();
		break;
	}

/* Data Movement */
	CASE_0F_MMX(0x6e) // MOVD Pq,Ed
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* rmrq = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			GetEArd;
			rmrq->ud.d0=*(uint32_t*)eard;
			rmrq->ud.d1=0;
		} else {
			GetEAa;
			rmrq->ud.d0=LoadMd(eaa);
			rmrq->ud.d1=0;
		}
		break;
	}
	CASE_0F_MMX(0x7e) // MOVD Ed,Pq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* rmrq = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			GetEArd;
			*(uint32_t*)eard=rmrq->ud.d0;
		} else {
			GetEAa;
			SaveMd(eaa,rmrq->ud.d0);
		}
		break;
	}
	CASE_0F_MMX(0x6f) // MOVQ Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			MMX_reg* src = reg_mmx[rm & 7];
			dest->q = src->q;
		} else {
			GetEAa;
			dest->q=LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0x7f) // MOVQ Qq,Pq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			MMX_reg* src = reg_mmx[rm & 7];
			src->q = dest->q;
		} else {
			GetEAa;
			SaveMq(eaa,dest->q);
		}
		break;
	}

/* Boolean Logic */
	CASE_0F_MMX(0xef) // PXOR Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			MMX_reg* src = reg_mmx[rm & 7];
			dest->q ^= src->q;
		} else {
			GetEAa;
			dest->q ^= LoadMq(eaa);
		}
		break;
	}

	CASE_0F_MMX(0xeb) // POR Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			MMX_reg* src = reg_mmx[rm & 7];
			dest->q |= src->q;
		} else {
			GetEAa;
			dest->q |= LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0xdb) // PAND Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			MMX_reg* src = reg_mmx[rm & 7];
			dest->q &= src->q;
		} else {
			GetEAa;
			dest->q &= LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0xdf) // PANDN Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		if (rm >= 0xc0) {
			MMX_reg* src = reg_mmx[rm & 7];
			dest->q = ~dest->q & src->q;
		} else {
			GetEAa;
			dest->q = ~dest->q & LoadMq(eaa);
		}
		break;
	}

/* Shift */
	CASE_0F_MMX(0xf1) // PSLLW Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		MMX_reg src;
		if (rm >= 0xc0) {
			src.q = reg_mmx[rm & 7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psllw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xd1) // PSRLW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psrlw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xe1) // PSRAW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psraw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x71) // PSLLW/PSRLW/PSRAW Pq,Ib
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        uint8_t op    = (rm >> 3) & 7;
	        uint8_t shift = Fetchb();
	        MMX_reg* dest = reg_mmx[rm & 7];
	        switch (op) {
	        case 0x06: // PSLLW
	        {
		        const auto dest_m = simde_m_from_int64(
		                static_cast<int64_t>(dest->q));
		        const auto res = simde_m_psllwi(dest_m, shift);
		        dest->q = static_cast<uint64_t>(simde_m_to_int64(res));
	        } break;
	        case 0x02: // PSRLW
	        {
		        const auto dest_m = simde_m_from_int64(
		                static_cast<int64_t>(dest->q));
		        const auto res = simde_m_psrlwi(dest_m, shift);
		        dest->q = static_cast<uint64_t>(simde_m_to_int64(res));
	        } break;
	        case 0x04: // PSRAW
		        const auto dest_m = simde_m_from_int64(
		                static_cast<int64_t>(dest->q));
		        const auto res = simde_m_psrawi(dest_m, shift);
		        dest->q = static_cast<uint64_t>(simde_m_to_int64(res));
		        break;
	        }
	        break;
        }
        CASE_0F_MMX(0xf2) // PSLLD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pslld(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xd2) // PSRLD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psrld(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xe2) // PSRAD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psrad(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x72) // PSLLD/PSRLD/PSRAD Pq,Ib
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        uint8_t op    = (rm >> 3) & 7;
	        uint8_t shift = Fetchb();
	        MMX_reg* dest = reg_mmx[rm & 7];
	        switch (op) {
	        case 0x06: // PSLLD
	        {
		        const auto dest_m = simde_m_from_int64(
		                static_cast<int64_t>(dest->q));
		        const auto res = simde_m_pslldi(dest_m, shift);
		        dest->q = static_cast<uint64_t>(simde_m_to_int64(res));
	        } break;
	        case 0x02: // PSRLD
	        {
		        const auto dest_m = simde_m_from_int64(
		                static_cast<int64_t>(dest->q));
		        const auto res = simde_m_psrldi(dest_m, shift);
		        dest->q = static_cast<uint64_t>(simde_m_to_int64(res));
	        } break;
	        case 0x04: // PSRAD
	        {
		        const auto dest_m = simde_m_from_int64(
		                static_cast<int64_t>(dest->q));
		        const auto res = simde_m_psradi(dest_m, shift);
		        dest->q = static_cast<uint64_t>(simde_m_to_int64(res));
	        } break;
	        }
	        break;
        }
        CASE_0F_MMX(0xf3) // PSLLQ Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psllq(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xd3) // PSRLQ Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psrlq(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x73) // PSLLQ/PSRLQ Pq,Ib
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        uint8_t shift = Fetchb();
	        MMX_reg* dest = reg_mmx[rm & 7];
	        if (shift > 63) {
		        dest->q = 0;
	        } else {
		        uint8_t op = rm & 0x20;
		        if (op) {
			        dest->q <<= shift;
		        } else {
			        dest->q >>= shift;
		        }
	        }
	        break;
        }

/* Math */
	CASE_0F_MMX(0xFC) // PADDB Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		MMX_reg src;
		if (rm >= 0xc0) {
			src.q = reg_mmx[rm & 7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xFD) // PADDW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xFE) // PADDD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xEC) // PADDSB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddsb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xED) // PADDSW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddsw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xDC) // PADDUSB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddusb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xDD) // PADDUSW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_paddusw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xF8) // PSUBB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xF9) // PSUBW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xFA) // PSUBD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xE8) // PSUBSB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubsb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xE9) // PSUBSW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubsw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xD8) // PSUBUSB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubusb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xD9) // PSUBUSW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_psubusw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));

	        break;
        }
        CASE_0F_MMX(0xE5) // PMULHW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pmulhw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xD5) // PMULLW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pmullw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0xF5) // PMADDWD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pmaddwd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }

/* Comparison */
	CASE_0F_MMX(0x74) // PCMPEQB Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		MMX_reg src;
		if (rm >= 0xc0) {
			src.q = reg_mmx[rm & 7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pcmpeqb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x75) // PCMPEQW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pcmpeqw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x76) // PCMPEQD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pcmpeqd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x64) // PCMPGTB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pcmpgtb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x65) // PCMPGTW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pcmpgtw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x66) // PCMPGTD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_pcmpgtd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }

/* Data Packing */
	CASE_0F_MMX(0x63) // PACKSSWB Pq,Qq
	{
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
		GetRM;
		MMX_reg* dest = lookupRMregMM[rm];
		MMX_reg src;
		if (rm >= 0xc0) {
			src.q = reg_mmx[rm & 7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_packsswb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x6B) // PACKSSDW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_packssdw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x67) // PACKUSWB Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_packuswb(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x68) // PUNPCKHBW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_punpckhbw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x69) // PUNPCKHWD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_punpckhwd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x6A) // PUNPCKHDQ Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_punpckhdq(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x60) // PUNPCKLBW Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_punpcklbw(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x61) // PUNPCKLWD Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_punpcklwd(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
        CASE_0F_MMX(0x62) // PUNPCKLDQ Pq,Qq
        {
	        if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) { //-V1037
		        goto illegal_opcode;
	        }
	        GetRM;
	        MMX_reg* dest = lookupRMregMM[rm];
	        MMX_reg src;
	        if (rm >= 0xc0) {
		        src.q = reg_mmx[rm & 7]->q;
	        } else {
		        GetEAa;
		        src.q = LoadMq(eaa);
	        }
	        const auto src_m = simde_m_from_int64(static_cast<int64_t>(src.q));
	        const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	        const auto res = simde_m_punpckldq(dest_m, src_m);
	        dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	        break;
        }
