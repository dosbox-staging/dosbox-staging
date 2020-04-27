/*
 *  Copyright (C) 2002-2009  The DOSBox Team
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

#include "dosbox.h"
#include "inout.h"
#include "mem.h"
#include "paging.h"
#include "glide.h"
#include "setup.h"
#include "vga.h"
#include "dos_inc.h" 		/* for Drives[] */
#include "../dos/drives.h"

#include <iomanip>
#include <sstream>
using namespace std;

#include "SDL.h"

#if defined (WIN32)
#include "SDL_syswm.h"
#include <windows.h>

#else // *nix
#include <dlfcn.h>

#include <dirent.h>
#include <errno.h>

#endif

extern void GFX_Stop(void);
extern void GFX_ResetScreen(void);
extern const char* RunningProgram;

// Macro to properly pass floating point values
#define F(a)		*((float*)&a)

#define SAFE_DELETE(p)	{ if(p) { delete p; p = NULL; } }
#define ALIGN(x, a) (((x)+(a)-1)&~((a)-1))

#define G_OK 	1
#define G_FAIL	0

// Print debug messages
#define LOG_GLIDE 0

void VFILE_Remove(const char *name);
static void process_msg(Bitu);

/** Global Variables **/
GLIDE_Block glide;

//  Pointers to loaded routines
static FncPointers FP;
static void ** fn_pt=NULL;

// Shared memory address
static Bit16u glsegment=0;
static Bit32u param[20];

// Pointer to return value
static PhysPt ret;
static Bit16u ret_value;

// Temporary texture buffer
static Bit32u texsize=0;
static void* texmem=NULL;

static HostPt hwnd=NULL;
static char lfbacc=0;

// Tomb Rider shadow hack
static Bit8u tomb = 0;
static FxI32 GrOriginLocation = 0;

#if defined (WIN32)
static HINSTANCE hdll=NULL;	//  Handle to glide2x lib file
#else
static void * hdll=NULL;
#endif

#if LOG_GLIDE
static int GLIDE_count[GLIDE_MAX+2];
#endif

static Bitu read_gl(Bitu port,Bitu iolen)
{
    Bitu r=ret_value;
#if LOG_GLIDE
    if(ret_value == G_OK)
	LOG_MSG("Glide:Port read. Return address: 0x%x, value: %d", ret, mem_readd(ret));
    else if(ret_value == G_FAIL)
	LOG_MSG("Glide:Port read. Return address: 0x%x, value: %d. Writing G_FAIL to port", ret, mem_readd(ret));
    else
	LOG_MSG("Glide:Port read. Returning %hu", ret_value);
#endif

    ret_value = ret_value >> 8;

    return r;
}

static void write_gl(Bitu port,Bitu val,Bitu iolen)
{
    ret = 0;
    ret_value = G_FAIL;
    FP.grFunction0 = NULL;

    // Allocate shared memory (80 bytes)
    if(val > GLIDE_MAX) {
	if(glsegment==0) {
	    glsegment=DOS_GetMemory(5);
#if LOG_GLIDE
	    LOG_MSG("Glide:Memory allocated at 0x%x (segment: %hu)", glsegment<<4, glsegment);
#endif
	}
	ret_value=glsegment;
	LOG_MSG("Glide:Activated");
	return;
    }

    // Process function parameters (80 bytes)
    MEM_BlockRead32(PhysMake(glsegment,0), param, 80);
    process_msg(val);

//  LOG_MSG("Glide:Function %s executed OK", grTable[val].name);
}

class GLIDE_PageHandler : public PageHandler {
private:
    PhysPt base_addr;	// LFB physical address
    HostPt lfb_addr;

public:

    FxU32 locked;
    GLIDE_PageHandler(HostPt addr, PhysPt phyaddr = GLIDE_LFB):base_addr(phyaddr),locked(0) {

	lfb_addr = addr-base_addr;
	if(addr == NULL) {
	    LOG_MSG("Glide:NULL address passed!");
	    lfb_addr = NULL;
	}
	flags=PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE;
	PAGING_UnlinkPages(base_addr>>12, GLIDE_PAGES);
	LOG_MSG("Glide:GLIDE_PageHandler installed at 0x%x (%d pages)", base_addr, GLIDE_PAGES);
    }

    ~GLIDE_PageHandler() {
	LOG_MSG("Glide:Resetting page handler at 0x%x", base_addr);
	PAGING_UnlinkPages(base_addr>>12, GLIDE_PAGES);
    }

    void SetLFBAddr(HostPt addr) {
	if(addr-base_addr != lfb_addr) {
	    lfb_addr=addr-base_addr;
#if LOG_GLIDE
	    LOG_MSG("Glide:LFB addr set to 0x%x (0x%x), clear TLB", lfb_addr, addr);
#endif
	    PAGING_UnlinkPages(base_addr>>12, GLIDE_PAGES);
	}
    }

    PhysPt GetPhysPt(void) {
	return base_addr;
    }

    Bitu readb(PhysPt addr) {
//	LOG_MSG("Glide:Read from 0x%p", lfb_addr+addr);
	return *(Bit8u *)(lfb_addr+addr);
    }

    Bitu readw(PhysPt addr) {
//	LOG_MSG("Glide:Read from 0x%p", lfb_addr+addr);
	return *(Bit16u *)(lfb_addr+addr);
    }

    Bitu readd(PhysPt addr) {
//	LOG_MSG("Glide:Read from 0x%p", lfb_addr+addr);
	return *(Bit32u *)(lfb_addr+addr);
    }

    void writeb(PhysPt addr,Bitu val) {
//	LOG_MSG("Glide:Write to 0x%p", lfb_addr+addr);
	*(Bit8u *)(lfb_addr+addr)=(Bit8u)val;
    }

    void writew(PhysPt addr,Bitu val) {
//	LOG_MSG("Glide:Write to 0x%p", lfb_addr+addr);
	*(Bit16u *)(lfb_addr+addr)=(Bit16u)val;
    }

    void writed(PhysPt addr,Bitu val) {
//	LOG_MSG("Glide:Write to 0x%p", lfb_addr+addr);
	*(Bit32u *)(lfb_addr+addr)=(Bit32u)val;
    }

    HostPt GetHostReadPt(Bitu phys_page) {
//	LOG_MSG("Glide:GetHostReadPt called with %d, returning 0x%x", phys_page, lfb_addr+(phys_page*MEM_PAGESIZE));
	return lfb_addr+(phys_page*MEM_PAGESIZE);
    }

    HostPt GetHostWritePt(Bitu phys_page) {
//	LOG_MSG("Glide:GetHostWritePt called with %d, returning 0x%x", phys_page, lfb_addr+(phys_page*MEM_PAGESIZE));
	return lfb_addr+(phys_page*MEM_PAGESIZE);
    }

};

