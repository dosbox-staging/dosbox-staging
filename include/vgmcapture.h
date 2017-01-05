#ifndef VGMCapture_h
#define VGMCapture_h 1
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <memory>
#include <vector>
#include "dosbox.h"
#include "hardware.h"

typedef std::unique_ptr<class VGMCapture> PVGMCapture;
class VGMCapture {
private:
        struct VGMHeader {
                char id[4];
                uint32_t rofsEOF;
                uint32_t version;
                uint32_t clockSN76489;
                uint32_t clockYM2413;
                uint32_t rofsGD3;
                uint32_t samplesInFile;
                uint32_t rofsLoop;
                uint32_t samplesInLoop;
                uint32_t videoRefreshRate;
                uint16_t SNfeedback;
                uint8_t SNshiftRegisterWidth;
                uint8_t SNflags;
                uint32_t clockYM2612;
                uint32_t clockYM2151;
                uint32_t rofsData;
                uint32_t clockSegaPCM;
                uint32_t interfaceRegisterSegaPCM;
                uint32_t clockRF5C68;
                uint32_t clockYM2203;
                uint32_t clockYM2608;
                uint32_t clockYM2610;
                uint32_t clockYM3812;
                uint32_t clockYM3526;
                uint32_t clockY8950;
                uint32_t clockYMF262;
                uint32_t clockYMF278B;
                uint32_t clockYMF271;
                uint32_t clockYMZ280B;
                uint32_t clockRF5C164;
                uint32_t clockPWM;
                uint32_t clockAY8910;
                uint8_t typeAY8910;
                uint8_t flagsAY8910;
                uint8_t flagsAY8910_YM2203;
                uint8_t flagsAY8910_YM2608;
                uint8_t volumeModifier;
                uint8_t reserved1;
                uint8_t loopBase;
                uint8_t loopModifier;
                uint32_t clockGBdmg;
                uint32_t clockNESapu;
                uint32_t clockMultiPCM;
                uint32_t clockUPD7759;
                uint32_t clockOKIM6258;
                uint8_t flagsOKIM6258;
                uint8_t flagsK054539;
                uint8_t typeC140;
                uint8_t reserved2;
                uint32_t clockOKIM6295;
                uint32_t clockK051649;
                uint32_t clockK054539;
                uint32_t clockHuC6280;
                uint32_t clockC140;
                uint32_t clockK053260;
                uint32_t clockPokey;
                uint32_t clockQSound;
                uint32_t clockSCSP;
                uint32_t rofsExtraHeader;
                uint32_t clockWonderSwan;
                uint32_t clockVBvsu;
                uint32_t clockSAA1099;
                uint32_t clockES5503;
                uint32_t clockES5505;
                uint8_t channelsES5503;
                uint8_t channelsES5506;
                uint8_t clockDividerC352;
                uint8_t reserved3;
                uint32_t clockX1_010;
                uint32_t clockC352;
                uint32_t clockGA20;
                uint32_t reserved4[7];
        } __attribute__((packed)) header;
	struct VGMExtraHeader {
		uint32_t theSize;
		uint32_t rofsChpClock;
		uint32_t rofsChpVol;
		uint8_t  entryCount;
		uint8_t  chipID;
		uint8_t  flags;
		uint16_t volume;
	}__attribute__((packed)) extraHeader;
        bool OPL_used, SN_used, SAA_used, DAC_used, DMA_active, SPK_used;

        float lastTickCount, samplesPassedFraction;
        uint32_t totalSamples;

        FILE *handle;
        std::vector<uint8_t> buffer;

        uint8_t SN_latch, SN_regs[8][2], SN_previous;
	
	uint8_t SPK_PITmode;
	uint32_t SPK_period_current, SPK_period_wanted;
	bool SPK_clockGate;
	bool SPK_outputGate_current, SPK_outputGate_wanted;
	
	uint32_t streamTail;
	struct previousPCM {
		uint32_t start;
		std::vector<uint8_t> data;
	};
	std::vector<struct previousPCM> previousPCMs;
	
        void logTimeDifference(void);
	void SPK_enable(void);
	void SPK_update(void);
public:
        VGMCapture(FILE *theHandle);
        ~VGMCapture();
        void ioWrite_SN(uint8_t value, int (&cache)[8]);
        void ioWrite_OPL(bool chip, uint8_t index, uint8_t value, uint8_t (&cache)[512]);
	void ioWrite_SAA(bool chip, uint8_t index, uint8_t value, uint8_t (&cache)[2][32]);
	void ioWrite_DAC(uint8_t value);
	void DAC_startDMA(uint32_t rate, uint32_t length, uint8_t* data);
	void DAC_stopDMA(void);
	void SPK_setPeriod(uint32_t period, uint8_t mode);
	void SPK_setType(bool clockGate, bool output);

        OPL_Mode oplmode;
	bool DAC_allowed, SPK_allowed;
};

extern PVGMCapture vgmCapture;
#endif
