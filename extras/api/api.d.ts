
export interface CpuRegisters {
    eax:   number; ebx: number; ecx: number; edx: number;
    esi:   number; edi: number; ebp: number; esp: number;
    cs:    number; ds:  number; es:  number;
    ss:    number; fs:  number; gs:  number;
    eip:   number;
	flags: number;
}

export interface CpuResponse {
	registers: CpuRegisters;
}

export interface AllocateResponse {
    addr: number;
}

export interface MemoryWithAddr {
	addr: number;
	data: Uint8Array;
}

export interface MemoryResponse {
    memory: MemoryWithAddr;
	registers: CpuRegisters;
}

export interface DosInfo {
	listOfLists:      number;
	dosSwappableArea: number;
	firstShell:       number;
}

export interface DosBoxInfo {
	version: string;
	configHome: string;
	configWebserver: string;
}

export class DOSBoxApi {
    constructor(baseUrl?: string);
    getCpu(): Promise<CpuResponse>;
    readMem(segment: string | number, offset: string | number, len: string | number): Promise<Uint8Array>;
	readMem(offset: string | number, len: string | number): Promise<Uint8Array>;
    readMemAndCpu(segment: string | number, offset: string | number, len: string | number): Promise<MemoryResponse>;
	readMemAndCpu(offset: string | number, len: string | number): Promise<MemoryResponse>;
    writeMem(segment: string | number, offset: string | number, data: Uint8Array): Promise<void>;
	writeMem(offset: string | number, data: Uint8Array): Promise<void>;
	compareAndSwap(segment: string | number, offset: string | number, data: Uint8Array, expected: Uint8Array): Promise<Uint8Array?>;
	compareAndSwap(offset: string | number, data: Uint8Array, expected: Uint8Array): Promise<Uint8Array?>;
    alloc(sizeBytes: number, area?: 'conv' | 'UMA' | 'XMS', strategy?: 'best_fit' | 'first_fit' | 'last_fit'): Promise<AllocateResponse>;
    free(physicalAddress: number): Promise<void>;
    getDosInfo(): Promise<DosInfo>;
}