class GLIDE: public Module_base {
private:
    AutoexecObject autoexecline;
    // Glide port
    Bitu glide_base;
    Bit8u *ovl_data;
public:
    GLIDE(Section* configuration):Module_base(configuration),glide_base(0),ovl_data(NULL) {
	Section_prop * section=static_cast<Section_prop *>(configuration);

	if(!section->Get_bool("glide")) return;
	std::string str = section->Get_string("lfb");
	lowcase(str);
	if(str == "none") {
	    LOG_MSG("Glide:Disabled LFB access");
	    lfbacc=0;
	} else if(str == "read") {
	    LOG_MSG("Glide:LFB access: read-only");
	    lfbacc=1;
	} else if(str == "write") {
	    LOG_MSG("Glide:LFB access: write-only");
	    lfbacc=2;
	} else {
	    LOG_MSG("Glide:LFB access: read-write");
	    lfbacc=3;
	}

	// Load glide2x.dll
#if defined(WIN32)
	hdll = LoadLibrary("glide2x.dll");
#elif defined(MACOSX)
	hdll = dlopen("libglide2x.dylib", RTLD_NOW);
#else
	hdll = dlopen("libglide2x.so", RTLD_NOW);
#endif

	if(!hdll) {
	    LOG_MSG("Glide:Unable to load glide2x library, glide emulation disabled");
	    return;
	}

	// Allocate some temporary space
	texmem = (void*)malloc(1024*768*2);
	if(texmem == NULL) {
	    LOG_MSG("Glide:Unable to allocate texture memory, glide disabled");
	    return;
	}

	// Load glide2x.ovl if possible, so it is available on the Z: drive
#if defined (WIN32)
	// Windows is simple, either glide2x.ovl is in current directory or in path and the search is case insensitive
	FILE * ovl = fopen("glide2x.ovl", "rb");
#else
	// Try a bit harder in *nix, perform case insensitive search and check /usr/share/dosbox as well
	DIR * pdir; struct dirent * pfile;
	FILE * ovl = NULL;

	if((pdir = opendir("./"))) {
	    while(pfile = readdir(pdir)) {
		if(!strcasecmp(pfile->d_name, "glide2x.ovl")) {
		    strcpy((char*)texmem, "./"); strcat((char*)texmem, pfile->d_name);
		    FILE * ovl = fopen((char*)texmem, "rb");
		}
	    }
	    closedir(pdir);
	}

	if((ovl == NULL) && ((pdir = opendir("/usr/share/dosbox/")))) {
	    while(pfile = readdir(pdir)) {
		if(!strcasecmp(pfile->d_name, "glide2x.ovl")) {
		    strcpy((char*)texmem, "/usr/share/dosbox/"); strcat((char*)texmem, pfile->d_name);
		    FILE * ovl = fopen((char*)texmem, "rb");
		}
	    }
	    closedir(pdir);
	}
#endif

	long ovl_size;

	if(ovl != NULL) {
	    fseek(ovl, 0, SEEK_END);
	    ovl_size=ftell(ovl);
	    ovl_data=(Bit8u*)malloc(ovl_size);
	    fseek(ovl, 0, SEEK_SET);
	    fread(ovl_data, sizeof(char), ovl_size, ovl);
	    fclose(ovl);
	}

#if LOG_GLIDE
	SDL_memset(GLIDE_count, 0, sizeof(GLIDE_count));
#endif

	// Allocate memory for dll pointers
	fn_pt = (void**)malloc(sizeof(void*)*(GLIDE_MAX+1));
	if(fn_pt == NULL) {
	    LOG_MSG("Glide:Unable to allocate memory, glide disabled");
	    free(texmem); texmem = NULL;
	    return;
	}
	for(int i=0; i<(GLIDE_MAX+1); i++) {
#if defined(WIN32)
	    ostringstream temp;
	    temp << "_" << grTable[i].name << "@" << (Bitu)grTable[i].parms;
	    fn_pt[i] = (void*)(GetProcAddress(hdll, temp.str().c_str()));
#else
	    fn_pt[i] = (void*)(dlsym(hdll, grTable[i].name));
#endif
#if LOG_GLIDE
	    if(fn_pt[i] == NULL) {
		LOG_MSG("Glide:Warning, unable to load %s from glide2x", grTable[i].name);
	    }
#endif
	}

	glide.lfb_pagehandler = new GLIDE_PageHandler((HostPt)texmem);
	glide_base=section->Get_hex("grport");

	IO_RegisterReadHandler(glide_base,read_gl,IO_MB);
	IO_RegisterWriteHandler(glide_base,write_gl,IO_MB);

	ostringstream temp;
	temp << "SET GLIDE=" << hex << glide_base << ends;

    	autoexecline.Install(temp.str());
	glide.splash = true;

#if defined (WIN32)
	// Get hwnd information
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if(SDL_GetWMInfo(&wmi)) {
	    hwnd = (HostPt)wmi.window;
	} else {
	    LOG_MSG("SDL:Error retrieving window information");
	}
#endif

	if(ovl_data)
	    VFILE_Register("GLIDE2X.OVL", ovl_data, ovl_size);
    }

    ~GLIDE() {
	if(glide.enabled) {
	    // void grGlideShutdown(void)
	    FP.grFunction0 = (pfunc0)fn_pt[_grGlideShutdown0];
	    if(FP.grFunction0) FP.grFunction0();
	    glide.enabled = false;
	}

	SAFE_DELETE(glide.lfb_pagehandler);
	if(fn_pt)
	    free(fn_pt); fn_pt = NULL;
	if(texmem)
	    free(texmem); texmem = NULL;

	if(glide_base) {
	    IO_FreeReadHandler(glide_base,IO_MB);
	    IO_FreeWriteHandler(glide_base,IO_MB);
	}

	if(hdll) {
#if defined (WIN32)
	    FreeLibrary(hdll);
#else
	    dlclose(hdll);
#endif
	    hdll = NULL;
	}

	VFILE_Remove("GLIDE2X.OVL");
	if(ovl_data) free(ovl_data);
    }
};

static GLIDE* test;
void GLIDE_ShutDown(Section* sec) {
    delete test;
}

void GLIDE_Init(Section* sec) {
    test = new GLIDE(sec);
    sec->AddDestroyFunction(&GLIDE_ShutDown,true);
}

void GLIDE_ResetScreen(bool update)
{
#if LOG_GLIDE
	LOG_MSG("Glide:ResetScreen");
#endif
	VGA_SetOverride(true);
	GFX_Stop();

	// OpenGlide will resize the window on it's own (using SDL)
	if(
#ifdef WIN32
	// dgVoodoo needs a little help :)
	// Most other wrappers render fullscreen by default
	  (GetProcAddress(hdll, "DispatchDosNT") != NULL) ||
#endif
	// and resize when mapper and/or GUI finish
	  update) {
	    SDL_SetVideoMode(glide.width,glide.height,0,
		(glide.fullscreen[0]?SDL_FULLSCREEN:0)|SDL_ANYFORMAT|SDL_SWSURFACE);
	}
}

static bool GetFileName(char * filename)
{
    localDrive  *ldp;
    Bit8u 	drive;
    char 	fullname[DOS_PATHLENGTH];

    // Get full path
    if(!DOS_MakeName(filename,fullname,&drive)) return false;

#if LOG_GLIDE
    LOG_MSG("Glide:Fullname: %s", fullname);
#endif

    // Get real system path
    ldp = dynamic_cast<localDrive*>(Drives[drive]);
    if(ldp == NULL) return false;

    ldp->GetSystemFilename(filename,fullname);
#if LOG_GLIDE
    LOG_MSG("Glide:System path: %s", filename);
#endif
    return true;
}

typedef FxBool (FX_CALL *pfxSplashInit)(FxU32 hWnd, FxU32 screenWidth, FxU32 screenHeight,
					FxU32 numColBuf, FxU32 numAuxBuf, GrColorFormat_t colorFormat);
typedef void (FX_CALL *pfxSplash)(float x, float y, float w, float h, FxU32 frameNumber);

static void grSplash(void)
{
#ifdef WIN32
    HINSTANCE dll = LoadLibrary("3dfxSpl2.dll");
    if(dll == NULL) {
	return;
    }

    pfxSplashInit fxSplashInit = (pfxSplashInit)GetProcAddress(dll, "_fxSplashInit@24");
    pfxSplash fxSplash = (pfxSplash)GetProcAddress(dll, "_fxSplash@20");

    if((fxSplashInit == NULL) || (fxSplash == NULL)) {
	FreeLibrary(dll);
	return;
    }

    fxSplashInit(0, glide.width, glide.height, 2, 1, GR_COLORFORMAT_ABGR);
    fxSplash(0, 0, glide.width, glide.height, 0);

    // Openglide does not restore this state
    FP.grFunction1i = (pfunc1i)fn_pt[_grSstOrigin4];
    if(FP.grFunction1i) {
	FP.grFunction1i(GrOriginLocation);
    }

    FreeLibrary(dll);
#endif
}

