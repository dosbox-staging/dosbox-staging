#ifndef DOSBOX_LOGGING_H
#define DOSBOX_LOGGING_H
enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA, LOG_VGAGFX,LOG_VGAMISC,LOG_INT10,
	LOG_SB,LOG_DMA,
	LOG_FPU,LOG_CPU,LOG_PAGING,
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
	LOG(LOG_TYPES type, LOG_SEVERITIES severity)										{ return;}
	void operator()(char const* buf)													{ return;}
	void operator()(char const* buf, double f1)											{ return;}
	void operator()(char const* buf, double f1, double f2)								{ return;}
	void operator()(char const* buf, double f1, double f2, double f3)					{ return;}
	void operator()(char const* buf, double f1, double f2, double f3, double f4)					{ return;}
	void operator()(char const* buf, double f1, double f2, double f3, double f4, double f5)					{ return;}

	void operator()(char const* buf, char const* s1)									{ return;}
	void operator()(char const* buf, char const* s1, double f1)							{ return;}
	void operator()(char const* buf, char const* s1, double f1,double f2)				{ return;}
	void operator()(char const* buf, double  f1, char const* s1)						{ return;}



}; //add missing operators to here
	//try to avoid anything smaller than bit32...
void GFX_ShowMsg(char * format,...);
#define LOG_MSG GFX_ShowMsg

#endif //C_DEBUG


#endif //DOSBOX_LOGGING_H
