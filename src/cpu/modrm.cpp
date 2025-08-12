// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/cpu.h"

// clang-format off

uint8_t * lookupRMregb[]=
{
	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,

	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,

	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,

	&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,&reg_al,
	&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,&reg_cl,
	&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,&reg_dl,
	&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,&reg_bl,
	&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,&reg_ah,
	&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,&reg_ch,
	&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,&reg_dh,
	&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh,&reg_bh
};

uint16_t * lookupRMregw[]={
	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,

	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,

	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,

	&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,&reg_ax,
	&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,&reg_cx,
	&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,&reg_dx,
	&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,&reg_bx,
	&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,&reg_sp,
	&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,&reg_bp,
	&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,&reg_si,
	&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di,&reg_di
};

uint32_t * lookupRMregd[256]={
	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,

	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,

	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,

	&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,&reg_eax,
	&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,&reg_ecx,
	&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,&reg_edx,
	&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,&reg_ebx,
	&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,&reg_esp,
	&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,&reg_ebp,
	&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,&reg_esi,
	&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi,&reg_edi
};


uint8_t * lookupRMEAregb[256]={
/* 24 lines of 8*nullptr should give nice errors when used */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh,
	&reg_al,&reg_cl,&reg_dl,&reg_bl,&reg_ah,&reg_ch,&reg_dh,&reg_bh
};

uint16_t * lookupRMEAregw[256]={
/* 24 lines of 8*nullptr should give nice errors when used */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di,
	&reg_ax,&reg_cx,&reg_dx,&reg_bx,&reg_sp,&reg_bp,&reg_si,&reg_di
};

uint32_t * lookupRMEAregd[256]={
/* 24 lines of 8*nullptr should give nice errors when used */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi,
	&reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&reg_esp,&reg_ebp,&reg_esi,&reg_edi
};

// clang-format on