static void process_msg(Bitu value)
{
    GrLfbInfo_t   lfbinfo;
    DBGrLfbInfo_t dblfbinfo;

    GrTexInfo     texinfo;
    DBGrTexInfo   dbtexinfo;

    Gu3dfInfo     guinfo;
    DBGu3dfInfo   dbguinfo;

    GrVertex	  vertex[3];

    GrMipMapInfo * mipmap;

    // Temporary memory used in functions
    FxI32	* ilist = (FxI32*)texmem;
    FxU16	* ptr16 = (FxU16*)texmem;

    // Filename translation
    char	  filename[512];

    // Return value address
    ret = param[0];
    Bitu i = value;
    FxU32 j, k;

    if((i > GLIDE_MAX) || (fn_pt[i] == NULL)) {
	LOG_MSG("Glide:Invalid function pointer for call %s", (i > GLIDE_MAX) ? "(invalid)" : grTable[i].name);
	return;
    }

#if LOG_GLIDE
    LOG_MSG("Glide:Processing call %s (%d), return address: 0x%x", grTable[i].name, value, ret);
    GLIDE_count[i]++;
#endif

    switch (value) {

    case _grAADrawLine8:
	// void grAADrawLine(GrVertex *va, GrVertex *vb)
	FP.grFunction2p = (pfunc2p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	MEM_BlockRead32(param[2], &vertex[1], sizeof(GrVertex));
	FP.grFunction2p(&vertex[0], &vertex[1]);
	break;
    case _grAADrawPoint4:
	// void grAADrawPoint(GrVertex *p)
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	FP.grFunction1p(&vertex[0]);
	break;
    case _grAADrawPolygon12:
	// void grAADrawPolygon(int nVerts, const int ilist[], const GrVertex vlist[])
	FP.grFunction1i2p = (pfunc1i2p)fn_pt[i];
	i = sizeof(FxI32)*param[1];
	MEM_BlockRead32(param[2], ilist, i);

	// Find the number of vertices (?)
	k = 0;
	for(j = 0; j < param[1]; j++) {
	    if(ilist[j] > k)
		k = ilist[j];
	}
	k++;

	MEM_BlockRead32(param[3], ilist+i, sizeof(GrVertex)*k);
	FP.grFunction1i2p(param[1], ilist, ilist+i);
	break;
    case _grAADrawPolygonVertexList8:
	// void grAADrawPolygonVertexList(int nVerts, const GrVertex vlist[])
	FP.grFunction1i1p = (pfunc1i1p)fn_pt[i];
	MEM_BlockRead32(param[2], texmem, sizeof(GrVertex)*param[1]);
	FP.grFunction1i1p(param[1], texmem);
	break;
    case _grAADrawTriangle24:
	// void grAADrawTriangle(GrVertex *a, GrVertex *b, GrVertex *c,
	//	FxBool antialiasAB, FxBool antialiasBC, FxBool antialiasCA)
	FP.grFunction3p3i = (pfunc3p3i)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	MEM_BlockRead32(param[2], &vertex[1], sizeof(GrVertex));
	MEM_BlockRead32(param[3], &vertex[2], sizeof(GrVertex));
	FP.grFunction3p3i(&vertex[0], &vertex[1], &vertex[2], param[4], param[5], param[6]);
	break;
    case _grAlphaBlendFunction16:
	// void grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df,
	//		GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df)
	if(tomb == 1) {
	    if((FP.grFunction1i = (pfunc1i)fn_pt[_grConstantColorValue4]))
		FP.grFunction1i(0x7f000000);
	} else if(tomb == 2) {
	    param[5] = 0x42fe0000;
	    param[6] = 0;
	    if((FP.grFunction4f = (pfunc4f)fn_pt[_grConstantColorValue416]))
		FP.grFunction4f(F(param[5]), F(param[6]), F(param[6]), F(param[6]));
	}
	FP.grFunction4i = (pfunc4i)fn_pt[i];
	FP.grFunction4i(param[1], param[2], param[3], param[4]);
	break;
    case _grAlphaCombine20:
	// void grAlphaCombine(GrCombineFunction_t func, GrCombineFactor_t factor,
	//		GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
	FP.grFunction5i = (pfunc5i)fn_pt[i];
	FP.grFunction5i(param[1], param[2], param[3], param[4], param[5]);
	break;
    case _grAlphaControlsITRGBLighting4:
	// void grAlphaControlsITRGBLighting(FxBool enable)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grAlphaTestFunction4:
	// void grAlphaTestFunction(GrCmpFnc_t function)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grAlphaTestReferenceValue4:
	// void grAlphaTestReferenceValue(GrAlpha_t value)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grBufferClear12:
	// void grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth)
	FP.grFunction3i = (pfunc3i)fn_pt[i];
	FP.grFunction3i(param[1], param[2], param[3]);
	break;
    case _grBufferNumPending0:
	// int grBufferNumPending(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
    case _grBufferSwap4:
	// void grBufferSwap(int swap_interval)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grCheckForRoom4:
	// void grCheckForRoom(FxI32 n)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grChromakeyMode4:
	// void grChromakeyMode(GrChromakeyMode_t mode)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grChromakeyValue4:
	// void grChromakeyValue(GrColor_t value)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grClipWindow16:
	// void grClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy)
	FP.grFunction4i = (pfunc4i)fn_pt[i];
	FP.grFunction4i(param[1], param[2], param[3], param[4]);
	break;
    case _grColorCombine20:
	// void grColorCombine(GrCombineFunction_t func, GrCombineFactor_t factor,
	//    		GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
	FP.grFunction5i = (pfunc5i)fn_pt[i];
	FP.grFunction5i(param[1], param[2], param[3], param[4], param[5]);
	break;
    case _grColorMask8:
	// void grColorMask(FxBool rgb, FxBool alpha)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
    case _grConstantColorValue416:
	// void grConstantColorValue4(float a, float r, float g, float b)
	FP.grFunction4f = (pfunc4f)fn_pt[i];
	FP.grFunction4f(F(param[1]), F(param[2]), F(param[3]), F(param[4]));
	break;
    case _grConstantColorValue4:
	// void grConstantColorValue(GrColor_t color)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grCullMode4:
	// void grCullMode(GrCullMode_t mode)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grDepthBiasLevel4:
	// void grDepthBiasLevel(FxI16 level)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grDepthBufferFunction4:
	// void grDepthBufferFunction(GrCmpFnc_t func)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grDepthBufferMode4:
	// void grDepthBufferMode(GrDepthBufferMode_t mode)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grDepthMask4:
	// void grDepthMask(FxBool enable)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grDisableAllEffects0:
	// void grDisableAllEffects(void)
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _grDitherMode4:
	// void grDitherMode(GrDitherMode_t mode)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grDrawLine8:
	// void grDrawLine(const GrVertex *a, const GrVertex *b)
	FP.grFunction2p = (pfunc2p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	MEM_BlockRead32(param[2], &vertex[1], sizeof(GrVertex));
	FP.grFunction2p(&vertex[0], &vertex[1]);
	break;
    case _grDrawPlanarPolygon12:
	// void grDrawPlanarPolygon(int nVerts, int ilist[], const GrVertex vlist[])
	FP.grFunction1i2p = (pfunc1i2p)fn_pt[i];
	i = sizeof(FxI32)*param[1];
	MEM_BlockRead32(param[2], ilist, i);

	// Find the number of vertices (?)
	k = 0;
	for(j = 0; j < param[1]; j++) {
	    if(ilist[j] > k)
		k = ilist[j];
	}
	k++;

	MEM_BlockRead32(param[3], ilist+i, sizeof(GrVertex)*k);
	FP.grFunction1i2p(param[1], ilist, ilist+i);
	break;
    case _grDrawPlanarPolygonVertexList8:
	// void grDrawPlanarPolygonVertexList(int nVertices, const GrVertex vlist[])
	FP.grFunction1i1p = (pfunc1i1p)fn_pt[i];
	MEM_BlockRead32(param[2], texmem, sizeof(GrVertex)*param[1]);
	FP.grFunction1i1p(param[1], texmem);
	break;
    case _grDrawPoint4:
	// void grDrawPoint(const GrVertex *a)
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	FP.grFunction1p(&vertex[0]);
	break;
    case _grDrawPolygon12:
	// void grDrawPolygon(int nVerts, int ilist[], const GrVertex vlist[])
	FP.grFunction1i2p = (pfunc1i2p)fn_pt[i];
	i = sizeof(FxI32)*param[1];
	MEM_BlockRead32(param[2], ilist, i);

	// Find the number of vertices (?)
	k = 0;
	for(j = 0; j < param[1]; j++) {
	    if(ilist[j] > k)
		k = ilist[j];
	}
	k++;

	MEM_BlockRead32(param[3], ilist+i, sizeof(GrVertex)*k);
	FP.grFunction1i2p(param[1], ilist, ilist+i);
	break;
    case _grDrawPolygonVertexList8:
	// void grDrawPolygonVertexList(int nVerts, const GrVertex vlist[])
	FP.grFunction1i1p = (pfunc1i1p)fn_pt[i];
	MEM_BlockRead32(param[2], texmem, sizeof(GrVertex)*param[1]);	
	FP.grFunction1i1p(param[1], texmem);
	break;
    case _grDrawTriangle12:
	// void grDrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c)
	FP.grFunction3p = (pfunc3p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	MEM_BlockRead32(param[2], &vertex[1], sizeof(GrVertex));
	MEM_BlockRead32(param[3], &vertex[2], sizeof(GrVertex));
	FP.grFunction3p(&vertex[0], &vertex[1], &vertex[2]);
	break;
/*
    case _grErrorSetCallback4:
	// void grErrorSetCallback(void (*function)(const char *string, FxBool fatal))
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(*param[1]);
	break;
*/
    case _grFogColorValue4:
	// void grFogColorValue(GrColor_t value)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grFogMode4:
	// void grFogMode(GrFogMode_t mode)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grFogTable4:
	// void grFogTable(const GrFog_t grTable[GR_FOG_TABLE_SIZE])
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], texmem, sizeof(GrFog_t)*GR_FOG_TABLE_SIZE);
	FP.grFunction1p(texmem);
	break;
    case _grGammaCorrectionValue4:
	// void grGammaCorrectionValue(float value)
	FP.grFunction1f = (pfunc1f)fn_pt[i];
	FP.grFunction1f(F(param[1]));
	break;
    case _grGlideGetState4:
	// void grGlideGetState(GrState *state)
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], texmem, sizeof(GrState));
	FP.grFunction1p(texmem);
	MEM_BlockWrite32(param[1], texmem, sizeof(GrState));
	break;
    case _grGlideGetVersion4:
	// void grGlideGetVersion(char version[80])
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	FP.grFunction1p(filename);
	k = 0;
	do {
	    mem_writeb(param[1]++, filename[k++]);
	} while(filename[k] != '\0');
	mem_writeb(param[1], '\0');
	break;
    case _grGlideInit0:
	// void grGlideInit(void)
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	if(!strncasecmp(RunningProgram, "Tombub", 6)) tomb = 2;
	else if(!strncasecmp(RunningProgram, "Tomb", 4)) tomb = 1;
	else tomb = 0;

	// Set LFB pointer
	if(mem_readd(param[1]) == 0xFFFFFFFF) {		// Find LFB magic
	    mem_writed(param[1], glide.lfb_pagehandler->GetPhysPt());
	} else {
	    LOG_MSG("Glide:Detected incompatible guest ovl/dll!");
	}
	break;
    case _grGlideSetState4:
	// void grGlideSetState(const GrState *state)
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], texmem, sizeof(GrState));
	FP.grFunction1p(texmem);
	MEM_BlockWrite32(param[1], texmem, sizeof(GrState));
	break;
    case _grGlideShamelessPlug4:
	// void grGlideShamelessPlug(const FxBool on)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grGlideShutdown0:
	// void grGlideShutdown(void)
	if(glide.enabled) {
	    FP.grFunction0 = (pfunc0)fn_pt[_grSstWinClose0];
	    FP.grFunction0();
	    glide.enabled = false;
	    VGA_SetOverride(false);
	    GFX_ResetScreen();
	}
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	glide.splash = true;

