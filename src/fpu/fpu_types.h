

enum { FPUREG_VALID=0, FPUREG_ZERO, FPUREG_PNAN, FPUREG_NNAN, FPUREG_EMPTY };

enum {
	t_FLD=0, t_FLDST, t_FDIV,
	t_FDIVP, t_FCHS, t_FCOMP,

	t_FUNKNOWN,
	t_FNOTDONE
};

struct FPU_Flag_Info { 
	struct {
		Real64 r;
		Bit8u tag;
	} var1,var2, result;
	struct {
		bool bf,c3,c2,c1,c0,ir,sf,pf,uf,of,zf,df,in;
		Bit8s tos;
	} sw;
	struct {
		bool ic,ie,sf,pf,uf,of,zf,df,in;
		Bit8u rc,pc;
	} cw;
	Bitu type;
	Bitu prev_type;
};

struct FPU_Reg {
	Real64 r;
	Bit8u tag;
};
