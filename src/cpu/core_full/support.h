enum {
	L_N=0,
	L_SKIP,
	/* Grouped ones using MOD/RM */
	L_MODRM,
	
	L_Ib,L_Iw,L_Id,
	L_Ibx,L_Iwx,L_Idx,				//Sign extend
	L_Ifw,L_Ifd,
	L_OP,

	L_REGb,L_REGw,L_REGd,
	L_REGbIb,L_REGwIw,L_REGdId,
	L_POPw,L_POPd,
	L_POPfw,L_POPfd,
	L_SEG,



	L_FLG,L_INTO,

	L_VAL,
	L_PRESEG,
	L_DOUBLE,
	L_PREOP,L_PREADD,L_PREREP,L_PREREPNE,
	L_STRING,

/* Direct ones */
	D_IRETw,D_IRETd,
	D_PUSHAw,D_PUSHAd,
	D_POPAw,D_POPAd,
	D_DAA,D_DAS,
	D_AAA,D_AAS,
	D_CBW,D_CWDE,
	D_CWD,D_CDQ,
	D_SETALC,
	D_XLATw,D_XLATd,
	D_CLI,D_STI,D_STC,D_CLC,D_CMC,D_CLD,D_STD,
	D_NOP,
	D_ENTERw,D_ENTERd,
	D_LEAVEw,D_LEAVEd,
	L_ERROR,

	D_RETFw,D_RETFd,
	D_RETFwIw,D_RETFdIw,
	D_CPUID,
};


enum {
	O_N=t_LASTFLAG,
	O_COND,
	O_XCHG_AX,O_XCHG_EAX,
	O_IMULRw,O_IMULRd,
	O_BOUNDw,O_BOUNDd,
	O_CALLNw,O_CALLNd,
	O_CALLFw,O_CALLFd,
	O_JMPFw,O_JMPFd,

	O_OPAL,O_ALOP,
	O_OPAX,O_AXOP,
	O_OPEAX,O_EAXOP,
	O_INT,
	O_SEGDS,O_SEGES,O_SEGFS,O_SEGGS,O_SEGSS,
	O_LOOP,O_LOOPZ,O_LOOPNZ,O_JCXZ,
	O_INb,O_INw,O_INd,
	O_OUTb,O_OUTw,O_OUTd,

	O_NOT,O_AAM,O_AAD,
	O_MULb,O_MULw,O_MULd,
	O_IMULb,O_IMULw,O_IMULd,
	O_DIVb,O_DIVw,O_DIVd,
	O_IDIVb,O_IDIVw,O_IDIVd,
	O_CBACK,


	O_DSHLw,O_DSHLd,
	O_DSHRw,O_DSHRd,
	O_C_O	,O_C_NO	,O_C_B	,O_C_NB	,O_C_Z	,O_C_NZ	,O_C_BE	,O_C_NBE,
	O_C_S	,O_C_NS	,O_C_P	,O_C_NP	,O_C_L	,O_C_NL	,O_C_LE	,O_C_NLE,

	O_GRP6w,O_GRP6d,
	O_GRP7w,O_GRP7d,
	O_M_Cd_Rd,O_M_Rd_Cd,
	O_LAR,O_LSL,
	O_ARPL,
	
	O_BTw,O_BTSw,O_BTRw,O_BTCw,
	O_BTd,O_BTSd,O_BTRd,O_BTCd,
	O_BSFw,O_BSRw,



};

enum {
	S_N=0,
	S_C_Eb,
	S_Eb,S_Gb,S_EbGb,
	S_Ew,S_Gw,S_EwGw,
	S_Ed,S_Gd,S_EdGd,

	
	S_REGb,S_REGw,S_REGd,
	S_PUSHw,S_PUSHd,
	S_SEGI,
	S_SEGm,
	S_SEGGw,S_SEGGd,

	
	S_AIPw,S_C_AIPw,
	S_AIPd,S_C_AIPd,

	S_FLGb,S_FLGw,S_FLGd,
	S_IP,S_IPIw,
};

enum {
	R_OUTSB,R_OUTSW,R_OUTSD,
	R_INSB,R_INSW,R_INSD,
	R_MOVSB,R_MOVSW,R_MOVSD,
	R_LODSB,R_LODSW,R_LODSD,
	R_STOSB,R_STOSW,R_STOSD,
	R_SCASB,R_SCASW,R_SCASD,
	R_CMPSB,R_CMPSW,R_CMPSD,
};

enum {
	M_None=0,
	M_Ebx,M_Eb,M_Gb,M_EbGb,M_GbEb,
	M_Ewx,M_Ew,M_Gw,M_EwGw,M_GwEw,M_EwxGwx,
	M_Edx,M_Ed,M_Gd,M_EdGd,M_GdEd,M_EdxGdx,
	
	M_EbIb,
	M_EwIw,M_EwIbx,M_EwxIbx,M_EwxIwx,M_EwGwIb,M_EwGwCL,
	M_EdId,M_EdIbx,M_EdGdIb,M_EdGdCL,
	
	M_Efw,M_Efd,
	
	M_Ib,M_Iw,M_Id,


	M_SEG,M_EA,
	M_GRP,
	M_GRP_Ib,M_GRP_CL,M_GRP_1,

	M_POPw,M_POPd,
};

struct OpCode {
	Bit8u load,op,save,extra;
};

static struct {
	Bitu entry;
	Bitu entry_default;
	Bitu rm;
	EAPoint rm_eaa;
	Bitu rm_off;
	Bitu rm_eai;
	Bitu rm_index;
	Bitu rm_mod;
	OpCode code;
	union {	
		Bit8u b;Bit8s bs;
		Bit16u w;Bit16s ws;
		Bit32u d;Bit32s ds;
	} op1,op2,imm;
	Bitu new_flags;
	struct {
		EAPoint base;
	} seg;
	Bitu cond;
	bool repz;
	Bitu prefix;
} inst;


#define PREFIX_NONE		0x0
#define PREFIX_SEG		0x1
#define PREFIX_ADDR		0x2
#define PREFIX_REP		0x4