#if LOG_GLIDE
	for(j=0;j<(GLIDE_MAX+1);j++) {
	    if(GLIDE_count[j]) {
		LOG_MSG("Glide:%6d calls function %s (%d)", GLIDE_count[j], grTable[j].name, j);
	    }
	}
	LOG_MSG("Glide: %d framebuffer locks (%d read, %d write)", GLIDE_count[_grLfbLock24],
	    GLIDE_count[GLIDE_MAX+1], GLIDE_count[_grLfbLock24] - GLIDE_count[GLIDE_MAX+1]);		
	SDL_memset(GLIDE_count, 0, sizeof(GLIDE_count));
#endif
	break;
    case _grHints8:
	// void grHints(GrHints_t type, FxU32 hintMask)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
    case _grLfbConstantAlpha4:
	// void grLfbConstantAlpha(GrAlpha_t alpha)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grLfbConstantDepth4:
	// void grLfbConstantDepth(FxU16 depth)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grLfbLock24:
	// FxBool grLfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode,
	//		GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t *info)
	FP.grRFunction5i1p = (prfunc5i1p)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}

	// Read parameters
	MEM_BlockRead32(param[6], &dblfbinfo, sizeof(DBGrLfbInfo_t));
	lfbinfo.size = sizeof(GrLfbInfo_t);
	lfbinfo.origin = dblfbinfo.origin;
	lfbinfo.lfbPtr = texmem;

	k = FXTRUE;
	j = param[1]&1; j++;

	if(glide.lfb_pagehandler) {

	    if(j&lfbacc) {
		// Lock the buffer
		k = FP.grRFunction5i1p(param[1], param[2], param[3], param[4], param[5], &lfbinfo);
		if(k == FXTRUE) {
		    glide.lfb_pagehandler->locked++;
		    dblfbinfo.writeMode = lfbinfo.writeMode;
		    dblfbinfo.strideInBytes = lfbinfo.strideInBytes;
		    dblfbinfo.lfbPtr = glide.lfb_pagehandler->GetPhysPt();
		    MEM_BlockWrite32(param[6], &dblfbinfo, sizeof(DBGrLfbInfo_t));
		} else {
		    LOG_MSG("Glide:LFB Lock failed!");
		}
	    } // else lock is faked (texmem used for read/write)

	    // Set LFB address for page handler
	    glide.lfb_pagehandler->SetLFBAddr((HostPt)lfbinfo.lfbPtr);

	    if(j == 1) {// Is a read-only lock
#if LOG_GLIDE
		LOG_MSG("Glide:Read-only lock. Real LFB is at 0x%p.", lfbinfo.lfbPtr);
		GLIDE_count[GLIDE_MAX+1]++;
#endif
	    } else {	// Is a write-only lock
#if LOG_GLIDE
		LOG_MSG("Glide:Write-only lock. Real LFB is at 0x%p.", lfbinfo.lfbPtr);
#endif
	    }
	} else { // Fail
	    LOG_MSG("Glide:Failed to install page handler!");
	    k = FXFALSE;
	}

	mem_writed(ret, k);
	ret_value = G_OK;
	break;
    case _grLfbReadRegion28:
	// FxBool grLfbReadRegion(GrBuffer_t src_buffer, FxU32 src_x, FxU32 src_y, FxU32 src_width,
	//		FxU32 src_height, FxU32 dst_stride, void *dst_data)
	FP.grRFunction6i1p = (prfunc6i1p)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction6i1p(param[1], param[2], param[3], param[4], param[5], param[6], ptr16));
	MEM_BlockWrite(param[7], ptr16, param[5]*param[6]);
	ret_value = G_OK;
	break;
    case _grLfbUnlock8:
	// FxBool grLfbUnlock(GrLock_t type, GrBuffer_t buffer)
	FP.grRFunction2i = (prfunc2i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle (%d)!", _grLfbUnlock8);
	    return;
	}
	k = FXTRUE;
	if(glide.lfb_pagehandler && glide.lfb_pagehandler->locked) {
	    k = FP.grRFunction2i(param[1], param[2]);
	    glide.lfb_pagehandler->locked--;
	}
	mem_writed(ret, k);
	ret_value = G_OK;
	break;
    case _grLfbWriteColorFormat4:
	// void grLfbWriteColorFormat(GrColorFormat_t colorFormat)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grLfbWriteColorSwizzle8:
	// void grLfbWriteColorSwizzle(FxBool swizzleBytes, FxBool swapWords)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
    case _grLfbWriteRegion32:
	// FxBool grLfbWriteRegion(GrBuffer_t dst_buffer, FxU32 dst_x, FxU32 dst_y,
	//	GrLfbSrcFmt_t src_format, FxU32 src_width, FxU32 src_height, FxU32 src_stride, void *src_data)
	FP.grRFunction7i1p = (prfunc7i1p)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}

	MEM_BlockRead(param[8], ptr16, param[6]*param[7]);
	mem_writed(ret, FP.grRFunction7i1p(param[1], param[2], param[3], param[4], param[5], param[6], param[7], ptr16));
	ret_value = G_OK;
	break;
    case _grRenderBuffer4:
	// void grRenderBuffer(GrBuffer_t buffer)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grResetTriStats0:
	// void grResetTriStats()
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _grSplash20:
	// void grSplash(float x, float y, float width, float height, FxU32 frame)
	FP.grFunction4f1i = (pfunc4f1i)fn_pt[i];
	FP.grFunction4f1i(F(param[1]), F(param[2]), F(param[3]), F(param[4]), param[5]);
	break;
