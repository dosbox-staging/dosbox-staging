#ifndef __LOGGING_H_
#define __LOGGING_H_
enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA, LOG_VGAGFX,LOG_VGAMISC,LOG_INT10,
	LOG_SB,LOG_DMA,
	LOG_FPU,LOG_CPU,
	LOG_FCB,LOG_FILES,LOG_IOCTL,LOG_EXEC,LOG_DOSMISC,
	LOG_PIT,LOG_KEYBOARD,LOG_PIC,
	LOG_MOUSE,LOG_BIOS,LOG_GUI,LOG_MISC,
	LOG_IO,
	LOG_MAX,
};

enum LOG_SEVERITIES {
	LOG_NORMAL,
    LOG_WARN,
	LOG_ERROR,
};

#if C_DEBUG
class LOG 
{ 
	LOG_TYPES		d_type;
	LOG_SEVERITIES	d_severity;
public:

	LOG (LOG_TYPES type , LOG_SEVERITIES severity):
		d_type(type),
		d_severity(severity)
		{}
	void operator() (char* buf, ...);  //../src/debug/debug_gui.cpp

};

void DEBUG_ShowMsg(char * format,...);
#define LOG_MSG DEBUG_ShowMsg

#else  //C_DEBUG

struct LOG
{
	LOG(LOG_TYPES type, LOG_SEVERITIES severity)				{ return;}
	void operator()(char* buf)									{ return;}
	void operator()(char* buf, double f1)						{ return;}
	void operator()(char* buf, double f1, Bit32u u1)			{ return;}
	void operator()(char* buf, Bitu u1, double f1)				{ return;}

	void operator()(char* buf, Bitu u1, Bitu u2)				{ return;}
	void operator()(char* buf, Bits u1, Bits u2)				{ return;}
	void operator()(char* buf, Bitu u1, Bits u2)				{ return;}
	void operator()(char* buf, Bits u1, Bitu u2)				{ return;}
	void operator()(char* buf, Bit32s u1)						{ return;}	
	void operator()(char* buf, Bit32u u1)						{ return;}
	void operator()(char* buf, Bits s1)							{ return;}
	void operator()(char* buf, Bitu u1)							{ return;}

	void operator()(char* buf, char* s1)						{ return;}
	void operator()(char* buf, char* s1, Bit32u u1)				{ return;}
	void operator()(char* buf, char* s1, Bit32u u1, Bit32u u2)	{ return;}
	void operator()(char* buf, Bit32u u1, char* s1)				{ return;}



}; //add missing operators to here
	//try to avoid anything smaller than bit32...
void GFX_ShowMsg(char * format,...);
#define LOG_MSG GFX_ShowMsg

#endif //C_DEBUG


#endif //__LOGGING_H_