/*
    case _grSstConfigPipeline12:
	//
	FP.grFunction3i = (pfunc3i)fn_pt[i];
	FP.grFunction3i();
	break;
*/
    case _grSstControl4:
	// FxBool grSstControl(FxU32 code)
	FP.grRFunction1i = (prfunc1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1i(param[1]));
	ret_value = G_OK;
	break;
    case _grSstIdle0:
	// void grSstIdle(void)
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _grSstIsBusy0:
	// FxBool grSstIsBusy(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
    case _grSstOrigin4:
	// void grSstOrigin(GrOriginLocation_t origin)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	GrOriginLocation = param[1];
	break;
    case _grSstPerfStats4:
	// void grSstPerfStats(GrSstPerfStats_t *pStats)
	FP.grFunction1p = (pfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], texmem, sizeof(GrSstPerfStats_t));
	FP.grFunction1p(texmem);
	MEM_BlockWrite32(param[1], texmem, sizeof(GrSstPerfStats_t));
	break;
    case _grSstQueryBoards4:
	// FxBool grSstQueryBoards(GrHwConfiguration *hwConfig)
	FP.grRFunction1p = (prfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], texmem, sizeof(GrHwConfiguration));
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1p(texmem));
	MEM_BlockWrite32(param[1], texmem, sizeof(GrHwConfiguration));
	ret_value = G_OK;
	break;
    case _grSstQueryHardware4:
	// FxBool grSstQueryHardware(GrHwConfiguration *hwConfig)
	FP.grRFunction1p = (prfunc1p)fn_pt[i];
	MEM_BlockRead32(param[1], texmem, sizeof(GrHwConfiguration));
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1p(texmem));
	MEM_BlockWrite32(param[1], texmem, sizeof(GrHwConfiguration));
	ret_value = G_OK;
	break;
    case _grSstResetPerfStats0:
	// void grSstResetPerfStats(void)
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _grSstScreenHeight0:
	// FxU32 grSstScreenHeight(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
    case _grSstScreenWidth0:
	// FxU32 grSstScreenWidth(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
    case _grSstSelect4:
	// void grSstSelect(int which_sst)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _grSstStatus0:
	// FxU32 grSstStatus(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
    case _grSstVRetraceOn0:
	// FxBool grSstVRetraceOn(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
/*
    case _grSstVidMode8:
	//
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i();
	break;
*/
    case _grSstVideoLine0:
	// FxU32 grSstVideoLine(void)
	FP.grRFunction0 = (prfunc0)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction0());
	ret_value = G_OK;
	break;
    case _grSstWinClose0:
	// void grSstWinClose(void)
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	if(glide.enabled) {
	    glide.enabled = false;
	    VGA_SetOverride(false);
	    GFX_ResetScreen();
	}
	break;
    case _grSstWinOpen28:
	// FxBool grSstWinOpen(FxU32 hwnd, GrScreenResolution_t res, GrScreenRefresh_t ref,
	//	GrColorFormat_t cformat, GrOriginLocation_t org_loc, int num_buffers, int num_aux_buffers)
	FP.grRFunction1p6i = (prfunc1p6i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}

	if(glide.enabled) {
	    LOG_MSG("Glide:grSstWinOpen called when glide is active!");
	    mem_writed(ret, FXFALSE);
	    ret_value = G_OK;
	    break;
	}

	// Check for successful memory map
	if(mem_readd(param[10]) == 0) {
	    LOG_MSG("Glide:LFB memory map failed, using default LFB address!");
	    // Write physical address instead, it can crash but it just might work
	    mem_writed(param[10], glide.lfb_pagehandler->GetPhysPt());
	}

	glide.enabled = true;
	glide.width = param[8];
	glide.height = param[9];
	GrOriginLocation = param[5];

	// Resize window and disable updates
	GLIDE_ResetScreen();

	k = FP.grRFunction1p6i(hwnd, param[2], param[3], param[4], param[5], param[6], param[7]);
	if(k == FXFALSE) {
	    LOG_MSG("Glide:grSstWinOpen failed!");
	    glide.enabled = false;
	    VGA_SetOverride(false);
	    GFX_ResetScreen();
	    mem_writed(ret, FXFALSE);
	    ret_value = G_OK;
	    break;
	}

	mem_writed(ret, k);
	if(glide.splash) {
	    grSplash();
	    glide.splash = false;
	}

	LOG_MSG("Glide:Resolution set to:%dx%d, LFB at 0x%x (linear: 0x%x)", glide.width, glide.height, glide.lfb_pagehandler->GetPhysPt(), mem_readd(param[10]));
	ret_value = G_OK;
	break;
    case _grTexCalcMemRequired16:
	// FxU32 grTexCalcMemRequired(GrLOD_t smallLod, GrLOD_t largeLod, GrAspectRatio_t aspect,
	//	    GrTextureFormat_t format)
	FP.grRFunction4i = (prfunc4i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction4i(param[1], param[2], param[3], param[4]));
	ret_value = G_OK;
	break;
    case _grTexClampMode12:
	// void grTexClampMode(GrChipID_t tmu, GrTextureClampMode_t sClampMode, GrTextureClampMode_t tClampMode)
	FP.grFunction3i = (pfunc3i)fn_pt[i];
	FP.grFunction3i(param[1], param[2], param[3]);
	break;
    case _grTexCombine28:
	// void grTexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function, GrCombineFactor_t rgb_factor,
	//	GrCombineFunction_t alpha_function, GrCombineFactor_t alpha_factor, FxBool rgb_invert,
	//	FxBool alpha_invert)
	FP.grFunction7i = (pfunc7i)fn_pt[i];
	FP.grFunction7i(param[1], param[2], param[3], param[4], param[5], param[6], param[7]);
	break;
    case _grTexCombineFunction8:
	// void grTexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
    case _grTexDetailControl16:
	// void grTexDetailControl(GrChipID_t tmu, int lodBias, FxU8 detailScale, float detailMax)
	FP.grFunction3i1f = (pfunc3i1f)fn_pt[i];
	FP.grFunction3i1f(param[1], param[2], param[3], F(param[4]));
	break;
    case _grTexDownloadMipMap16:
	// void grTexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
	FP.grRFunction1i1p = (prfunc1i1p)fn_pt[_grTexTextureMemRequired8];
	if(FP.grRFunction1i1p == NULL) {
	    LOG_MSG("Glide:Unable to get pointer to grTexTextureMemRequired");
	    return;
	}

	MEM_BlockRead32(param[4], &dbtexinfo, sizeof(DBGrTexInfo));

	texinfo.smallLod = dbtexinfo.smallLod;
 	texinfo.largeLod = dbtexinfo.largeLod;
	texinfo.aspectRatio = dbtexinfo.aspectRatio;
	texinfo.format = dbtexinfo.format;
	texinfo.data = NULL;

	texsize = FP.grRFunction1i1p(param[3], &texinfo);
	MEM_BlockRead(dbtexinfo.data, texmem, texsize);
	texinfo.data = texmem;

	FP.grFunction3i1p = (pfunc3i1p)fn_pt[i];
	FP.grFunction3i1p(param[1], param[2], param[3], &texinfo);
	break;
    case _grTexDownloadMipMapLevel32:
	// void grTexDownloadMipMapLevel(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod,
	//	GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format,
	//	FxU32 evenOdd, void *data)
	FP.grRFunction1i1p = (prfunc1i1p)fn_pt[_grTexTextureMemRequired8];
	if(FP.grRFunction1i1p == NULL) {
	    LOG_MSG("Glide:Unable to get pointer to grTexTextureMemRequired");
	    return;
	}

	texinfo.smallLod = param[3];
 	texinfo.largeLod = param[4];
	texinfo.aspectRatio = param[5];
	texinfo.format = param[6];
	texinfo.data = NULL;

	texsize = FP.grRFunction1i1p(param[7], &texinfo);
	MEM_BlockRead(param[8], texmem, texsize);

	FP.grFunction7i1p = (pfunc7i1p)fn_pt[i];
	FP.grFunction7i1p(param[1], param[2], param[3], param[4], param[5], param[6], param[7], texmem);
	break;
    case _grTexDownloadMipMapLevelPartial40:
	// FX_ENTRY void FX_CALL grTexDownloadMipMapLevelPartial(GrChipID_t tmu, FxU32 startAddress,
	//    GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format,
	//    FxU32 evenOdd, void *data, int start, int end);
	FP.grRFunction1i1p = (prfunc1i1p)fn_pt[_grTexTextureMemRequired8];
	if(FP.grRFunction1i1p == NULL) {
	    LOG_MSG("Glide:Unable to get pointer to grTexTextureMemRequired");
	    return;
	}

	texinfo.smallLod = param[3];
 	texinfo.largeLod = param[4];
	texinfo.aspectRatio = param[5];
	texinfo.format = param[6];
	texinfo.data = NULL;

	texsize = FP.grRFunction1i1p(param[7], &texinfo);
	MEM_BlockRead(param[8], texmem, texsize);

	FP.grFunction7i1p2i = (pfunc7i1p2i)fn_pt[i];
	FP.grFunction7i1p2i(param[1], param[2], param[3], param[4], param[5], param[6], param[7], texmem,
			    param[9], param[10]);
	break;
    case _grTexDownloadTable12:
	// void grTexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data)
	FP.grFunction2i1p = (pfunc2i1p)fn_pt[i];

	if(param[2] == GR_TEXTABLE_PALETTE)
	    MEM_BlockRead32(param[3], texmem, sizeof(GuTexPalette));
	else // GR_TEXTABLE_NCC0 or GR_TEXTABLE_NCC1
	    MEM_BlockRead32(param[3], texmem, sizeof(GuNccTable));

	FP.grFunction2i1p(param[1], param[2], texmem);
	break;
    case _grTexDownloadTablePartial20:
	// void grTexDownloadTablePartial(GrChipID_t tmu, GrTexTable_t type, void *data, int start, int end)
	FP.grFunction2i1p2i = (pfunc2i1p2i)fn_pt[i];

	if(param[2] == GR_TEXTABLE_PALETTE) {
	    MEM_BlockRead32(param[3], texmem, sizeof(GuTexPalette));
	    FP.grFunction2i1p2i(param[1], param[2], texmem, param[4], param[5]);
	} else { // GR_TEXTABLE_NCC0 or GR_TEXTABLE_NCC1
	    LOG_MSG("Glide:Downloading partial NCC tables is not supported!");
	}

	break;
    case _grTexFilterMode12:
	// void grTexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minFilterMode,
	//	GrTextureFilterMode_t magFilterMode)
	FP.grFunction3i = (pfunc3i)fn_pt[i];
	FP.grFunction3i(param[1], param[2], param[3]);
	break;
    case _grTexLodBiasValue8:
	// void grTexLodBiasValue(GrChipID_t tmu, float bias)
	FP.grFunction1i1f = (pfunc1i1f)fn_pt[i];
	FP.grFunction1i1f(param[1], F(param[2]));
	break;
    case _grTexMaxAddress4:
	// FxU32 grTexMaxAddress(GrChipID_t tmu)
	FP.grRFunction1i = (prfunc1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1i(param[1]));
	ret_value = G_OK;
	break;
    case _grTexMinAddress4:
	// FxU32 grTexMinAddress(GrChipID_t tmu)
	FP.grRFunction1i = (prfunc1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1i(param[1]));
	ret_value = G_OK;
	break;
    case _grTexMipMapMode12:
	// void grTexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend)
	FP.grFunction3i = (pfunc3i)fn_pt[i];
	FP.grFunction3i(param[1], param[2], param[3]);
	break;
    case _grTexMultibase8:
	// void grTexMultibase(GrChipID_t tmu, FxBool enable)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
/*
    case _grTexMultibaseAddress20:
	// void grTexMultibaseAddress(GrChipID_t tmu, GrTexBaseRange_t range, FxU32 startAddress,
	//	FxU32 evenOdd, GrTexInfo *info)
	FP.grFunction4i1p = (pfunc4i1p)fn_pt[i];

	// This is a bit more complicated since *info contains a pointer to data
	texinfo = (GrTexInfo*)param[5];
	data = (PhysPt)texinfo->data;			// Store for later reference
	texinfo->data = VIRTOREAL(data);
#if LOG_GLIDE
	if(log_func[value-_grAADrawLine8] == 1) {
	    LOG_MSG("Glide:Replacing pointer 0x%x with 0x%x in function %d", data, texinfo->data, _grTexMultibaseAddress20);
	    log_func[value-_grAADrawLine8] = 2;
	}
#endif
	FP.grFunction20(param[1], param[2], param[3], param[4], (int)texinfo);
	texinfo->data = (void*)data;			// Change the pointer back
	break;
*/
    case _grTexNCCTable8:
	// void grTexNCCTable(GrChipID_t tmu, GrNCCTable_t table)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
    case _grTexSource16:
	// void grTexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
	FP.grFunction3i1p = (pfunc3i1p)fn_pt[i];

	// Copy the data from DB struct
	MEM_BlockRead32(param[4], &dbtexinfo, sizeof(DBGrTexInfo));

	texinfo.smallLod = dbtexinfo.smallLod;
 	texinfo.largeLod = dbtexinfo.largeLod;
	texinfo.aspectRatio = dbtexinfo.aspectRatio;
	texinfo.format = dbtexinfo.format;

	FP.grFunction3i1p(param[1], param[2], param[3], &texinfo);
	break;
    case _grTexTextureMemRequired8:
	// FxU32 grTexTextureMemRequired(FxU32 evenOdd, GrTexInfo *info)
	FP.grRFunction1i1p = (prfunc1i1p)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}

	// Copy the data from DB struct
	MEM_BlockRead32(param[2], &dbtexinfo, sizeof(DBGrTexInfo));

	texinfo.smallLod = dbtexinfo.smallLod;
 	texinfo.largeLod = dbtexinfo.largeLod;
	texinfo.aspectRatio = dbtexinfo.aspectRatio;
	texinfo.format = dbtexinfo.format;

	mem_writed(ret, FP.grRFunction1i1p(param[1], &texinfo));

	ret_value = G_OK;
	break;
    case _grTriStats8:
	// void grTriStats(FxU32 *trisProcessed, FxU32 *trisDrawn)
	FP.grFunction2p = (pfunc2p)fn_pt[i];
	MEM_BlockRead32(param[1], ilist, sizeof(FxU32));
	MEM_BlockRead32(param[2], ilist + 1, sizeof(FxU32));
	FP.grFunction2p(ilist, ilist + 1);
	MEM_BlockWrite32(param[1], ilist, sizeof(FxU32));
	MEM_BlockWrite32(param[2], ilist + 1, sizeof(FxU32));
	break;
    case _gu3dfGetInfo8:
	// FxBool gu3dfGetInfo(const char *filename, Gu3dfInfo *info)
	FP.grRFunction2p = (prfunc2p)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	MEM_StrCopy(param[1], filename, 512);

#if LOG_GLIDE
	LOG_MSG("Glide:Filename info found in gu3dfGetInfo: %s", filename);
#endif
	if(!GetFileName(filename)) break;
	mem_writed(ret, FP.grRFunction2p(filename, &guinfo));

	// Copy the data back to DB struct if successful
	if(mem_readd(ret)) {
	    MEM_BlockRead32(param[2], &dbguinfo, sizeof(DBGu3dfInfo));
	    dbguinfo.header.width = (Bit32u)guinfo.header.width;
	    dbguinfo.header.height = (Bit32u)guinfo.header.height;
	    dbguinfo.header.small_lod = (Bit32s)guinfo.header.small_lod;
	    dbguinfo.header.large_lod = (Bit32s)guinfo.header.large_lod;
	    dbguinfo.header.aspect_ratio = (Bit32s)guinfo.header.aspect_ratio;
	    dbguinfo.header.format = (Bit32s)guinfo.header.format;
	    dbguinfo.mem_required = (Bit32u)guinfo.mem_required;
	    MEM_BlockWrite32(param[2], &dbguinfo, sizeof(DBGu3dfInfo));
	}

	ret_value = G_OK;
	break;
    case _gu3dfLoad8:
	// FxBool gu3dfLoad(const char *filename, Gu3dfInfo *info)
	FP.grRFunction2p = (prfunc2p)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}

	// Although glide ref specifies *info should be filled by gu3dfGetInfo before calling gu3dfLoad,
	// OpenGlide will re-read the header in gu3dfLoad as well
	MEM_BlockRead32(param[2], &dbguinfo, sizeof(DBGu3dfInfo));
	MEM_StrCopy(param[1], filename, 512);

#if LOG_GLIDE
	LOG_MSG("Glide:Filename info found in gu3dfLoad: %s", filename);
#endif
	if(!GetFileName(filename)) break;

	guinfo.data = texmem;
	mem_writed(ret, FP.grRFunction2p(filename, &guinfo));

	// Copy the data back to DB struct if successful
	if(mem_readd(ret)) {
	    for(j=0;j<256;j++)
		dbguinfo.table.palette.data[j] = (Bit32u)guinfo.table.palette.data[j];
	    MEM_BlockWrite(dbguinfo.data, guinfo.data, guinfo.mem_required);
	}

	MEM_BlockWrite32(param[2], &dbguinfo, sizeof(DBGu3dfInfo));
	ret_value = G_OK;
	break;
    case _guAADrawTriangleWithClip12:
	// void guAADrawTriangleWithClip(const GrVertex *va, const GrVertex *vb, const GrVertex *vc)
	FP.grFunction3p = (pfunc3p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	MEM_BlockRead32(param[2], &vertex[1], sizeof(GrVertex));
	MEM_BlockRead32(param[3], &vertex[2], sizeof(GrVertex));
	FP.grFunction3p(&vertex[0], &vertex[1], &vertex[2]);
	break;
    case _guAlphaSource4:
	// void guAlphaSource(GrAlphaSourceMode_t mode)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _guColorCombineFunction4:
	// void guColorCombineFunction(GrColorCombineFunction_t func)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _guDrawPolygonVertexListWithClip8:
	// void guDrawPolygonVertexListWithClip(int nverts, const GrVertex vlist[])
	FP.grFunction1i1p = (pfunc1i1p)fn_pt[i];
	MEM_BlockRead32(param[2], texmem, sizeof(GrVertex)*param[1]);
	FP.grFunction1i1p(param[1], texmem);
	break;
    case _guDrawTriangleWithClip12:
	// void guDrawTriangleWithClip(const GrVertex *va, const GrVertex *vb, const GrVertex *vc)
	FP.grFunction3p = (pfunc3p)fn_pt[i];
	MEM_BlockRead32(param[1], &vertex[0], sizeof(GrVertex));
	MEM_BlockRead32(param[2], &vertex[1], sizeof(GrVertex));
	MEM_BlockRead32(param[3], &vertex[2], sizeof(GrVertex));
	FP.grFunction3p(&vertex[0], &vertex[1], &vertex[2]);
	break;
/*
    case _guEncodeRLE1616:
	//
	FP.grFunction16 = (pfunc16)fn_pt[i];
	FP.grFunction16();
	break;
    case _guEndianSwapBytes4:
	//
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i();
	break;
    case _guEndianSwapWords4:
	//
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i();
	break;
*/
    case _guFogGenerateExp28:
	// void guFogGenerateExp2(GrFog_t fogTable[GR_FOG_TABLE_SIZE], float density)
	FP.grFunction1p1f = (pfunc1p1f)fn_pt[i];
	FP.grFunction1p1f(texmem, F(param[2]));
	MEM_BlockWrite32(param[1], texmem, GR_FOG_TABLE_SIZE*sizeof(GrFog_t));
	break;
    case _guFogGenerateExp8:
	// void guFogGenerateExp(GrFog_t fogTable[GR_FOG_TABLE_SIZE], float density)
	FP.grFunction1p1f = (pfunc1p1f)fn_pt[i];
	FP.grFunction1p1f(texmem, F(param[2]));
	MEM_BlockWrite32(param[1], texmem, GR_FOG_TABLE_SIZE*sizeof(GrFog_t));
	break;
    case _guFogGenerateLinear12:
	// void guFogGenerateLinear(GrFog_t fogTable[GR_FOG_TABLE_SIZE], float nearW, float farW)
	FP.grFunction1p2f = (pfunc1p2f)fn_pt[i];
	FP.grFunction1p2f(texmem, F(param[2]), F(param[3]));
	MEM_BlockWrite32(param[1], texmem, GR_FOG_TABLE_SIZE*sizeof(GrFog_t));
	break;
    case _guFogTableIndexToW4: {
	// float guFogTableIndexToW(int i)
	FP.grFFunction1i = (pffunc1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	float tmp = FP.grFFunction1i(param[1]);
	mem_writed(ret, *((Bit32u*)&tmp));
	ret_value = G_OK;
	break;
    }
/*
    case _guMPDrawTriangle12:
	//
	FP.grFunction3i = (pfunc3i)fn_pt[i];
	FP.grFunction3i();
	break;
    case _guMPInit0:
	//
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _guMPTexCombineFunction4:
	//
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i();
	break;
    case _guMPTexSource8:
	//
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i();
	break;
    case _guMovieSetName4:
	//
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i();
	break;
    case _guMovieStart0:
	//
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _guMovieStop0:
	//
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
*/
    case _guTexAllocateMemory60:
	// GrMipMapId_t guTexAllocateMemory(GrChipID_t tmu, FxU8 evenOddMask, int width, int height,
	//	    GrTextureFormat_t format, GrMipMapMode_t mmMode, GrLOD_t smallLod, GrLOD_t largeLod,
	//	    GrAspectRatio_t aspectRatio, GrTextureClampMode_t sClampMode,
	//	    GrTextureClampMode_t tClampMode, GrTextureFilterMode_t minFilterMode,
	//	    GrTextureFilterMode_t magFilterMode, float lodBias, FxBool lodBlend)
	FP.grRFunction13i1f1i = (prfunc13i1f1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction13i1f1i(param[1], param[2], param[3], param[4], param[5], param[6], param[7],
			    param[8], param[9], param[10], param[11], param[12], param[13], F(param[14]), param[15]));
	ret_value = G_OK;
	break;
    case _guTexChangeAttributes48:
	// FxBool guTexChangeAttributes(GrMipMapID_t mmid, int width, int height, GrTextureFormat_t format,
	//	    GrMipMapMode_t mmMode, GrLOD_t smallLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio,
	//	    GrTextureClampMode_t sClampMode, GrTextureClampMode_t tClampMode,
	//	    GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode)
	FP.grRFunction12i = (prfunc12i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction12i(param[1], param[2], param[3], param[4], param[5], param[6], param[7],
			param[8], param[9], param[10], param[11], param[12]));
	ret_value = G_OK;
	break;
    case _guTexCombineFunction8:
	// void guTexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t func)
	FP.grFunction2i = (pfunc2i)fn_pt[i];
	FP.grFunction2i(param[1], param[2]);
	break;
/*
    case _guTexCreateColorMipMap0:
	//
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
*/
    case _guTexDownloadMipMap12:
	// void guTexDownloadMipMap(GrMipMapId_t mmid, const void *src, const GuNccTable *nccTable)
	FP.grRPTFunction1i = (prptfunc1i)fn_pt[_guTexGetMipMapInfo4];
	mipmap = (GrMipMapInfo*)FP.grRPTFunction1i(param[1]);

	if(mipmap) {
	    texinfo.aspectRatio = mipmap->aspect_ratio;
	    texinfo.format      = mipmap->format;
	    texinfo.largeLod    = mipmap->lod_max;
	    texinfo.smallLod    = mipmap->lod_min;

	    FP.grRFunction1i1p = (prfunc1i1p)fn_pt[_grTexTextureMemRequired8];
	    if(FP.grRFunction1i1p == NULL) {
		LOG_MSG("Glide:Unable to get pointer to grTexTextureMemRequired");
		return;
	    }

	    texsize = FP.grRFunction1i1p(mipmap->odd_even_mask, &texinfo);

	    MEM_BlockRead(param[2], texmem, texsize);
	    MEM_BlockRead32(param[3], (Bit8u*)texmem+texsize, sizeof(GuNccTable));

	    FP.grFunction1i2p = (pfunc1i2p)fn_pt[i];
	    FP.grFunction1i2p(param[1], texmem, (Bit8u*)texmem+texsize);
	} else {
	    LOG_MSG("Glide:Unable to get GrMipMapInfo pointer");
	}
	break;
/*
    case _guTexDownloadMipMapLevel12:
	// void guTexDownloadMipMapLevel(GrMipMapId_t mmid, GrLOD_t lod, const void **src)
	FP.grFunction2i1p = (pfunc2i1p)fn_pt[i];
	FP.grFunction2i1p(param[1], param[2], *param[3]);
	break;
*/
    case _guTexGetCurrentMipMap4:
	// GrMipMapId_t guTexGetCurrentMipMap(GrChipID_t tmu)
	FP.grRFunction1i = (prfunc1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1i(param[1]));
	ret_value = G_OK;
	break;
/*
    case _guTexGetMipMapInfo4:
	// GrMipMapInfo *guTexGetMipMapInfo(GrMipMapId_t mmid)
	FP.grRFunction4 = (prfunc4)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction4(param[1]));
	ret_value = G_OK;
	break;
*/
    case _guTexMemQueryAvail4:
	// FxU32 guTexMemQueryAvail(GrChipID_t tmu)
	FP.grRFunction1i = (prfunc1i)fn_pt[i];
	if(ret == 0) {
	    LOG_MSG("Glide:Invalid return value handle for %s!", grTable[i].name);
	    return;
	}
	mem_writed(ret, FP.grRFunction1i(param[1]));
	ret_value = G_OK;
	break;
    case _guTexMemReset0:
	// void guTexMemReset(void)
	FP.grFunction0 = (pfunc0)fn_pt[i];
	FP.grFunction0();
	break;
    case _guTexSource4:
	// void guTexSource(GrMipMapId_t mmid)
	FP.grFunction1i = (pfunc1i)fn_pt[i];
	FP.grFunction1i(param[1]);
	break;
    case _ConvertAndDownloadRle64: {
	// void ConvertAndDownloadRle(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod,
	//				GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd,
	//				FxU8 *bm_data, long  bm_h, FxU32 u0, FxU32 v0, FxU32 width, FxU32 height,
	//				FxU32 dest_width, FxU32 dest_height, FxU16 *tlut)

#if LOG_GLIDE
	LOG_MSG("Glide: RLE width: %d, height: %d, bm_h: %d, u0: %d, v0: %d, dest %dx%d",
	    param[12], param[13], param[9], param[10], param[11], param[14], param[15]);
#endif

	FxU8 c;
	FxU32 scount = 0;
	FxU32 dcount = 0;

	FxU16 * src = ptr16 + param[14]*param[15];
	FxU32 offset = 4 + param[9];

	// Line offset (v0)
	for(j = 0; j < param[11]; j++ ) {
	    offset += mem_readb(param[8]+4+j);
	}

	// Write height lines
	for(k = 0; k < param[13]; k++) {

	    // Decode one RLE line
	    scount = offset;
	    while((c = mem_readb(param[8]+scount)) != 0xE0) {

		if(c > 0xE0) {
		    for(int count = 0; count < (c&0x1f); count++) {

			// tlut is FxU16*
			src[dcount] = mem_readw(param[16]+(mem_readb(param[8]+scount+1)<<1));
			dcount++;
		    }
		    scount += 2;

		} else {
		    src[dcount] = mem_readw(param[16]+(c<<1));
		    dcount++; scount++;
		}
	    }

	    // Copy Line into destination texture, offset u0
	    SDL_memcpy(ptr16 + (k*param[14]), src + param[10], sizeof(FxU16)*param[14]);
	    offset += mem_readb(param[8] + 4 + j++);
	    dcount = 0;
	}

	// One additional line
	if(param[13] < param[15])
	    SDL_memcpy(ptr16 + (k*param[14]), src + param[10], sizeof(FxU16)*param[14]);

	// Download decoded texture
	texinfo.smallLod = param[3];
	texinfo.largeLod = param[4];
	texinfo.aspectRatio = param[5];
	texinfo.format = param[6];
	texinfo.data = ptr16;

	// void grTexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
	FP.grFunction3i1p = (pfunc3i1p)fn_pt[_grTexDownloadMipMap16];
	if(FP.grFunction3i1p == NULL) {
	    LOG_MSG("Glide:Unable to get pointer to grTexDownloadMipMap");
	    break;
	}
	FP.grFunction3i1p(param[1], param[2], param[7], &texinfo);
	break;
    }
    default:
	LOG_MSG("Glide:Unsupported glide call %s", grTable[i].name);
	break;

    }	/* switch */
}	/* process_msg() */
