/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011, 2012, 2013 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "mt32emu.h"
#include "mmath.h"
#include "PartialManager.h"
#include "../save_state.h"

#if MT32EMU_USE_REVERBMODEL == 1
#include "AReverbModel.h"
#elif MT32EMU_USE_REVERBMODEL == 2
#include "BReverbModel.h"
#else
#include "FreeverbModel.h"
#endif
#include "DelayReverb.h"

namespace MT32Emu {

static const ControlROMMap ControlROMMaps[7] = {
	// ID    IDc IDbytes                     PCMmap  PCMc  tmbrA   tmbrAO, tmbrAC tmbrB   tmbrBO, tmbrBC tmbrR   trC  rhythm  rhyC  rsrv    panpot  prog    rhyMax  patMax  sysMax  timMax
	{0x4014, 22, "\000 ver1.04 14 July 87 ", 0x3000,  128, 0x8000, 0x0000, false, 0xC000, 0x4000, false, 0x3200,  30, 0x73A6,  85,  0x57C7, 0x57E2, 0x57D0, 0x5252, 0x525E, 0x526E, 0x520A},
	{0x4014, 22, "\000 ver1.05 06 Aug, 87 ", 0x3000,  128, 0x8000, 0x0000, false, 0xC000, 0x4000, false, 0x3200,  30, 0x7414,  85,  0x57C7, 0x57E2, 0x57D0, 0x5252, 0x525E, 0x526E, 0x520A},
	{0x4014, 22, "\000 ver1.06 31 Aug, 87 ", 0x3000,  128, 0x8000, 0x0000, false, 0xC000, 0x4000, false, 0x3200,  30, 0x7414,  85,  0x57D9, 0x57F4, 0x57E2, 0x5264, 0x5270, 0x5280, 0x521C},
	{0x4010, 22, "\000 ver1.07 10 Oct, 87 ", 0x3000,  128, 0x8000, 0x0000, false, 0xC000, 0x4000, false, 0x3200,  30, 0x73fe,  85,  0x57B1, 0x57CC, 0x57BA, 0x523C, 0x5248, 0x5258, 0x51F4}, // MT-32 revision 1
	{0x4010, 22, "\000verX.XX  30 Sep, 88 ", 0x3000,  128, 0x8000, 0x0000, false, 0xC000, 0x4000, false, 0x3200,  30, 0x741C,  85,  0x57E5, 0x5800, 0x57EE, 0x5270, 0x527C, 0x528C, 0x5228}, // MT-32 Blue Ridge mod
	{0x2205, 22, "\000CM32/LAPC1.00 890404", 0x8100,  256, 0x8000, 0x8000, false, 0x8080, 0x8000, false, 0x8500,  64, 0x8580,  85,  0x4F65, 0x4F80, 0x4F6E, 0x48A1, 0x48A5, 0x48BE, 0x48D5},
	{0x2205, 22, "\000CM32/LAPC1.02 891205", 0x8100,  256, 0x8000, 0x8000, true,  0x8080, 0x8000, true,  0x8500,  64, 0x8580,  85,  0x4F93, 0x4FAE, 0x4F9C, 0x48CB, 0x48CF, 0x48E8, 0x48FF}  // CM-32L
	// (Note that all but CM-32L ROM actually have 86 entries for rhythmTemp)
};

static inline Bit16s *streamOffset(Bit16s *stream, Bit32u pos) {
	return stream == NULL ? NULL : stream + pos;
}

static inline void clearIfNonNull(Bit16s *stream, Bit32u len) {
	if (stream != NULL) {
		memset(stream, 0, len * sizeof(Bit16s));
	}
}

static inline void mix(float *target, const float *stream, Bit32u len) {
	while (len--) {
		*target += *stream;
		stream++;
		target++;
	}
}

static inline void clearFloats(float *leftBuf, float *rightBuf, Bit32u len) {
	// FIXME: Use memset() where compatibility is guaranteed (if this turns out to be a win)
	while (len--) {
		*leftBuf++ = 0.0f;
		*rightBuf++ = 0.0f;
	}
}

static inline Bit16s clipBit16s(Bit32s a) {
	// Clamp values above 32767 to 32767, and values below -32768 to -32768
	if ((a + 32768) & ~65535) {
		return (a >> 31) ^ 32767;
	}
	return a;
}

static void floatToBit16s_nice(Bit16s *target, const float *source, Bit32u len, float outputGain) {
	float gain = outputGain * 16384.0f;
	while (len--) {
		// Since we're not shooting for accuracy here, don't worry about the rounding mode.
		*target = clipBit16s((Bit32s)(*source * gain));
		source++;
		target++;
	}
}

static void floatToBit16s_pure(Bit16s *target, const float *source, Bit32u len, float /*outputGain*/) {
	while (len--) {
		*target = clipBit16s((Bit32s)floor(*source * 8192.0f));
		source++;
		target++;
	}
}

static void floatToBit16s_reverb(Bit16s *target, const float *source, Bit32u len, float outputGain) {
	float gain = outputGain * 8192.0f;
	while (len--) {
		*target = clipBit16s((Bit32s)floor(*source * gain));
		source++;
		target++;
	}
}

static void floatToBit16s_generation1(Bit16s *target, const float *source, Bit32u len, float outputGain) {
	float gain = outputGain * 8192.0f;
	while (len--) {
		*target = clipBit16s((Bit32s)floor(*source * gain));
		*target = (*target & 0x8000) | ((*target << 1) & 0x7FFE);
		source++;
		target++;
	}
}

static void floatToBit16s_generation2(Bit16s *target, const float *source, Bit32u len, float outputGain) {
	float gain = outputGain * 8192.0f;
	while (len--) {
		*target = clipBit16s((Bit32s)floor(*source * gain));
		*target = (*target & 0x8000) | ((*target << 1) & 0x7FFE) | ((*target >> 14) & 0x0001);
		source++;
		target++;
	}
}

Bit8u Synth::calcSysexChecksum(const Bit8u *data, Bit32u len, Bit8u checksum) {
	for (unsigned int i = 0; i < len; i++) {
		checksum = checksum + data[i];
	}
	checksum = checksum & 0x7f;
	if (checksum) {
		checksum = 0x80 - checksum;
	}
	return checksum;
}

Synth::Synth(ReportHandler *useReportHandler) {
	isOpen = false;
	reverbEnabled = true;
	reverbOverridden = false;

	if (useReportHandler == NULL) {
		reportHandler = new ReportHandler;
		isDefaultReportHandler = true;
	} else {
		reportHandler = useReportHandler;
		isDefaultReportHandler = false;
	}

#if MT32EMU_USE_REVERBMODEL == 1
	reverbModels[REVERB_MODE_ROOM] = new AReverbModel(REVERB_MODE_ROOM);
	reverbModels[REVERB_MODE_HALL] = new AReverbModel(REVERB_MODE_HALL);
	reverbModels[REVERB_MODE_PLATE] = new AReverbModel(REVERB_MODE_PLATE);
	reverbModels[REVERB_MODE_TAP_DELAY] = new DelayReverb();
#elif MT32EMU_USE_REVERBMODEL == 2
	reverbModels[REVERB_MODE_ROOM] = new BReverbModel(REVERB_MODE_ROOM);
	reverbModels[REVERB_MODE_HALL] = new BReverbModel(REVERB_MODE_HALL);
	reverbModels[REVERB_MODE_PLATE] = new BReverbModel(REVERB_MODE_PLATE);
	reverbModels[REVERB_MODE_TAP_DELAY] = new BReverbModel(REVERB_MODE_TAP_DELAY);
#else
	reverbModels[REVERB_MODE_ROOM] = new FreeverbModel(0.76f, 0.687770909f, 0.63f, 0, 0.5f);
	reverbModels[REVERB_MODE_HALL] = new FreeverbModel(2.0f, 0.712025098f, 0.86f, 1, 0.5f);
	reverbModels[REVERB_MODE_PLATE] = new FreeverbModel(0.4f, 0.939522749f, 0.38f, 2, 0.05f);
	reverbModels[REVERB_MODE_TAP_DELAY] = new DelayReverb();
#endif

	reverbModel = NULL;
	setDACInputMode(DACInputMode_NICE);
	setOutputGain(1.0f);
	setReverbOutputGain(0.68f);
	partialManager = NULL;
	memset(parts, 0, sizeof(parts));
	renderedSampleCount = 0;

	partialLimit = MT32EMU_MAX_PARTIALS;
}

Synth::~Synth() {
	close(); // Make sure we're closed and everything is freed
	for (int i = 0; i < 4; i++) {
		delete reverbModels[i];
	}
	if (isDefaultReportHandler) {
		delete reportHandler;
	}
}

void ReportHandler::showLCDMessage(const char *data) {
	printf("WRITE-LCD: %s", data);
	printf("\n");
}

void ReportHandler::printDebug(const char *fmt, va_list list) {
		vprintf(fmt, list);
		printf("\n");
}

void Synth::partStateChanged(int partNum, bool isPartActive) {
	reportHandler->onPartStateChanged(partNum, isPartActive);
}

void Synth::polyStateChanged(int partNum) {
	reportHandler->onPolyStateChanged(partNum);
}

void Synth::partialStateChanged(const Partial * const partial, int oldPartialPhase, int newPartialPhase) {
	for (int i = 0; i < getPartialLimit(); i++) {
		if (getPartial(i) == partial) {
			reportHandler->onPartialStateChanged(i, oldPartialPhase, newPartialPhase);
			break;
		}
	}
}

void Synth::newTimbreSet(int partNum, char patchName[]) {
	reportHandler->onProgramChanged(partNum, patchName);
}

void Synth::printDebug(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
#if MT32EMU_DEBUG_SAMPLESTAMPS > 0
	reportHandler->printDebug("[%u] ", renderedSampleCount);
#endif
	reportHandler->printDebug(fmt, ap);
	va_end(ap);
}

void Synth::setReverbEnabled(bool newReverbEnabled) {
	reverbEnabled = newReverbEnabled;
}

bool Synth::isReverbEnabled() const {
	return reverbEnabled;
}

void Synth::setReverbOverridden(bool newReverbOverridden) {
	reverbOverridden = newReverbOverridden;
}

bool Synth::isReverbOverridden() const {
	return reverbOverridden;
}

void Synth::setDACInputMode(DACInputMode mode) {
	switch(mode) {
	case DACInputMode_GENERATION1:
		la32FloatToBit16sFunc = floatToBit16s_generation1;
		reverbFloatToBit16sFunc = floatToBit16s_reverb;
		break;
	case DACInputMode_GENERATION2:
		la32FloatToBit16sFunc = floatToBit16s_generation2;
		reverbFloatToBit16sFunc = floatToBit16s_reverb;
		break;
	case DACInputMode_PURE:
		la32FloatToBit16sFunc = floatToBit16s_pure;
		reverbFloatToBit16sFunc = floatToBit16s_pure;
		break;
	case DACInputMode_NICE:
	default:
		la32FloatToBit16sFunc = floatToBit16s_nice;
		reverbFloatToBit16sFunc = floatToBit16s_reverb;
		break;
	}
}

void Synth::setOutputGain(float newOutputGain) {
	outputGain = newOutputGain;
}

void Synth::setReverbOutputGain(float newReverbOutputGain) {
	reverbOutputGain = newReverbOutputGain;
}

bool Synth::loadControlROM(const ROMImage &controlROMImage) {
	if (&controlROMImage == NULL) return false;
	File *file = controlROMImage.getFile();
	const ROMInfo *controlROMInfo = controlROMImage.getROMInfo();
	if ((controlROMInfo == NULL)
			|| (controlROMInfo->type != ROMInfo::Control)
			|| (controlROMInfo->pairType != ROMInfo::Full)) {
		return false;
	}
#if MT32EMU_MONITOR_INIT
	printDebug("Found Control ROM: %s, %s", controlROMInfo->shortName, controlROMInfo->description);
#endif
	const Bit8u *fileData = file->getData();
	memcpy(controlROMData, fileData, CONTROL_ROM_SIZE);

	// Control ROM successfully loaded, now check whether it's a known type
	controlROMMap = NULL;
	for (unsigned int i = 0; i < sizeof(ControlROMMaps) / sizeof(ControlROMMaps[0]); i++) {
		if (memcmp(&controlROMData[ControlROMMaps[i].idPos], ControlROMMaps[i].idBytes, ControlROMMaps[i].idLen) == 0) {
			controlROMMap = &ControlROMMaps[i];
			return true;
		}
	}
#if MT32EMU_MONITOR_INIT
	printDebug("Control ROM failed to load");
#endif
	return false;
}

bool Synth::loadPCMROM(const ROMImage &pcmROMImage) {
	if (&pcmROMImage == NULL) return false;
	File *file = pcmROMImage.getFile();
	const ROMInfo *pcmROMInfo = pcmROMImage.getROMInfo();
	if ((pcmROMInfo == NULL)
			|| (pcmROMInfo->type != ROMInfo::PCM)
			|| (pcmROMInfo->pairType != ROMInfo::Full)) {
		return false;
	}
#if MT32EMU_MONITOR_INIT
	printDebug("Found PCM ROM: %s, %s", pcmROMInfo->shortName, pcmROMInfo->description);
#endif
	size_t fileSize = file->getSize();
	if (fileSize != (2 * pcmROMSize)) {
#if MT32EMU_MONITOR_INIT
		printDebug("PCM ROM file has wrong size (expected %d, got %d)", 2 * pcmROMSize, fileSize);
#endif
		return false;
	}
	const Bit8u *fileData = file->getData();
	for (size_t i = 0; i < pcmROMSize; i++) {
		Bit8u s = *(fileData++);
		Bit8u c = *(fileData++);

		int order[16] = {0, 9, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 8};

		signed short log = 0;
		for (int u = 0; u < 15; u++) {
			int bit;
			if (order[u] < 8) {
				bit = (s >> (7 - order[u])) & 0x1;
			} else {
				bit = (c >> (7 - (order[u] - 8))) & 0x1;
			}
			log = log | (short)(bit << (15 - u));
		}
		pcmROMData[i] = log;
	}
	return true;
}

bool Synth::initPCMList(Bit16u mapAddress, Bit16u count) {
	ControlROMPCMStruct *tps = (ControlROMPCMStruct *)&controlROMData[mapAddress];
	for (int i = 0; i < count; i++) {
		size_t rAddr = tps[i].pos * 0x800;
		size_t rLenExp = (tps[i].len & 0x70) >> 4;
		size_t rLen = 0x800 << rLenExp;
		if (rAddr + rLen > pcmROMSize) {
			printDebug("Control ROM error: Wave map entry %d points to invalid PCM address 0x%04X, length 0x%04X", i, rAddr, rLen);
			return false;
		}
		pcmWaves[i].addr = rAddr;
		pcmWaves[i].len = rLen;
		pcmWaves[i].loop = (tps[i].len & 0x80) != 0;
		pcmWaves[i].controlROMPCMStruct = &tps[i];
		//int pitch = (tps[i].pitchMSB << 8) | tps[i].pitchLSB;
		//bool unaffectedByMasterTune = (tps[i].len & 0x01) == 0;
		//printDebug("PCM %d: pos=%d, len=%d, pitch=%d, loop=%s, unaffectedByMasterTune=%s", i, rAddr, rLen, pitch, pcmWaves[i].loop ? "YES" : "NO", unaffectedByMasterTune ? "YES" : "NO");
	}
	return false;
}

bool Synth::initCompressedTimbre(int timbreNum, const Bit8u *src, unsigned int srcLen) {
	// "Compressed" here means that muted partials aren't present in ROM (except in the case of partial 0 being muted).
	// Instead the data from the previous unmuted partial is used.
	if (srcLen < sizeof(TimbreParam::CommonParam)) {
		return false;
	}
	TimbreParam *timbre = &mt32ram.timbres[timbreNum].timbre;
	timbresMemoryRegion->write(timbreNum, 0, src, sizeof(TimbreParam::CommonParam), true);
	unsigned int srcPos = sizeof(TimbreParam::CommonParam);
	unsigned int memPos = sizeof(TimbreParam::CommonParam);
	for (int t = 0; t < 4; t++) {
		if (t != 0 && ((timbre->common.partialMute >> t) & 0x1) == 0x00) {
			// This partial is muted - we'll copy the previously copied partial, then
			srcPos -= sizeof(TimbreParam::PartialParam);
		} else if (srcPos + sizeof(TimbreParam::PartialParam) >= srcLen) {
			return false;
		}
		timbresMemoryRegion->write(timbreNum, memPos, src + srcPos, sizeof(TimbreParam::PartialParam));
		srcPos += sizeof(TimbreParam::PartialParam);
		memPos += sizeof(TimbreParam::PartialParam);
	}
	return true;
}

bool Synth::initTimbres(Bit16u mapAddress, Bit16u offset, int count, int startTimbre, bool compressed) {
	const Bit8u *timbreMap = &controlROMData[mapAddress];
	for (Bit16u i = 0; i < count * 2; i += 2) {
		Bit16u address = (timbreMap[i + 1] << 8) | timbreMap[i];
		if (!compressed && (address + offset + sizeof(TimbreParam) > CONTROL_ROM_SIZE)) {
			printDebug("Control ROM error: Timbre map entry 0x%04x for timbre %d points to invalid timbre address 0x%04x", i, startTimbre, address);
			return false;
		}
		address += offset;
		if (compressed) {
			if (!initCompressedTimbre(startTimbre, &controlROMData[address], CONTROL_ROM_SIZE - address)) {
				printDebug("Control ROM error: Timbre map entry 0x%04x for timbre %d points to invalid timbre at 0x%04x", i, startTimbre, address);
				return false;
			}
		} else {
			timbresMemoryRegion->write(startTimbre, 0, &controlROMData[address], sizeof(TimbreParam), true);
		}
		startTimbre++;
	}
	return true;
}

bool Synth::open(const ROMImage &controlROMImage, const ROMImage &pcmROMImage) {
	if (isOpen) {
		return false;
	}
	prerenderReadIx = prerenderWriteIx = 0;
#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Constant Tables");
#endif
#if !MT32EMU_REDUCE_REVERB_MEMORY
	for (int i = 0; i < 4; i++) {
		reverbModels[i]->open(useProp.sampleRate);
	}
#endif

	// This is to help detect bugs
	memset(&mt32ram, '?', sizeof(mt32ram));

#if MT32EMU_MONITOR_INIT
	printDebug("Loading Control ROM");
#endif
	if (!loadControlROM(controlROMImage)) {
		printDebug("Init Error - Missing or invalid Control ROM image");
		reportHandler->onErrorControlROM();
		return false;
	}

	initMemoryRegions();

	// 512KB PCM ROM for MT-32, etc.
	// 1MB PCM ROM for CM-32L, LAPC-I, CM-64, CM-500
	// Note that the size below is given in samples (16-bit), not bytes
	pcmROMSize = controlROMMap->pcmCount == 256 ? 512 * 1024 : 256 * 1024;
	pcmROMData = new Bit16s[pcmROMSize];

#if MT32EMU_MONITOR_INIT
	printDebug("Loading PCM ROM");
#endif
	if (!loadPCMROM(pcmROMImage)) {
		printDebug("Init Error - Missing PCM ROM image");
		reportHandler->onErrorPCMROM();
		return false;
	}

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Timbre Bank A");
#endif
	if (!initTimbres(controlROMMap->timbreAMap, controlROMMap->timbreAOffset, 0x40, 0, controlROMMap->timbreACompressed)) {
		return false;
	}

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Timbre Bank B");
#endif
	if (!initTimbres(controlROMMap->timbreBMap, controlROMMap->timbreBOffset, 0x40, 64, controlROMMap->timbreBCompressed)) {
		return false;
	}

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Timbre Bank R");
#endif
	if (!initTimbres(controlROMMap->timbreRMap, 0, controlROMMap->timbreRCount, 192, true)) {
		return false;
	}

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Timbre Bank M");
#endif
	// CM-64 seems to initialise all bytes in this bank to 0.
	memset(&mt32ram.timbres[128], 0, sizeof(mt32ram.timbres[128]) * 64);

	partialManager = new PartialManager(this, parts);

	pcmWaves = new PCMWaveEntry[controlROMMap->pcmCount];

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising PCM List");
#endif
	initPCMList(controlROMMap->pcmTable, controlROMMap->pcmCount);

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Rhythm Temp");
#endif
	memcpy(mt32ram.rhythmTemp, &controlROMData[controlROMMap->rhythmSettings], controlROMMap->rhythmSettingsCount * 4);

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising Patches");
#endif
	for (Bit8u i = 0; i < 128; i++) {
		PatchParam *patch = &mt32ram.patches[i];
		patch->timbreGroup = i / 64;
		patch->timbreNum = i % 64;
		patch->keyShift = 24;
		patch->fineTune = 50;
		patch->benderRange = 12;
		patch->assignMode = 0;
		patch->reverbSwitch = 1;
		patch->dummy = 0;
	}

#if MT32EMU_MONITOR_INIT
	printDebug("Initialising System");
#endif
	// The MT-32 manual claims that "Standard pitch" is 442Hz.
	mt32ram.system.masterTune = 0x4A; // Confirmed on CM-64
	mt32ram.system.reverbMode = 0; // Confirmed
	mt32ram.system.reverbTime = 5; // Confirmed
	mt32ram.system.reverbLevel = 3; // Confirmed
	memcpy(mt32ram.system.reserveSettings, &controlROMData[controlROMMap->reserveSettings], 9); // Confirmed
	for (Bit8u i = 0; i < 9; i++) {
		// This is the default: {1, 2, 3, 4, 5, 6, 7, 8, 9}
		// An alternative configuration can be selected by holding "Master Volume"
		// and pressing "PART button 1" on the real MT-32's frontpanel.
		// The channel assignment is then {0, 1, 2, 3, 4, 5, 6, 7, 9}
		mt32ram.system.chanAssign[i] = i + 1;
	}
	mt32ram.system.masterVol = 100; // Confirmed
	refreshSystem();

	for (int i = 0; i < 9; i++) {
		MemParams::PatchTemp *patchTemp = &mt32ram.patchTemp[i];

		// Note that except for the rhythm part, these patch fields will be set in setProgram() below anyway.
		patchTemp->patch.timbreGroup = 0;
		patchTemp->patch.timbreNum = 0;
		patchTemp->patch.keyShift = 24;
		patchTemp->patch.fineTune = 50;
		patchTemp->patch.benderRange = 12;
		patchTemp->patch.assignMode = 0;
		patchTemp->patch.reverbSwitch = 1;
		patchTemp->patch.dummy = 0;

		patchTemp->outputLevel = 80;
		patchTemp->panpot = controlROMData[controlROMMap->panSettings + i];
		memset(patchTemp->dummyv, 0, sizeof(patchTemp->dummyv));
		patchTemp->dummyv[1] = 127;

		if (i < 8) {
			parts[i] = new Part(this, i);
			parts[i]->setProgram(controlROMData[controlROMMap->programSettings + i]);
		} else {
			parts[i] = new RhythmPart(this, i);
		}
	}

	// For resetting mt32 mid-execution
	mt32default = mt32ram;

	isOpen = true;
	isEnabled = false;

#if MT32EMU_MONITOR_INIT
	printDebug("*** Initialisation complete ***");
#endif
	return true;
}

void Synth::close() {
	if (!isOpen) {
		return;
	}

	delete partialManager;
	partialManager = NULL;

	for (int i = 0; i < 9; i++) {
		delete parts[i];
		parts[i] = NULL;
	}

	delete[] pcmWaves;
	delete[] pcmROMData;

	deleteMemoryRegions();

	for (int i = 0; i < 4; i++) {
		reverbModels[i]->close();
	}
	reverbModel = NULL;
	isOpen = false;
}

void Synth::playMsg(Bit32u msg) {
	// FIXME: Implement active sensing
	unsigned char code     = (unsigned char)((msg & 0x0000F0) >> 4);
	unsigned char chan     = (unsigned char)(msg & 0x00000F);
	unsigned char note     = (unsigned char)((msg & 0x00FF00) >> 8);
	unsigned char velocity = (unsigned char)((msg & 0xFF0000) >> 16);
	isEnabled = true;

	//printDebug("Playing chan %d, code 0x%01x note: 0x%02x", chan, code, note);

	char part = chantable[chan];
	if (part < 0 || part > 8) {
#if MT32EMU_MONITOR_MIDI > 0
		printDebug("Play msg on unreg chan %d (%d): code=0x%01x, vel=%d", chan, part, code, velocity);
#endif
		return;
	}
	playMsgOnPart(part, code, note, velocity);

	// This ensures minimum 1-sample delay between sequential MIDI events
	// Without this, a sequence of NoteOn and immediately succeeding NoteOff messages is always silent
	// Technically, it's also impossible to send events through the MIDI interface faster than about each millisecond
	prerender();
}

void Synth::playMsgOnPart(unsigned char part, unsigned char code, unsigned char note, unsigned char velocity) {
	Bit32u bend;

	//printDebug("Synth::playMsgOnPart(%02x, %02x, %02x, %02x)", part, code, note, velocity);
	switch (code) {
	case 0x8:
		//printDebug("Note OFF - Part %d", part);
		// The MT-32 ignores velocity for note off
		parts[part]->noteOff(note);
		break;
	case 0x9:
		//printDebug("Note ON - Part %d, Note %d Vel %d", part, note, velocity);
		if (velocity == 0) {
			// MIDI defines note-on with velocity 0 as being the same as note-off with velocity 40
			parts[part]->noteOff(note);
		} else {
			parts[part]->noteOn(note, velocity);
		}
		break;
	case 0xB: // Control change
		switch (note) {
		case 0x01:  // Modulation
			//printDebug("Modulation: %d", velocity);
			parts[part]->setModulation(velocity);
			break;
		case 0x06:
			parts[part]->setDataEntryMSB(velocity);
			break;
		case 0x07:  // Set volume
			//printDebug("Volume set: %d", velocity);
			parts[part]->setVolume(velocity);
			break;
		case 0x0A:  // Pan
			//printDebug("Pan set: %d", velocity);
			parts[part]->setPan(velocity);
			break;
		case 0x0B:
			//printDebug("Expression set: %d", velocity);
			parts[part]->setExpression(velocity);
			break;
		case 0x40: // Hold (sustain) pedal
			//printDebug("Hold pedal set: %d", velocity);
			parts[part]->setHoldPedal(velocity >= 64);
			break;

		case 0x62:
		case 0x63:
			parts[part]->setNRPN();
			break;
		case 0x64:
			parts[part]->setRPNLSB(velocity);
			break;
		case 0x65:
			parts[part]->setRPNMSB(velocity);
			break;

		case 0x79: // Reset all controllers
			//printDebug("Reset all controllers");
			parts[part]->resetAllControllers();
			break;

		case 0x7B: // All notes off
			//printDebug("All notes off");
			parts[part]->allNotesOff();
			break;

		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F:
			// CONFIRMED:Mok: A real LAPC-I responds to these controllers as follows:
			parts[part]->setHoldPedal(false);
			parts[part]->allNotesOff();
			break;

		default:
#if MT32EMU_MONITOR_MIDI > 0
			printDebug("Unknown MIDI Control code: 0x%02x - vel 0x%02x", note, velocity);
#endif
			break;
		}

		break;
	case 0xC: // Program change
		//printDebug("Program change %01x", note);
		parts[part]->setProgram(note);
		break;
	case 0xE: // Pitch bender
		bend = (velocity << 7) | (note);
		//printDebug("Pitch bender %02x", bend);
		parts[part]->setBend(bend);
		break;
	default:
#if MT32EMU_MONITOR_MIDI > 0
		printDebug("Unknown Midi code: 0x%01x - %02x - %02x", code, note, velocity);
#endif
		break;
	}

	//midiOutShortMsg(m_out, msg);
}

void Synth::playSysex(const Bit8u *sysex, Bit32u len) {
	if (len < 2) {
		printDebug("playSysex: Message is too short for sysex (%d bytes)", len);
	}
	if (sysex[0] != 0xF0) {
		printDebug("playSysex: Message lacks start-of-sysex (0xF0)");
		return;
	}
	// Due to some programs (e.g. Java) sending buffers with junk at the end, we have to go through and find the end marker rather than relying on len.
	Bit32u endPos;
	for (endPos = 1; endPos < len; endPos++) {
		if (sysex[endPos] == 0xF7) {
			break;
		}
	}
	if (endPos == len) {
		printDebug("playSysex: Message lacks end-of-sysex (0xf7)");
		return;
	}
	playSysexWithoutFraming(sysex + 1, endPos - 1);
}

void Synth::playSysexWithoutFraming(const Bit8u *sysex, Bit32u len) {
	if (len < 4) {
		printDebug("playSysexWithoutFraming: Message is too short (%d bytes)!", len);
		return;
	}
	if (sysex[0] != SYSEX_MANUFACTURER_ROLAND) {
		printDebug("playSysexWithoutFraming: Header not intended for this device manufacturer: %02x %02x %02x %02x", (int)sysex[0], (int)sysex[1], (int)sysex[2], (int)sysex[3]);
		return;
	}
	if (sysex[2] == SYSEX_MDL_D50) {
		printDebug("playSysexWithoutFraming: Header is intended for model D-50 (not yet supported): %02x %02x %02x %02x", (int)sysex[0], (int)sysex[1], (int)sysex[2], (int)sysex[3]);
		return;
	} else if (sysex[2] != SYSEX_MDL_MT32) {
		printDebug("playSysexWithoutFraming: Header not intended for model MT-32: %02x %02x %02x %02x", (int)sysex[0], (int)sysex[1], (int)sysex[2], (int)sysex[3]);
		return;
	}
	playSysexWithoutHeader(sysex[1], sysex[3], sysex + 4, len - 4);
}

void Synth::playSysexWithoutHeader(unsigned char device, unsigned char command, const Bit8u *sysex, Bit32u len) {
	if (device > 0x10) {
		// We have device ID 0x10 (default, but changeable, on real MT-32), < 0x10 is for channels
		printDebug("playSysexWithoutHeader: Message is not intended for this device ID (provided: %02x, expected: 0x10 or channel)", (int)device);
		return;
	}
	// This is checked early in the real devices (before any sysex length checks or further processing)
	// FIXME: Response to SYSEX_CMD_DAT reset with partials active (and in general) is untested.
	if ((command == SYSEX_CMD_DT1 || command == SYSEX_CMD_DAT) && sysex[0] == 0x7F) {
		reset();
		return;
	}
	if (len < 4) {
		printDebug("playSysexWithoutHeader: Message is too short (%d bytes)!", len);
		return;
	}
	unsigned char checksum = calcSysexChecksum(sysex, len - 1, 0);
	if (checksum != sysex[len - 1]) {
		printDebug("playSysexWithoutHeader: Message checksum is incorrect (provided: %02x, expected: %02x)!", sysex[len - 1], checksum);
		return;
	}
	len -= 1; // Exclude checksum
	switch (command) {
	case SYSEX_CMD_DAT:
		if (hasActivePartials()) {
			printDebug("playSysexWithoutHeader: Got SYSEX_CMD_DAT but partials are active - ignoring");
			// FIXME: We should send SYSEX_CMD_RJC in this case
			break;
		}
		// Deliberate fall-through
	case SYSEX_CMD_DT1:
		writeSysex(device, sysex, len);
		break;
	case SYSEX_CMD_RQD:
		if (hasActivePartials()) {
			printDebug("playSysexWithoutHeader: Got SYSEX_CMD_RQD but partials are active - ignoring");
			// FIXME: We should send SYSEX_CMD_RJC in this case
			break;
		}
		// Deliberate fall-through
	case SYSEX_CMD_RQ1:
		readSysex(device, sysex, len);
		break;
	default:
		printDebug("playSysexWithoutHeader: Unsupported command %02x", command);
		return;
	}
}

void Synth::readSysex(unsigned char /*device*/, const Bit8u * /*sysex*/, Bit32u /*len*/) const {
	// NYI
}

void Synth::writeSysex(unsigned char device, const Bit8u *sysex, Bit32u len) {
	Bit32u addr = (sysex[0] << 16) | (sysex[1] << 8) | (sysex[2]);
	addr = MT32EMU_MEMADDR(addr);
	sysex += 3;
	len -= 3;
	//printDebug("Sysex addr: 0x%06x", MT32EMU_SYSEXMEMADDR(addr));
	// NOTE: Please keep both lower and upper bounds in each check, for ease of reading

	// Process channel-specific sysex by converting it to device-global
	if (device < 0x10) {
#if MT32EMU_MONITOR_SYSEX > 0
		printDebug("WRITE-CHANNEL: Channel %d temp area 0x%06x", device, MT32EMU_SYSEXMEMADDR(addr));
#endif
		if (/*addr >= MT32EMU_MEMADDR(0x000000) && */addr < MT32EMU_MEMADDR(0x010000)) {
			int offset;
			if (chantable[device] == -1) {
#if MT32EMU_MONITOR_SYSEX > 0
				printDebug(" (Channel not mapped to a part... 0 offset)");
#endif
				offset = 0;
			} else if (chantable[device] == 8) {
#if MT32EMU_MONITOR_SYSEX > 0
				printDebug(" (Channel mapped to rhythm... 0 offset)");
#endif
				offset = 0;
			} else {
				offset = chantable[device] * sizeof(MemParams::PatchTemp);
#if MT32EMU_MONITOR_SYSEX > 0
				printDebug(" (Setting extra offset to %d)", offset);
#endif
			}
			addr += MT32EMU_MEMADDR(0x030000) + offset;
		} else if (/*addr >= MT32EMU_MEMADDR(0x010000) && */ addr < MT32EMU_MEMADDR(0x020000)) {
			addr += MT32EMU_MEMADDR(0x030110) - MT32EMU_MEMADDR(0x010000);
		} else if (/*addr >= MT32EMU_MEMADDR(0x020000) && */ addr < MT32EMU_MEMADDR(0x030000)) {
			int offset;
			if (chantable[device] == -1) {
#if MT32EMU_MONITOR_SYSEX > 0
				printDebug(" (Channel not mapped to a part... 0 offset)");
#endif
				offset = 0;
			} else if (chantable[device] == 8) {
#if MT32EMU_MONITOR_SYSEX > 0
				printDebug(" (Channel mapped to rhythm... 0 offset)");
#endif
				offset = 0;
			} else {
				offset = chantable[device] * sizeof(TimbreParam);
#if MT32EMU_MONITOR_SYSEX > 0
				printDebug(" (Setting extra offset to %d)", offset);
#endif
			}
			addr += MT32EMU_MEMADDR(0x040000) - MT32EMU_MEMADDR(0x020000) + offset;
		} else {
#if MT32EMU_MONITOR_SYSEX > 0
			printDebug(" Invalid channel");
#endif
			return;
		}
	}

	// Process device-global sysex (possibly converted from channel-specific sysex above)
	for (;;) {
		// Find the appropriate memory region
		const MemoryRegion *region = findMemoryRegion(addr);

		if (region == NULL) {
			printDebug("Sysex write to unrecognised address %06x, len %d", MT32EMU_SYSEXMEMADDR(addr), len);
			break;
		}
		writeMemoryRegion(region, addr, region->getClampedLen(addr, len), sysex);

		Bit32u next = region->next(addr, len);
		if (next == 0) {
			break;
		}
		addr += next;
		sysex += next;
		len -= next;
	}
}

void Synth::readMemory(Bit32u addr, Bit32u len, Bit8u *data) {
	const MemoryRegion *region = findMemoryRegion(addr);
	if (region != NULL) {
		readMemoryRegion(region, addr, len, data);
	}
}

void Synth::initMemoryRegions() {
	// Timbre max tables are slightly more complicated than the others, which are used directly from the ROM.
	// The ROM (sensibly) just has maximums for TimbreParam.commonParam followed by just one TimbreParam.partialParam,
	// so we produce a table with all partialParams filled out, as well as padding for PaddedTimbre, for quick lookup.
	paddedTimbreMaxTable = new Bit8u[sizeof(MemParams::PaddedTimbre)];
	memcpy(&paddedTimbreMaxTable[0], &controlROMData[controlROMMap->timbreMaxTable], sizeof(TimbreParam::CommonParam) + sizeof(TimbreParam::PartialParam)); // commonParam and one partialParam
	int pos = sizeof(TimbreParam::CommonParam) + sizeof(TimbreParam::PartialParam);
	for (int i = 0; i < 3; i++) {
		memcpy(&paddedTimbreMaxTable[pos], &controlROMData[controlROMMap->timbreMaxTable + sizeof(TimbreParam::CommonParam)], sizeof(TimbreParam::PartialParam));
		pos += sizeof(TimbreParam::PartialParam);
	}
	memset(&paddedTimbreMaxTable[pos], 0, 10); // Padding
	patchTempMemoryRegion = new PatchTempMemoryRegion(this, (Bit8u *)&mt32ram.patchTemp[0], &controlROMData[controlROMMap->patchMaxTable]);
	rhythmTempMemoryRegion = new RhythmTempMemoryRegion(this, (Bit8u *)&mt32ram.rhythmTemp[0], &controlROMData[controlROMMap->rhythmMaxTable]);
	timbreTempMemoryRegion = new TimbreTempMemoryRegion(this, (Bit8u *)&mt32ram.timbreTemp[0], paddedTimbreMaxTable);
	patchesMemoryRegion = new PatchesMemoryRegion(this, (Bit8u *)&mt32ram.patches[0], &controlROMData[controlROMMap->patchMaxTable]);
	timbresMemoryRegion = new TimbresMemoryRegion(this, (Bit8u *)&mt32ram.timbres[0], paddedTimbreMaxTable);
	systemMemoryRegion = new SystemMemoryRegion(this, (Bit8u *)&mt32ram.system, &controlROMData[controlROMMap->systemMaxTable]);
	displayMemoryRegion = new DisplayMemoryRegion(this);
	resetMemoryRegion = new ResetMemoryRegion(this);
}

void Synth::deleteMemoryRegions() {
	delete patchTempMemoryRegion;
	patchTempMemoryRegion = NULL;
	delete rhythmTempMemoryRegion;
	rhythmTempMemoryRegion = NULL;
	delete timbreTempMemoryRegion;
	timbreTempMemoryRegion = NULL;
	delete patchesMemoryRegion;
	patchesMemoryRegion = NULL;
	delete timbresMemoryRegion;
	timbresMemoryRegion = NULL;
	delete systemMemoryRegion;
	systemMemoryRegion = NULL;
	delete displayMemoryRegion;
	displayMemoryRegion = NULL;
	delete resetMemoryRegion;
	resetMemoryRegion = NULL;

	delete[] paddedTimbreMaxTable;
	paddedTimbreMaxTable = NULL;
}

MemoryRegion *Synth::findMemoryRegion(Bit32u addr) {
	MemoryRegion *regions[] = {
		patchTempMemoryRegion,
		rhythmTempMemoryRegion,
		timbreTempMemoryRegion,
		patchesMemoryRegion,
		timbresMemoryRegion,
		systemMemoryRegion,
		displayMemoryRegion,
		resetMemoryRegion,
		NULL
	};
	for (int pos = 0; regions[pos] != NULL; pos++) {
		if (regions[pos]->contains(addr)) {
			return regions[pos];
		}
	}
	return NULL;
}

void Synth::readMemoryRegion(const MemoryRegion *region, Bit32u addr, Bit32u len, Bit8u *data) {
	unsigned int first = region->firstTouched(addr);
	//unsigned int last = region->lastTouched(addr, len);
	unsigned int off = region->firstTouchedOffset(addr);
	len = region->getClampedLen(addr, len);

	unsigned int m;

	if (region->isReadable()) {
		region->read(first, off, data, len);
	} else {
		// FIXME: We might want to do these properly in future
		for (m = 0; m < len; m += 2) {
			data[m] = 0xff;
			if (m + 1 < len) {
				data[m+1] = (Bit8u)region->type;
			}
		}
	}
}

void Synth::writeMemoryRegion(const MemoryRegion *region, Bit32u addr, Bit32u len, const Bit8u *data) {
	unsigned int first = region->firstTouched(addr);
	unsigned int last = region->lastTouched(addr, len);
	unsigned int off = region->firstTouchedOffset(addr);
	switch (region->type) {
	case MR_PatchTemp:
		region->write(first, off, data, len);
		//printDebug("Patch temp: Patch %d, offset %x, len %d", off/16, off % 16, len);

		for (unsigned int i = first; i <= last; i++) {
			int absTimbreNum = mt32ram.patchTemp[i].patch.timbreGroup * 64 + mt32ram.patchTemp[i].patch.timbreNum;
			char timbreName[11];
			memcpy(timbreName, mt32ram.timbres[absTimbreNum].timbre.common.name, 10);
			timbreName[10] = 0;
#if MT32EMU_MONITOR_SYSEX > 0
			printDebug("WRITE-PARTPATCH (%d-%d@%d..%d): %d; timbre=%d (%s), outlevel=%d", first, last, off, off + len, i, absTimbreNum, timbreName, mt32ram.patchTemp[i].outputLevel);
#endif
			if (parts[i] != NULL) {
				if (i != 8) {
					// Note: Confirmed on CM-64 that we definitely *should* update the timbre here,
					// but only in the case that the sysex actually writes to those values
					if (i == first && off > 2) {
#if MT32EMU_MONITOR_SYSEX > 0
						printDebug(" (Not updating timbre, since those values weren't touched)");
#endif
					} else {
						parts[i]->setTimbre(&mt32ram.timbres[parts[i]->getAbsTimbreNum()].timbre);
					}
				}
				parts[i]->refresh();
			}
		}
		break;
	case MR_RhythmTemp:
		region->write(first, off, data, len);
		for (unsigned int i = first; i <= last; i++) {
			int timbreNum = mt32ram.rhythmTemp[i].timbre;
			char timbreName[11];
			if (timbreNum < 94) {
				memcpy(timbreName, mt32ram.timbres[128 + timbreNum].timbre.common.name, 10);
				timbreName[10] = 0;
			} else {
				strcpy(timbreName, "[None]");
			}
#if MT32EMU_MONITOR_SYSEX > 0
			printDebug("WRITE-RHYTHM (%d-%d@%d..%d): %d; level=%02x, panpot=%02x, reverb=%02x, timbre=%d (%s)", first, last, off, off + len, i, mt32ram.rhythmTemp[i].outputLevel, mt32ram.rhythmTemp[i].panpot, mt32ram.rhythmTemp[i].reverbSwitch, mt32ram.rhythmTemp[i].timbre, timbreName);
#endif
		}
		if (parts[8] != NULL) {
			parts[8]->refresh();
		}
		break;
	case MR_TimbreTemp:
		region->write(first, off, data, len);
		for (unsigned int i = first; i <= last; i++) {
			char instrumentName[11];
			memcpy(instrumentName, mt32ram.timbreTemp[i].common.name, 10);
			instrumentName[10] = 0;
#if MT32EMU_MONITOR_SYSEX > 0
			printDebug("WRITE-PARTTIMBRE (%d-%d@%d..%d): timbre=%d (%s)", first, last, off, off + len, i, instrumentName);
#endif
			if (parts[i] != NULL) {
				parts[i]->refresh();
			}
		}
		break;
	case MR_Patches:
		region->write(first, off, data, len);
#if MT32EMU_MONITOR_SYSEX > 0
		for (unsigned int i = first; i <= last; i++) {
			PatchParam *patch = &mt32ram.patches[i];
			int patchAbsTimbreNum = patch->timbreGroup * 64 + patch->timbreNum;
			char instrumentName[11];
			memcpy(instrumentName, mt32ram.timbres[patchAbsTimbreNum].timbre.common.name, 10);
			instrumentName[10] = 0;
			Bit8u *n = (Bit8u *)patch;
			printDebug("WRITE-PATCH (%d-%d@%d..%d): %d; timbre=%d (%s) %02X%02X%02X%02X%02X%02X%02X%02X", first, last, off, off + len, i, patchAbsTimbreNum, instrumentName, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);
		}
#endif
		break;
	case MR_Timbres:
		// Timbres
		first += 128;
		last += 128;
		region->write(first, off, data, len);
		for (unsigned int i = first; i <= last; i++) {
#if MT32EMU_MONITOR_TIMBRES >= 1
			TimbreParam *timbre = &mt32ram.timbres[i].timbre;
			char instrumentName[11];
			memcpy(instrumentName, timbre->common.name, 10);
			instrumentName[10] = 0;
			printDebug("WRITE-TIMBRE (%d-%d@%d..%d): %d; name=\"%s\"", first, last, off, off + len, i, instrumentName);
#if MT32EMU_MONITOR_TIMBRES >= 2
#define DT(x) printDebug(" " #x ": %d", timbre->x)
			DT(common.partialStructure12);
			DT(common.partialStructure34);
			DT(common.partialMute);
			DT(common.noSustain);

#define DTP(x) \
			DT(partial[x].wg.pitchCoarse); \
			DT(partial[x].wg.pitchFine); \
			DT(partial[x].wg.pitchKeyfollow); \
			DT(partial[x].wg.pitchBenderEnabled); \
			DT(partial[x].wg.waveform); \
			DT(partial[x].wg.pcmWave); \
			DT(partial[x].wg.pulseWidth); \
			DT(partial[x].wg.pulseWidthVeloSensitivity); \
			DT(partial[x].pitchEnv.depth); \
			DT(partial[x].pitchEnv.veloSensitivity); \
			DT(partial[x].pitchEnv.timeKeyfollow); \
			DT(partial[x].pitchEnv.time[0]); \
			DT(partial[x].pitchEnv.time[1]); \
			DT(partial[x].pitchEnv.time[2]); \
			DT(partial[x].pitchEnv.time[3]); \
			DT(partial[x].pitchEnv.level[0]); \
			DT(partial[x].pitchEnv.level[1]); \
			DT(partial[x].pitchEnv.level[2]); \
			DT(partial[x].pitchEnv.level[3]); \
			DT(partial[x].pitchEnv.level[4]); \
			DT(partial[x].pitchLFO.rate); \
			DT(partial[x].pitchLFO.depth); \
			DT(partial[x].pitchLFO.modSensitivity); \
			DT(partial[x].tvf.cutoff); \
			DT(partial[x].tvf.resonance); \
			DT(partial[x].tvf.keyfollow); \
			DT(partial[x].tvf.biasPoint); \
			DT(partial[x].tvf.biasLevel); \
			DT(partial[x].tvf.envDepth); \
			DT(partial[x].tvf.envVeloSensitivity); \
			DT(partial[x].tvf.envDepthKeyfollow); \
			DT(partial[x].tvf.envTimeKeyfollow); \
			DT(partial[x].tvf.envTime[0]); \
			DT(partial[x].tvf.envTime[1]); \
			DT(partial[x].tvf.envTime[2]); \
			DT(partial[x].tvf.envTime[3]); \
			DT(partial[x].tvf.envTime[4]); \
			DT(partial[x].tvf.envLevel[0]); \
			DT(partial[x].tvf.envLevel[1]); \
			DT(partial[x].tvf.envLevel[2]); \
			DT(partial[x].tvf.envLevel[3]); \
			DT(partial[x].tva.level); \
			DT(partial[x].tva.veloSensitivity); \
			DT(partial[x].tva.biasPoint1); \
			DT(partial[x].tva.biasLevel1); \
			DT(partial[x].tva.biasPoint2); \
			DT(partial[x].tva.biasLevel2); \
			DT(partial[x].tva.envTimeKeyfollow); \
			DT(partial[x].tva.envTimeVeloSensitivity); \
			DT(partial[x].tva.envTime[0]); \
			DT(partial[x].tva.envTime[1]); \
			DT(partial[x].tva.envTime[2]); \
			DT(partial[x].tva.envTime[3]); \
			DT(partial[x].tva.envTime[4]); \
			DT(partial[x].tva.envLevel[0]); \
			DT(partial[x].tva.envLevel[1]); \
			DT(partial[x].tva.envLevel[2]); \
			DT(partial[x].tva.envLevel[3]);

			DTP(0);
			DTP(1);
			DTP(2);
			DTP(3);
#undef DTP
#undef DT
#endif
#endif
			// FIXME:KG: Not sure if the stuff below should be done (for rhythm and/or parts)...
			// Does the real MT-32 automatically do this?
			for (unsigned int part = 0; part < 9; part++) {
				if (parts[part] != NULL) {
					parts[part]->refreshTimbre(i);
				}
			}
		}
		break;
	case MR_System:
		region->write(0, off, data, len);

		reportHandler->onDeviceReconfig();
		// FIXME: We haven't properly confirmed any of this behaviour
		// In particular, we tend to reset things such as reverb even if the write contained
		// the same parameters as were already set, which may be wrong.
		// On the other hand, the real thing could be resetting things even when they aren't touched
		// by the write at all.
#if MT32EMU_MONITOR_SYSEX > 0
		printDebug("WRITE-SYSTEM:");
#endif
		if (off <= SYSTEM_MASTER_TUNE_OFF && off + len > SYSTEM_MASTER_TUNE_OFF) {
			refreshSystemMasterTune();
		}
		if (off <= SYSTEM_REVERB_LEVEL_OFF && off + len > SYSTEM_REVERB_MODE_OFF) {
			refreshSystemReverbParameters();
		}
		if (off <= SYSTEM_RESERVE_SETTINGS_END_OFF && off + len > SYSTEM_RESERVE_SETTINGS_START_OFF) {
			refreshSystemReserveSettings();
		}
		if (off <= SYSTEM_CHAN_ASSIGN_END_OFF && off + len > SYSTEM_CHAN_ASSIGN_START_OFF) {
			int firstPart = off - SYSTEM_CHAN_ASSIGN_START_OFF;
			if(firstPart < 0)
				firstPart = 0;
			int lastPart = off + len - SYSTEM_CHAN_ASSIGN_START_OFF;
			if(lastPart > 9)
				lastPart = 9;
			refreshSystemChanAssign(firstPart, lastPart);
		}
		if (off <= SYSTEM_MASTER_VOL_OFF && off + len > SYSTEM_MASTER_VOL_OFF) {
			refreshSystemMasterVol();
		}
		break;
	case MR_Display:
		char buf[MAX_SYSEX_SIZE];
		memcpy(&buf, &data[0], len);
		buf[len] = 0;
#if MT32EMU_MONITOR_SYSEX > 0
		printDebug("WRITE-LCD: %s", buf);
#endif
		reportHandler->showLCDMessage(buf);
		break;
	case MR_Reset:
		reset();
		break;
	}
}

void Synth::refreshSystemMasterTune() {
#if MT32EMU_MONITOR_SYSEX > 0
	//FIXME:KG: This is just an educated guess.
	// The LAPC-I documentation claims a range of 427.5Hz-452.6Hz (similar to what we have here)
	// The MT-32 documentation claims a range of 432.1Hz-457.6Hz
	float masterTune = 440.0f * EXP2F((mt32ram.system.masterTune - 64.0f) / (128.0f * 12.0f));
	printDebug(" Master Tune: %f", masterTune);
#endif
}

void Synth::refreshSystemReverbParameters() {
#if MT32EMU_MONITOR_SYSEX > 0
	printDebug(" Reverb: mode=%d, time=%d, level=%d", mt32ram.system.reverbMode, mt32ram.system.reverbTime, mt32ram.system.reverbLevel);
#endif
	if (reverbOverridden && reverbModel != NULL) {
#if MT32EMU_MONITOR_SYSEX > 0
		printDebug(" (Reverb overridden - ignoring)");
#endif
		return;
	}
	reportHandler->onNewReverbMode(mt32ram.system.reverbMode);
	reportHandler->onNewReverbTime(mt32ram.system.reverbTime);
	reportHandler->onNewReverbLevel(mt32ram.system.reverbLevel);

	ReverbModel *newReverbModel = reverbModels[mt32ram.system.reverbMode];
#if MT32EMU_REDUCE_REVERB_MEMORY
	if (reverbModel != newReverbModel) {
		if (reverbModel != NULL) {
			reverbModel->close();
		}
		newReverbModel->open();
	}
#endif
	reverbModel = newReverbModel;
	reverbModel->setParameters(mt32ram.system.reverbTime, mt32ram.system.reverbLevel);
}

void Synth::refreshSystemReserveSettings() {
	Bit8u *rset = mt32ram.system.reserveSettings;
#if MT32EMU_MONITOR_SYSEX > 0
	printDebug(" Partial reserve: 1=%02d 2=%02d 3=%02d 4=%02d 5=%02d 6=%02d 7=%02d 8=%02d Rhythm=%02d", rset[0], rset[1], rset[2], rset[3], rset[4], rset[5], rset[6], rset[7], rset[8]);
#endif
	partialManager->setReserve(rset);
}

void Synth::refreshSystemChanAssign(unsigned int firstPart, unsigned int lastPart) {
	memset(chantable, -1, sizeof(chantable));

	// CONFIRMED: In the case of assigning a channel to multiple parts, the lower part wins.
	for (unsigned int i = 0; i <= 8; i++) {
		if (parts[i] != NULL && i >= firstPart && i <= lastPart) {
			// CONFIRMED: Decay is started for all polys, and all controllers are reset, for every part whose assignment was touched by the sysex write.
			parts[i]->allSoundOff();
			parts[i]->resetAllControllers();
		}
		int chan = mt32ram.system.chanAssign[i];
		if (chan != 16 && chantable[chan] == -1) {
			chantable[chan] = i;
		}
	}

#if MT32EMU_MONITOR_SYSEX > 0
	Bit8u *rset = mt32ram.system.chanAssign;
	printDebug(" Part assign:     1=%02d 2=%02d 3=%02d 4=%02d 5=%02d 6=%02d 7=%02d 8=%02d Rhythm=%02d", rset[0], rset[1], rset[2], rset[3], rset[4], rset[5], rset[6], rset[7], rset[8]);
#endif
}

void Synth::refreshSystemMasterVol() {
#if MT32EMU_MONITOR_SYSEX > 0
	printDebug(" Master volume: %d", mt32ram.system.masterVol);
#endif
}

void Synth::refreshSystem() {
	refreshSystemMasterTune();
	refreshSystemReverbParameters();
	refreshSystemReserveSettings();
	refreshSystemChanAssign(0, 8);
	refreshSystemMasterVol();
}

void Synth::reset() {
#if MT32EMU_MONITOR_SYSEX > 0
	printDebug("RESET");
#endif
	reportHandler->onDeviceReset();
	partialManager->deactivateAll();
	mt32ram = mt32default;
	for (int i = 0; i < 9; i++) {
		parts[i]->reset();
		if (i != 8) {
			parts[i]->setProgram(controlROMData[controlROMMap->programSettings + i]);
		} else {
			parts[8]->refresh();
		}
	}
	refreshSystem();
	isEnabled = false;
}

void Synth::render(Bit16s *stream, Bit32u len) {
	if (!isEnabled) {
		memset(stream, 0, len * sizeof(Bit16s) * 2);
		return;
	}
	while (len > 0) {
		Bit32u thisLen = len > MAX_SAMPLES_PER_RUN ? MAX_SAMPLES_PER_RUN : len;
		renderStreams(tmpNonReverbLeft, tmpNonReverbRight, tmpReverbDryLeft, tmpReverbDryRight, tmpReverbWetLeft, tmpReverbWetRight, thisLen);
		for (Bit32u i = 0; i < thisLen; i++) {
			stream[0] = clipBit16s((Bit32s)tmpNonReverbLeft[i] + (Bit32s)tmpReverbDryLeft[i] + (Bit32s)tmpReverbWetLeft[i]);
			stream[1] = clipBit16s((Bit32s)tmpNonReverbRight[i] + (Bit32s)tmpReverbDryRight[i] + (Bit32s)tmpReverbWetRight[i]);
			stream += 2;
		}
		len -= thisLen;
	}
}

bool Synth::prerender() {
	int newPrerenderWriteIx = (prerenderWriteIx + 1) % MAX_PRERENDER_SAMPLES;
	if (newPrerenderWriteIx == prerenderReadIx) {
		// The prerender buffer is full
		return false;
	}
	doRenderStreams(
		prerenderNonReverbLeft + prerenderWriteIx,
		prerenderNonReverbRight + prerenderWriteIx,
		prerenderReverbDryLeft + prerenderWriteIx,
		prerenderReverbDryRight + prerenderWriteIx,
		prerenderReverbWetLeft + prerenderWriteIx,
		prerenderReverbWetRight + prerenderWriteIx,
		1);
	prerenderWriteIx = newPrerenderWriteIx;
	return true;
}

static inline void maybeCopy(Bit16s *out, Bit32u outPos, Bit16s *in, Bit32u inPos, Bit32u len) {
	if (out == NULL) {
		return;
	}
	memcpy(out + outPos, in + inPos, len * sizeof(Bit16s));
}

void Synth::copyPrerender(Bit16s *nonReverbLeft, Bit16s *nonReverbRight, Bit16s *reverbDryLeft, Bit16s *reverbDryRight, Bit16s *reverbWetLeft, Bit16s *reverbWetRight, Bit32u pos, Bit32u len) {
	maybeCopy(nonReverbLeft, pos, prerenderNonReverbLeft, prerenderReadIx, len);
	maybeCopy(nonReverbRight, pos, prerenderNonReverbRight, prerenderReadIx, len);
	maybeCopy(reverbDryLeft, pos, prerenderReverbDryLeft, prerenderReadIx, len);
	maybeCopy(reverbDryRight, pos, prerenderReverbDryRight, prerenderReadIx, len);
	maybeCopy(reverbWetLeft, pos, prerenderReverbWetLeft, prerenderReadIx, len);
	maybeCopy(reverbWetRight, pos, prerenderReverbWetRight, prerenderReadIx, len);
}

void Synth::checkPrerender(Bit16s *nonReverbLeft, Bit16s *nonReverbRight, Bit16s *reverbDryLeft, Bit16s *reverbDryRight, Bit16s *reverbWetLeft, Bit16s *reverbWetRight, Bit32u &pos, Bit32u &len) {
	if (prerenderReadIx > prerenderWriteIx) {
		// There's data in the prerender buffer, and the write index has wrapped.
		Bit32u prerenderCopyLen = MAX_PRERENDER_SAMPLES - prerenderReadIx;
		if (prerenderCopyLen > len) {
			prerenderCopyLen = len;
		}
		copyPrerender(nonReverbLeft, nonReverbRight, reverbDryLeft, reverbDryRight, reverbWetLeft, reverbWetRight, pos, prerenderCopyLen);
		len -= prerenderCopyLen;
		pos += prerenderCopyLen;
		prerenderReadIx = (prerenderReadIx + prerenderCopyLen) % MAX_PRERENDER_SAMPLES;
	}
	if (prerenderReadIx < prerenderWriteIx) {
		// There's data in the prerender buffer, and the write index is ahead of the read index.
		Bit32u prerenderCopyLen = prerenderWriteIx - prerenderReadIx;
		if (prerenderCopyLen > len) {
			prerenderCopyLen = len;
		}
		copyPrerender(nonReverbLeft, nonReverbRight, reverbDryLeft, reverbDryRight, reverbWetLeft, reverbWetRight, pos, prerenderCopyLen);
		len -= prerenderCopyLen;
		pos += prerenderCopyLen;
		prerenderReadIx += prerenderCopyLen;
	}
	if (prerenderReadIx == prerenderWriteIx) {
		// If the ring buffer's empty, reset it to start at 0 to minimise wrapping,
		// which requires two writes instead of one.
		prerenderReadIx = prerenderWriteIx = 0;
	}
}

void Synth::renderStreams(Bit16s *nonReverbLeft, Bit16s *nonReverbRight, Bit16s *reverbDryLeft, Bit16s *reverbDryRight, Bit16s *reverbWetLeft, Bit16s *reverbWetRight, Bit32u len) {
	if (!isEnabled) {
		clearIfNonNull(nonReverbLeft, len);
		clearIfNonNull(nonReverbRight, len);
		clearIfNonNull(reverbDryLeft, len);
		clearIfNonNull(reverbDryRight, len);
		clearIfNonNull(reverbWetLeft, len);
		clearIfNonNull(reverbWetRight, len);
		return;
	}
	Bit32u pos = 0;

	// First, check for data in the prerender buffer and spit that out before generating anything new.
	// Note that the prerender buffer is rarely used - see comments elsewhere for details.
	checkPrerender(nonReverbLeft, nonReverbRight, reverbDryLeft, reverbDryRight, reverbWetLeft, reverbWetRight, pos, len);

	while (len > 0) {
		Bit32u thisLen = len > MAX_SAMPLES_PER_RUN ? MAX_SAMPLES_PER_RUN : len;
		doRenderStreams(
			streamOffset(nonReverbLeft, pos),
			streamOffset(nonReverbRight, pos),
			streamOffset(reverbDryLeft, pos),
			streamOffset(reverbDryRight, pos),
			streamOffset(reverbWetLeft, pos),
			streamOffset(reverbWetRight, pos),
			thisLen);
		len -= thisLen;
		pos += thisLen;
	}
}

// FIXME: Using more temporary buffers than we need to
void Synth::doRenderStreams(Bit16s *nonReverbLeft, Bit16s *nonReverbRight, Bit16s *reverbDryLeft, Bit16s *reverbDryRight, Bit16s *reverbWetLeft, Bit16s *reverbWetRight, Bit32u len) {
	clearFloats(&tmpBufMixLeft[0], &tmpBufMixRight[0], len);
	if (!reverbEnabled) {
		for (unsigned int i = 0; i < getPartialLimit(); i++) {
			if (partialManager->produceOutput(i, &tmpBufPartialLeft[0], &tmpBufPartialRight[0], len)) {
				mix(&tmpBufMixLeft[0], &tmpBufPartialLeft[0], len);
				mix(&tmpBufMixRight[0], &tmpBufPartialRight[0], len);
			}
		}
		if (nonReverbLeft != NULL) {
			la32FloatToBit16sFunc(nonReverbLeft, &tmpBufMixLeft[0], len, outputGain);
		}
		if (nonReverbRight != NULL) {
			la32FloatToBit16sFunc(nonReverbRight, &tmpBufMixRight[0], len, outputGain);
		}
		clearIfNonNull(reverbDryLeft, len);
		clearIfNonNull(reverbDryRight, len);
		clearIfNonNull(reverbWetLeft, len);
		clearIfNonNull(reverbWetRight, len);
	} else {
		for (unsigned int i = 0; i < getPartialLimit(); i++) {
			if (!partialManager->shouldReverb(i)) {
				if (partialManager->produceOutput(i, &tmpBufPartialLeft[0], &tmpBufPartialRight[0], len)) {
					mix(&tmpBufMixLeft[0], &tmpBufPartialLeft[0], len);
					mix(&tmpBufMixRight[0], &tmpBufPartialRight[0], len);
				}
			}
		}
		if (nonReverbLeft != NULL) {
			la32FloatToBit16sFunc(nonReverbLeft, &tmpBufMixLeft[0], len, outputGain);
		}
		if (nonReverbRight != NULL) {
			la32FloatToBit16sFunc(nonReverbRight, &tmpBufMixRight[0], len, outputGain);
		}

		clearFloats(&tmpBufMixLeft[0], &tmpBufMixRight[0], len);
		for (unsigned int i = 0; i < getPartialLimit(); i++) {
			if (partialManager->shouldReverb(i)) {
				if (partialManager->produceOutput(i, &tmpBufPartialLeft[0], &tmpBufPartialRight[0], len)) {
					mix(&tmpBufMixLeft[0], &tmpBufPartialLeft[0], len);
					mix(&tmpBufMixRight[0], &tmpBufPartialRight[0], len);
				}
			}
		}
		if (reverbDryLeft != NULL) {
			la32FloatToBit16sFunc(reverbDryLeft, &tmpBufMixLeft[0], len, outputGain);
		}
		if (reverbDryRight != NULL) {
			la32FloatToBit16sFunc(reverbDryRight, &tmpBufMixRight[0], len, outputGain);
		}

		// FIXME: Note that on the real devices, reverb input and output are signed linear 16-bit (well, kinda, there's some fudging) PCM, not float.
		reverbModel->process(&tmpBufMixLeft[0], &tmpBufMixRight[0], &tmpBufReverbOutLeft[0], &tmpBufReverbOutRight[0], len);
		if (reverbWetLeft != NULL) {
			reverbFloatToBit16sFunc(reverbWetLeft, &tmpBufReverbOutLeft[0], len, reverbOutputGain);
		}
		if (reverbWetRight != NULL) {
			reverbFloatToBit16sFunc(reverbWetRight, &tmpBufReverbOutRight[0], len, reverbOutputGain);
		}
	}
	partialManager->clearAlreadyOutputed();
	renderedSampleCount += len;
}

void Synth::printPartialUsage(unsigned long sampleOffset) {
	unsigned int partialUsage[9];
	partialManager->getPerPartPartialUsage(partialUsage);
	if (sampleOffset > 0) {
		printDebug("[+%lu] Partial Usage: 1:%02d 2:%02d 3:%02d 4:%02d 5:%02d 6:%02d 7:%02d 8:%02d R: %02d  TOTAL: %02d", sampleOffset, partialUsage[0], partialUsage[1], partialUsage[2], partialUsage[3], partialUsage[4], partialUsage[5], partialUsage[6], partialUsage[7], partialUsage[8], getPartialLimit() - partialManager->getFreePartialCount());
	} else {
		printDebug("Partial Usage: 1:%02d 2:%02d 3:%02d 4:%02d 5:%02d 6:%02d 7:%02d 8:%02d R: %02d  TOTAL: %02d", partialUsage[0], partialUsage[1], partialUsage[2], partialUsage[3], partialUsage[4], partialUsage[5], partialUsage[6], partialUsage[7], partialUsage[8], getPartialLimit() - partialManager->getFreePartialCount());
	}
}

bool Synth::hasActivePartials() const {
	if (prerenderReadIx != prerenderWriteIx) {
		// Data in the prerender buffer means that the current isActive() states are "in the future".
		// It also means that partials are definitely active at this render point.
		return true;
	}
	for (int partialNum = 0; partialNum < getPartialLimit(); partialNum++) {
		if (partialManager->getPartial(partialNum)->isActive()) {
			return true;
		}
	}
	return false;
}

bool Synth::isActive() const {
	if (hasActivePartials()) {
		return true;
	}
	if (reverbEnabled) {
		return reverbModel->isActive();
	}
	return false;
}

const Partial *Synth::getPartial(unsigned int partialNum) const {
	return partialManager->getPartial(partialNum);
}

const Part *Synth::getPart(unsigned int partNum) const {
	if (partNum > 8) {
		return NULL;
	}
	return parts[partNum];
}

void MemoryRegion::read(unsigned int entry, unsigned int off, Bit8u *dst, unsigned int len) const {
	off += entry * entrySize;
	// This method should never be called with out-of-bounds parameters,
	// or on an unsupported region - seeing any of this debug output indicates a bug in the emulator
	if (off > entrySize * entries - 1) {
#if MT32EMU_MONITOR_SYSEX > 0
		synth->printDebug("read[%d]: parameters start out of bounds: entry=%d, off=%d, len=%d", type, entry, off, len);
#endif
		return;
	}
	if (off + len > entrySize * entries) {
#if MT32EMU_MONITOR_SYSEX > 0
		synth->printDebug("read[%d]: parameters end out of bounds: entry=%d, off=%d, len=%d", type, entry, off, len);
#endif
		len = entrySize * entries - off;
	}
	Bit8u *src = getRealMemory();
	if (src == NULL) {
#if MT32EMU_MONITOR_SYSEX > 0
		synth->printDebug("read[%d]: unreadable region: entry=%d, off=%d, len=%d", type, entry, off, len);
#endif
		return;
	}
	memcpy(dst, src + off, len);
}

void MemoryRegion::write(unsigned int entry, unsigned int off, const Bit8u *src, unsigned int len, bool init) const {
	unsigned int memOff = entry * entrySize + off;
	// This method should never be called with out-of-bounds parameters,
	// or on an unsupported region - seeing any of this debug output indicates a bug in the emulator
	if (off > entrySize * entries - 1) {
#if MT32EMU_MONITOR_SYSEX > 0
		synth->printDebug("write[%d]: parameters start out of bounds: entry=%d, off=%d, len=%d", type, entry, off, len);
#endif
		return;
	}
	if (off + len > entrySize * entries) {
#if MT32EMU_MONITOR_SYSEX > 0
		synth->printDebug("write[%d]: parameters end out of bounds: entry=%d, off=%d, len=%d", type, entry, off, len);
#endif
		len = entrySize * entries - off;
	}
	Bit8u *dest = getRealMemory();
	if (dest == NULL) {
#if MT32EMU_MONITOR_SYSEX > 0
		synth->printDebug("write[%d]: unwritable region: entry=%d, off=%d, len=%d", type, entry, off, len);
#endif
	}

	for (unsigned int i = 0; i < len; i++) {
		Bit8u desiredValue = src[i];
		Bit8u maxValue = getMaxValue(memOff);
		// maxValue == 0 means write-protected unless called from initialisation code, in which case it really means the maximum value is 0.
		if (maxValue != 0 || init) {
			if (desiredValue > maxValue) {
#if MT32EMU_MONITOR_SYSEX > 0
				synth->printDebug("write[%d]: Wanted 0x%02x at %d, but max 0x%02x", type, desiredValue, memOff, maxValue);
#endif
				desiredValue = maxValue;
			}
			dest[memOff] = desiredValue;
		} else if (desiredValue != 0) {
#if MT32EMU_MONITOR_SYSEX > 0
			// Only output debug info if they wanted to write non-zero, since a lot of things cause this to spit out a lot of debug info otherwise.
			synth->printDebug("write[%d]: Wanted 0x%02x at %d, but write-protected", type, desiredValue, memOff);
#endif
		}
		memOff++;
	}
}


void Synth::setPartialLimit( unsigned int _partialLimit )
{
	partialLimit = _partialLimit;
}


const unsigned int Synth::getPartialLimit() const
{
	return partialLimit;
}


void Synth::findPart( const Part *src, Bit8u *index_out )
{
	Bit8u part_idx;


	part_idx = 0xff;
	if( src != NULL ) {
		bool stop;
	
		part_idx = 0;
		stop = false;

		for( int lcv=0; lcv<9; lcv++ ) {
			if( src == getPart(lcv) ) { stop = true; break; }

			part_idx++;
		}

#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}


	*index_out = part_idx;
}


void Synth::findPartial( const Partial *src, Bit8u *index_out )
{
	Bit8u partials_idx;


	partials_idx = 0xff;
	if( src != NULL ) {
		bool stop;

		partials_idx = 0;
		stop = false;

		for( int lcv=0; lcv<getPartialLimit(); lcv++ ) {
			if( src == getPartial(lcv) ) { stop = true; break; }

			partials_idx++;
		}

#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}


	*index_out = partials_idx;
}


void Synth::findPartialParam( const TimbreParam::PartialParam *src, Bit16u *index_out1, Bit16u *index_out2 )
{
	Bit16u partialParam_idx1, partialParam_idx2;
	bool stop;


	stop = false;

	partialParam_idx1 = 0xffff;
	partialParam_idx2 = 0xffff;


	if( src != NULL ) {
		partialParam_idx1 = 0;

		// #1 = mt32ram.timbres[] - partial
		for( int lcv1=0; lcv1<256; lcv1++ ) {
			partialParam_idx2 = 0;

			for( int lcv2=0; lcv2<4; lcv2++ ) {
				if( src == &mt32ram.timbres[lcv1].timbre.partial[lcv2] ) { stop = true; break; }

				partialParam_idx2++;
			}
			if( stop == true ) break;

			partialParam_idx1++;
		}


		if( stop == false ) {
			partialParam_idx1 = 0x1000;

			// #2 = mt32ram.timbreTemp - partial[]
			for( int lcv1=0; lcv1<8; lcv1++ ) {
				partialParam_idx2 = 0;

				for( int lcv2=0; lcv2<4; lcv2++ ) {
					if( src == &mt32ram.timbreTemp[lcv1].partial[lcv2] ) { stop = true; break; }

					partialParam_idx2++;
				}
				if( stop == true ) break;

				partialParam_idx1++;
			}
		}


		if( stop == false ) {
			partialParam_idx1 = 0x2000;

			// #3 = parts[0-7] - patchCache[0-3].srcPartial
			for( int lcv1=0; lcv1<8; lcv1++ ) {
				partialParam_idx2 = 0;

				for( int lcv2=0; lcv2<4; lcv2++ ) {
					Part *part;

					part = (Part *) getPart(lcv1);
					if( src == &part->getPatchCache(lcv2)->srcPartial ) { stop = true; break; }

					partialParam_idx2++;
				}
				if( stop == true ) break;

				partialParam_idx1++;
			}
		}


		if( stop == false ) {
			partialParam_idx1 = 0x3000;

			// #4 = parts[8] - patchCache[0-3].srcPartial (Rhythm)
			for( int lcv1=0; lcv1<1; lcv1++ ) {
				partialParam_idx2 = 0;

				for( int lcv2=0; lcv2<4; lcv2++ ) {
					RhythmPart *part;

					part = (RhythmPart *) getPart(8);
					if( src == &part->getPatchCache(lcv2)->srcPartial ) { stop = true; break; }

					partialParam_idx2++;
				}
				if( stop == true ) break;

				partialParam_idx1++;
			}
		}


		if( stop == false ) {
			partialParam_idx1 = 0x4000;

			// #5 = parts[8] - drumCache[][].srcPartial (Rhythm)
			for( int lcv1=0; lcv1<85; lcv1++ ) {
				partialParam_idx2 = 0;

				for( int lcv2=0; lcv2<4; lcv2++ ) {
					RhythmPart *part;

					part = (RhythmPart *) getPart(8);
					if( src == &part->getDrumCache(lcv1,lcv2)->srcPartial ) { stop = true; break; }

					partialParam_idx2++;
				}
				if( stop == true ) break;

				partialParam_idx1++;
			}
		}


		if( stop == false ) {
			partialParam_idx1 = 0x5000;

			// #6 = partials[] - cacheBackup
			for( int lcv1=0; lcv1<getPartialLimit(); lcv1++ ) {
				partialParam_idx2 = 0;

				for( int lcv2=0; lcv2<1; lcv2++ ) {
					if( src == &getPartial(lcv1)->cachebackup.srcPartial ) { stop = true; break; }

					partialParam_idx2++;
				}
				if( stop == true ) break;

				partialParam_idx1++;
			}


#ifdef WIN32_DEBUG
			// DEBUG
			if( stop == false ) __asm int 3
#endif
		}
	}


	*index_out1 = partialParam_idx1;
	*index_out2 = partialParam_idx2;
}


void Synth::findPatchCache( const PatchCache *src, Bit16u *index_out1, Bit16u *index_out2 )
{
	Bit16u patchCache_idx1, patchCache_idx2;
	bool stop;


	stop = false;

	patchCache_idx1 = 0xffff;
	patchCache_idx2 = 0xffff;


	if( src != NULL ) {
		patchCache_idx1 = 0;

		// #1 = parts[0-7] - patchCache
		for( int lcv1=0; lcv1<8; lcv1++ ) {
			patchCache_idx2 = 0;

			for( int lcv2=0; lcv2<4; lcv2++ ) {
				Part *part;

				part = (Part *) getPart(lcv1);
				if( src == part->getPatchCache(lcv2) ) { stop = true; break; }

				patchCache_idx2++;
			}
			if( stop == true ) break;

			patchCache_idx1++;
		}


		if( stop == false ) {
			patchCache_idx1 = 0x1000;

			// #2 parts[8] - patchCache (Rhythm)
			for( int lcv1=0; lcv1<1; lcv1++ ) {
				patchCache_idx2 = 0;

				for( int lcv2=0; lcv2<4; lcv2++ ) {
					RhythmPart *part;

					part = (RhythmPart *) getPart(8);
					if( src == part->getPatchCache(lcv2) ) { stop = true; break; }

					patchCache_idx2++;
				}
				if( stop == true ) break;

				patchCache_idx1++;
			}
		}


		if( stop == false ) {
			patchCache_idx1 = 0x2000;

			// #3 parts[8] - drumcache[][] (Rhythm)
			for( int lcv1=0; lcv1<85; lcv1++ ) {
				patchCache_idx2 = 0;

				for( int lcv2=0; lcv2<4; lcv2++ ) {
					RhythmPart *part;

					part = (RhythmPart *) getPart(8);
					if( src == part->getDrumCache(lcv1,lcv2) ) { stop = true; break; }

					patchCache_idx2++;
				}
				if( stop == true ) break;

				patchCache_idx1++;
			}
		}


		if( stop == false ) {
			patchCache_idx1 = 0x3000;

			// #4 partials[] - cacheBackup
			for( int lcv1=0; lcv1<getPartialLimit(); lcv1++ ) {
				patchCache_idx2 = 0;

				for( int lcv2=0; lcv2<1; lcv2++ ) {
					if( src == &getPartial(lcv1)->cachebackup ) { stop = true; break; }

					patchCache_idx2++;
				}
				if( stop == true ) break;

				patchCache_idx1++;
			}


#ifdef WIN32_DEBUG
			// DEBUG
			if( stop == false ) __asm int 3
#endif
		}
	}


	*index_out1 = patchCache_idx1;
	*index_out2 = patchCache_idx2;
}


void Synth::findPatchTemp( const MemParams::PatchTemp *src, Bit8u *index_out )
{
	Bit8u patchTemp_idx;


	patchTemp_idx = 0xff;
	if( src != NULL ) {
		bool stop;

		patchTemp_idx = 0;
		stop = false;

		for( int lcv=0; lcv<9; lcv++ ) {
			if( src == &mt32ram.patchTemp[lcv] ) { stop = true; break; }

			patchTemp_idx++;
		}


#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}


	*index_out = patchTemp_idx;
}


void Synth::findPCMWaveEntry( const PCMWaveEntry *src, Bit16u *index_out )
{
	Bit16u PCMWaveEntry_idx;

	PCMWaveEntry_idx = 0xffff;
	if( src != NULL ) {
		bool stop;

		PCMWaveEntry_idx = 0;
		stop = false;

		for( int lcv=0; lcv<controlROMMap->pcmCount; lcv++ ) {
			if( src == &pcmWaves[lcv] ) { stop = true; break; }

			PCMWaveEntry_idx++;
		}


#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}


	*index_out = PCMWaveEntry_idx;
}

// WE NEED TO REFRESH THIS
void Synth::findPoly( const Poly *src, Bit16u *index_out1, Bit16u *index_out2 )
{
	Bit16u poly_idx1, poly_idx2;


	poly_idx1 = 0xffff;
	poly_idx2 = 0xffff;

	if( src != NULL ) {
		bool stop;

		poly_idx1 = 0;
		stop = false;

		for( int lcv1=0; lcv1<9; lcv1++ ) {
			Part *part;
			part = (Part *) getPart(lcv1);

			poly_idx2 = 0;

			for( int lcv2=0; lcv2<part->getActivePolyCount(); lcv2++ ) {
				if( src == part->getActivePoly(lcv2) ) { stop = true; break; }

				poly_idx2++;
			}
			if( stop ) break;

			poly_idx1++;
		}


#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}

	*index_out1 = poly_idx1;
	*index_out2 = poly_idx2;
}



void Synth::findRhythmTemp( const MemParams::RhythmTemp *src, Bit8u *index_out )
{
	Bit8u rhythmTemp_idx;

	rhythmTemp_idx = 0xff;
	if( src != NULL ) {
		bool stop;

		rhythmTemp_idx = 0;
		stop = false;

		for( int lcv=0; lcv<85; lcv++ ) {
			if( src == &mt32ram.rhythmTemp[lcv] ) { stop = true; break; }

			rhythmTemp_idx++;
		}

#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}


	*index_out = rhythmTemp_idx;
}


void Synth::findTimbreParam( const TimbreParam *src, Bit8u *index_out )
{
	Bit8u timbreParam_idx;


	timbreParam_idx = 0xff;
	if( src != NULL ) {
		bool stop;

		timbreParam_idx = 0;
		stop = false;

		for( int lcv=0; lcv<8; lcv++ ) {
			if( src == &mt32ram.timbreTemp[lcv] ) { stop = true; break; }

			timbreParam_idx++;
		}

#ifdef WIN32_DEBUG
		// DEBUG
		if( stop == false ) __asm int 3
#endif
	}


	*index_out = timbreParam_idx;
}


Part *Synth::indexPart( Bit8u index )
{
	Part *ptr;

	ptr = NULL;
	if( index != 0xff )
		ptr = (Part *) getPart(index);

	return ptr;
}


Partial *Synth::indexPartial( Bit8u index )
{
	Partial *ptr;

	ptr = NULL;
	if( index != 0xff )
		ptr = (Partial *) getPartial(index);

	return ptr;
}


TimbreParam::PartialParam *Synth::indexPartialParam( Bit16u index1, Bit16u index2 )
{
	TimbreParam::PartialParam *ptr;


	ptr = NULL;
	if( index1 < 0x1000 ) {
		index1 -= 0x0000;

		ptr = &mt32ram.timbres[index1].timbre.partial[index2];
	}
	else if( index1 < 0x2000 ) {
		index1 -= 0x1000;

		ptr = &mt32ram.timbreTemp[index1].partial[index2];
	}
	else if( index1 < 0x3000 ) {
		Part *part;

		index1 -= 0x2000;

		part = (Part *) getPart(index1);
		ptr = (TimbreParam::PartialParam *) &part->getPatchCache(index2)->srcPartial;
	}
	else if( index1 < 0x4000 ) {
		RhythmPart *part;

		index1 -= 0x3000;

		part = (RhythmPart *) getPart(8);
		ptr = (TimbreParam::PartialParam *) &part->getPatchCache(index2)->srcPartial;
	}
	else if( index1 < 0x5000 ) {
		RhythmPart *part;

		index1 -= 0x4000;

		part = (RhythmPart *) getPart(8);
		ptr = (TimbreParam::PartialParam *) &part->getDrumCache(index1,index2)->srcPartial;
	}
	else if( index1 < 0x6000 ) {
		index1 -= 0x5000;

		ptr = (TimbreParam::PartialParam *) &getPartial(index1)->cachebackup.srcPartial;
	}


	return ptr;
}


PatchCache *Synth::indexPatchCache( Bit16u index1, Bit16u index2 )
{
	PatchCache *ptr;


	ptr = NULL;
	if( index1 < 0x1000 ) {
		Part *part;

		index1 -= 0x0000;

		part = (Part *) getPart(index1);
		ptr = (PatchCache *) part->getPatchCache(index2);
	}
	else if( index1 < 0x2000 ) {
		RhythmPart *part;

		index1 -= 0x1000;

		part = (RhythmPart *) getPart(8);
		ptr = (PatchCache *) part->getPatchCache(index2);
	}
	else if( index1 < 0x3000 ) {
		RhythmPart *part;

		index1 -= 0x2000;

		part = (RhythmPart *) getPart(8);
		ptr = (PatchCache *) part->getDrumCache(index1, index2);
	}
	else if( index1 < 0x4000 ) {
		index1 -= 0x3000;

		ptr = (PatchCache *) &getPartial(index1)->cachebackup;
	}


	return ptr;
}


MemParams::PatchTemp *Synth::indexPatchTemp( Bit8u index )
{
	MemParams::PatchTemp *ptr;

	ptr = NULL;
	if( index != 0xff )
		ptr = &mt32ram.patchTemp[index];

	return ptr;
}


PCMWaveEntry *Synth::indexPCMWaveEntry( Bit16u index )
{
	PCMWaveEntry *ptr;

	ptr = NULL;
	if( index != 0xffff ) {
		ptr = &pcmWaves[index];
	}

	return ptr;
}

// WE NEED TO REFRESH THIS
Poly *Synth::indexPoly( Bit16u index1, Bit16u index2 )
{
	Poly *ptr;


	ptr = NULL;
	if( index1 != 0xffff ) {
		Part *part;
		part = (Part *) getPart(index1);

		ptr = (Poly *) part->getActivePoly(index2);
	}


	return ptr;
}


MemParams::RhythmTemp *Synth::indexRhythmTemp( Bit8u index )
{
	MemParams::RhythmTemp *ptr;

	ptr = NULL;
	if( index != 0xff ) {
		ptr = &mt32ram.rhythmTemp[index];
	}

	return ptr;
}


TimbreParam *Synth::indexTimbreParam( Bit8u index )
{
	TimbreParam *ptr;

	ptr = NULL;
	if( index != 0xff )
		ptr = &mt32ram.timbreTemp[index];

	return ptr;
}


void Synth::savePatchCache( std::ostream &stream, PatchCache *patchCache )
{
	Bit16u partialParam_idx1, partialParam_idx2;


	stream.write(reinterpret_cast<const char*>(&patchCache->playPartial), sizeof(patchCache->playPartial) );
	stream.write(reinterpret_cast<const char*>(&patchCache->PCMPartial), sizeof(patchCache->PCMPartial) );
	stream.write(reinterpret_cast<const char*>(&patchCache->pcm), sizeof(patchCache->pcm) );
	stream.write(reinterpret_cast<const char*>(&patchCache->waveform), sizeof(patchCache->waveform) );
	stream.write(reinterpret_cast<const char*>(&patchCache->structureMix), sizeof(patchCache->structureMix) );
	stream.write(reinterpret_cast<const char*>(&patchCache->structurePosition), sizeof(patchCache->structurePosition) );
	stream.write(reinterpret_cast<const char*>(&patchCache->structurePair), sizeof(patchCache->structurePair) );
	stream.write(reinterpret_cast<const char*>(&patchCache->dirty), sizeof(patchCache->dirty) );
	stream.write(reinterpret_cast<const char*>(&patchCache->partialCount), sizeof(patchCache->partialCount) );
	stream.write(reinterpret_cast<const char*>(&patchCache->sustain), sizeof(patchCache->sustain) );
	stream.write(reinterpret_cast<const char*>(&patchCache->reverb), sizeof(patchCache->reverb) );
	stream.write(reinterpret_cast<const char*>(&patchCache->srcPartial), sizeof(patchCache->srcPartial) );


	// - reloc ptr (!!)
	//patchCache.partialParam;
	findPartialParam( patchCache->partialParam, &partialParam_idx1, &partialParam_idx2 );

	stream.write(reinterpret_cast<const char*>(&partialParam_idx1), sizeof(partialParam_idx1) );
	stream.write(reinterpret_cast<const char*>(&partialParam_idx2), sizeof(partialParam_idx2) );
}


void Synth::loadPatchCache( std::istream &stream, PatchCache *patchCache )
{
	Bit16u partialParam_idx1, partialParam_idx2;


	stream.read(reinterpret_cast<char*>(&patchCache->playPartial), sizeof(patchCache->playPartial) );
	stream.read(reinterpret_cast<char*>(&patchCache->PCMPartial), sizeof(patchCache->PCMPartial) );
	stream.read(reinterpret_cast<char*>(&patchCache->pcm), sizeof(patchCache->pcm) );
	stream.read(reinterpret_cast<char*>(&patchCache->waveform), sizeof(patchCache->waveform) );
	stream.read(reinterpret_cast<char*>(&patchCache->structureMix), sizeof(patchCache->structureMix) );
	stream.read(reinterpret_cast<char*>(&patchCache->structurePosition), sizeof(patchCache->structurePosition) );
	stream.read(reinterpret_cast<char*>(&patchCache->structurePair), sizeof(patchCache->structurePair) );
	stream.read(reinterpret_cast<char*>(&patchCache->dirty), sizeof(patchCache->dirty) );
	stream.read(reinterpret_cast<char*>(&patchCache->partialCount), sizeof(patchCache->partialCount) );
	stream.read(reinterpret_cast<char*>(&patchCache->sustain), sizeof(patchCache->sustain) );
	stream.read(reinterpret_cast<char*>(&patchCache->reverb), sizeof(patchCache->reverb) );
	stream.read(reinterpret_cast<char*>(&patchCache->srcPartial), sizeof(patchCache->srcPartial) );


	// - reloc ptr (!!)
	//patchCache.partialParam;
	stream.read(reinterpret_cast<char*>(&partialParam_idx1), sizeof(partialParam_idx1) );
	stream.read(reinterpret_cast<char*>(&partialParam_idx2), sizeof(partialParam_idx2) );

	patchCache->partialParam = indexPartialParam(partialParam_idx1, partialParam_idx2);
}


void Synth::rawDumpState( char *name, void *data, Bit32u size )
{
	FILE *fp;
	char *str;

#ifndef WIN32_DUMP
	return;
#endif


	//if( rawDumpNo == 40 ) __asm int 3


	str = (char*) malloc(1024);
	sprintf( str, "%s-%d.bin", name, rawDumpNo );

	fp = fopen( str, "wb" );
	if(fp) {
		fwrite( data, 1, size, fp );

		fclose(fp);
	}

	free(str);
}


void Synth::rawLoadState( char *name, void *data, Bit32u size )
{
	FILE *fp;
	char *str;

#ifndef WIN32_DUMP
	return;
#endif


	//if( rawDumpNo == 40 ) __asm int 3


	str = (char*) malloc(1024);
	sprintf( str, "%s-%d.bin", name, rawDumpNo );

	fp = fopen( str, "rb" );
	if(fp) {
		fread( data, 1, size, fp );

		fclose(fp);
	}

	free(str);
}


#ifdef WIN32_DEBUG
void Synth::rawVerifyState( char *name, Synth *useSynth )
{
	Synth *ptr1, *ptr2;
	Synth synth_temp;

#ifndef WIN32_DUMP
	return;
#endif

	ptr1 = this;
	ptr2 = &synth_temp;
	useSynth->rawLoadState( name, ptr2, sizeof(*this) );


	if( ptr1->patchTempMemoryRegion != ptr2->patchTempMemoryRegion ) __asm int 3
	if( ptr1->rhythmTempMemoryRegion != ptr2->rhythmTempMemoryRegion ) __asm int 3
	if( ptr1->timbreTempMemoryRegion != ptr2->timbreTempMemoryRegion ) __asm int 3
	if( ptr1->patchesMemoryRegion != ptr2->patchesMemoryRegion ) __asm int 3
	if( ptr1->timbresMemoryRegion != ptr2->timbresMemoryRegion ) __asm int 3
	if( ptr1->systemMemoryRegion != ptr2->systemMemoryRegion ) __asm int 3
	if( ptr1->displayMemoryRegion != ptr2->displayMemoryRegion ) __asm int 3
	if( ptr1->resetMemoryRegion != ptr2->resetMemoryRegion ) __asm int 3
	if( ptr1->paddedTimbreMaxTable != ptr2->paddedTimbreMaxTable ) __asm int 3
	
	if( ptr1->isEnabled != ptr2->isEnabled ) __asm int 3
	if( ptr1->pcmWaves != ptr2->pcmWaves ) __asm int 3
	if( ptr1->controlROMMap != ptr2->controlROMMap ) __asm int 3
	
	if( memcmp( &ptr1->controlROMData, &ptr2->controlROMData, sizeof(ptr2->controlROMData) ) != 0 ) __asm int 3

	if( ptr1->pcmROMData != ptr2->pcmROMData ) __asm int 3
	if( ptr1->pcmROMSize != ptr2->pcmROMSize ) __asm int 3

	if( memcmp( &ptr1->chantable, &ptr2->chantable, sizeof(ptr2->chantable) ) != 0 ) __asm int 3
	
	if( ptr1->renderedSampleCount != ptr2->renderedSampleCount ) __asm int 3

	if( memcmp( &ptr1->tables, &ptr2->tables, sizeof(ptr2->tables) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->mt32ram, &ptr2->mt32ram, sizeof(ptr2->mt32ram) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->mt32default, &ptr2->mt32default, sizeof(ptr2->mt32default) ) != 0 ) __asm int 3

	for( int lcv=0; lcv<4; lcv++ )
		if( ptr1->reverbModels[lcv] != ptr2->reverbModels[lcv] ) __asm int 3

	if( ptr1->reverbEnabled != ptr2->reverbEnabled ) __asm int 3
	if( ptr1->reverbOverridden != ptr2->reverbOverridden ) __asm int 3
	if( ptr1->la32FloatToBit16sFunc != ptr2->la32FloatToBit16sFunc ) __asm int 3
	if( ptr1->reverbFloatToBit16sFunc != ptr2->reverbFloatToBit16sFunc ) __asm int 3
	if( ptr1->isOpen != ptr2->isOpen ) __asm int 3
	if( ptr1->partialManager != ptr2->partialManager ) __asm int 3

	for( int lcv=0; lcv<9; lcv++ )
		if( ptr1->parts[lcv] != ptr2->parts[lcv] ) __asm int 3

	if( memcmp( &ptr1->tmpBufPartialLeft, &ptr2->tmpBufPartialLeft, sizeof(ptr2->tmpBufPartialLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpBufPartialRight, &ptr2->tmpBufPartialRight, sizeof(ptr2->tmpBufPartialRight) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpBufMixLeft, &ptr2->tmpBufMixLeft, sizeof(ptr2->tmpBufMixLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpBufMixRight, &ptr2->tmpBufMixRight, sizeof(ptr2->tmpBufMixRight) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpBufReverbOutLeft, &ptr2->tmpBufReverbOutLeft, sizeof(ptr2->tmpBufReverbOutLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpBufReverbOutRight, &ptr2->tmpBufReverbOutRight, sizeof(ptr2->tmpBufReverbOutRight) ) != 0 ) __asm int 3

	if( memcmp( &ptr1->tmpNonReverbLeft, &ptr2->tmpNonReverbLeft, sizeof(ptr2->tmpNonReverbLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpNonReverbRight, &ptr2->tmpNonReverbRight, sizeof(ptr2->tmpNonReverbRight) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpReverbDryLeft, &ptr2->tmpReverbDryLeft, sizeof(ptr2->tmpReverbDryLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpReverbDryRight, &ptr2->tmpReverbDryRight, sizeof(ptr2->tmpReverbDryRight) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpReverbWetLeft, &ptr2->tmpReverbWetLeft, sizeof(ptr2->tmpReverbWetLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->tmpReverbWetRight, &ptr2->tmpReverbWetRight, sizeof(ptr2->tmpReverbWetRight) ) != 0 ) __asm int 3

	if( memcmp( &ptr1->prerenderNonReverbLeft, &ptr2->prerenderNonReverbLeft, sizeof(ptr2->prerenderNonReverbLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->prerenderNonReverbRight, &ptr2->prerenderNonReverbRight, sizeof(ptr2->prerenderNonReverbRight) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->prerenderReverbDryLeft, &ptr2->prerenderReverbDryLeft, sizeof(ptr2->prerenderReverbDryLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->prerenderReverbDryRight, &ptr2->prerenderReverbDryRight, sizeof(ptr2->prerenderReverbDryRight) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->prerenderReverbWetLeft, &ptr2->prerenderReverbWetLeft, sizeof(ptr2->prerenderReverbWetLeft) ) != 0 ) __asm int 3
	if( memcmp( &ptr1->prerenderReverbWetRight, &ptr2->prerenderReverbWetRight, sizeof(ptr2->prerenderReverbWetRight) ) != 0 ) __asm int 3

	if( ptr1->prerenderReadIx != ptr2->prerenderReadIx ) __asm int 3
	if( ptr1->prerenderWriteIx != ptr2->prerenderWriteIx ) __asm int 3

	if( memcmp( &ptr1->myProp, &ptr2->myProp, sizeof(ptr2->myProp) ) != 0 ) __asm int 3



	// avoid destructor problems
	memset( ptr2, 0, sizeof(*ptr2) );
}


void Synth::rawVerifyPatchCache( PatchCache *ptr1, PatchCache *ptr2 )
{
	if( ptr1->dirty != ptr2->dirty ) __asm int 3
	if( ptr1->partialCount != ptr2->partialCount ) __asm int 3
	if( ptr1->partialParam != ptr2->partialParam ) __asm int 3
	if( ptr1->pcm != ptr2->pcm ) __asm int 3
	if( ptr1->PCMPartial != ptr2->PCMPartial ) __asm int 3
	if( ptr1->playPartial != ptr2->playPartial ) __asm int 3
	if( ptr1->reverb != ptr2->reverb ) __asm int 3
	if( memcmp( &ptr1->srcPartial, &ptr2->srcPartial, sizeof(ptr1->srcPartial) ) != 0 ) __asm int 3
	if( ptr1->structureMix != ptr2->structureMix ) __asm int 3
	if( ptr1->structurePair != ptr2->structurePair ) __asm int 3
	if( ptr1->structurePosition != ptr2->structurePosition ) __asm int 3
	if( ptr1->sustain != ptr2->sustain ) __asm int 3
	if( ptr1->waveform != ptr2->waveform ) __asm int 3
}
#endif


/*
NOTES:
ykhwong svn-daum munt - 2012-02-20

(*#) Synth.cpp = MT-32 top level
- (*#) MemoryRegion = static data
- (*#) Tables.cpp = static math tables

- (*#) SynthProperties

- (#) ReverbModel
  - (*#) AReverbModel.cpp (* not done *)
  - (#) DelayReverb.cpp
  - (#) FreeverbModel.cpp
		@ (#) freeverb (new/delete - check status)

	  - (#) freeverb\revmodel.cpp
		  - (#) freeverb\allpass.cpp
		  - (#) freeverb\comb.cpp

- PartialManager.cpp
  = parts (static fastptr - synth/parts)
	= synth (static fastptr - synth)

	= partialTable
		- Partial.cpp
			= synth (static fastptr - constructor - synth)

			> cachebackup/partialParam (reloc fastptr - (see part.cpp) )
			> pair (reloc fastptr - startPartial - synth/partialManager/partialTable[] or NULL)
			> patchCache (reloc fastptr - startPartial - synth/parts[]/patchCache or synth/parts[]/drumCache or NULL)
			> pcmWave (reloc fastptr - startPartial - synth/pcmWaves[] or NULL)
			> poly (reloc fastptr - startPartial - synth/parts[]/activePolys[])

			@ tva (static new - contructor)
			@ tvf (static new - contructor)
			@ tvp (static new - contructor)

			- LA32Ramp.cpp

			- TVA.cpp
				= ampRamp (static fastptr - constructor - synth/partialManager/partialTable[]/new LA32Ramp)
				= partial (static fastptr - constructor - synth/partialManager/partialTable[])
				= system (static fastptr - constructor - synth/mt32ram/system)

				> part (reloc fastptr - reset - synth/parts)
				> partialParam (reloc fastptr - reset - synth/mt32ram/timbres[]/timbre/partial[] -or- timbreTemp )
				> patchTemp (reloc fastptr - reset - synth/mt32ram/patchTemp[] or NULL)
			  > rhythmTemp (reloc fastptr - reset - synth/mt32ram/rhythmTemp[] or NULL)

			- TVF.cpp
				= cutoffModifierRamp (static fastptr - constructor - synth/partialManager/partialTable[]/new LA32Ramp)
				= partial (static fastptr - constructor - synth/partialManager/partialTable[])

				> partialParam (reloc fastptr - reset - synth/mt32ram/timbres[]/timbre/partial[] -or- timbreTemp )

			- TVP.cpp
				= partial (static fastptr - constructor - synth/partialManager/partialTable[])
				= system (static fastptr - constructor - synth/mt32ram/system)

				> part (reloc fastptr - reset - synth/parts)
				> partialParam (reloc fastptr - reset - synth/mt32ram/timbres[]/timbre/partial[] -or- timbreTemp )
				> patchTemp (reloc fastptr - reset - synth/mt32ram/patchTemp[] or NULL)

- Part.cpp
	= synth (static fastptr - synth)

	> PatchCache / partialParam (reloc fastptr - timbreTemp or synth/mt32ram/timbres[]/timbre/partial[])
	> PatchTemp (reloc fastptr - synth/mt32ram/patchTemp[])
	> timbreTemp (reloc fastptr - synth/mt32ram/timbreTemp[] or NULL)

	@ freePolys / activePolys (new/delete - check status)
	  (NOTE: PartialManager -uses- these lists // Polys created -here-)

		- Poly.cpp
			= part (static fastptr - synth/parts[])
			= synth (static fastptr - synth)

			> partials (reloc fastptr - synth/partialManager/partialTable[])


	(class RhythmPart::Part)
	= rhythmTemp (static fastptr - synth/mt32ram/rhythmTemp[])
	
	> PatchCache / drumCache[85][4] (reloc fastptr - timbreTemp or synth/mt32ram/timbres[]/timbre/partial[])
*/

void Synth::saveState( std::ostream &stream )
{
	Bit8u reverbModel_idx;


	reverbModel_idx = 0;
	for( int lcv=0; lcv<4; lcv++ ) {
		if( reverbModel == reverbModels[lcv] ) break;

		reverbModel_idx++;
	}

#ifdef WIN32_DEBUG
	// DEBUG
	if( reverbModel_idx == 4 ) __asm int 3
#endif

	// ====================================
	// ====================================
	// ====================================

#ifdef WIN32_DEBUG
	rawDumpNo = 0;
#endif


	// - static data
	//patchTempMemoryRegion->saveState(stream);
	//rhythmTempMemoryRegion->saveState(stream);
	//timbreTempMemoryRegion->saveState(stream);
	//patchesMemoryRegion->saveState(stream);
	//timbresMemoryRegion->saveState(stream);
	//systemMemoryRegion->saveState(stream);
	//displayMemoryRegion->saveState(stream);
	//resetMemoryRegion->saveState(stream);

	stream.write(reinterpret_cast<const char*>(paddedTimbreMaxTable), sizeof(MemParams::PaddedTimbre) * sizeof(Bit8u) );
	stream.write(reinterpret_cast<const char*>(&isEnabled), sizeof(isEnabled) );


	for( int lcv=0; lcv<controlROMMap->pcmCount; lcv++ ) {
		stream.write(reinterpret_cast<const char*>(&pcmWaves[lcv].addr), sizeof(pcmWaves[lcv].addr) );
		stream.write(reinterpret_cast<const char*>(&pcmWaves[lcv].len), sizeof(pcmWaves[lcv].len) );
		stream.write(reinterpret_cast<const char*>(&pcmWaves[lcv].loop), sizeof(pcmWaves[lcv].loop) );

		// - static fastptr
		//ControlROMPCMStruct *controlROMPCMStruct
	}


	// - static data
	//const ControlROMMap *controlROMMap;
	//Bit8u controlROMData[CONTROL_ROM_SIZE];
	//float *pcmROMData;
	//int pcmROMSize; // This is in 16-bit samples, therefore half the number of bytes in the ROM
	
	stream.write(reinterpret_cast<const char*>(&chantable), sizeof(chantable) );
	stream.write(reinterpret_cast<const char*>(&renderedSampleCount), sizeof(renderedSampleCount) );

	// - static data
	//tables->saveState(stream);

	stream.write(reinterpret_cast<const char*>(&mt32ram), sizeof(mt32ram) );
	stream.write(reinterpret_cast<const char*>(&mt32default), sizeof(mt32default) );


	for( int lcv=0; lcv<4; lcv++ ) {
		reverbModels[lcv]->saveState(stream);
	}


	// - reloc ptr (!!)
	//ReverbModel *reverbModel;
	stream.write(reinterpret_cast<const char*>(&reverbModel_idx), sizeof(reverbModel_idx) );


	stream.write(reinterpret_cast<const char*>(&reverbEnabled), sizeof(reverbEnabled) );
	stream.write(reinterpret_cast<const char*>(&reverbOverridden), sizeof(reverbOverridden) );

	// - static functions
	//FloatToBit16sFunc la32FloatToBit16sFunc;
	//FloatToBit16sFunc reverbFloatToBit16sFunc;

	stream.write(reinterpret_cast<const char*>(&outputGain), sizeof(outputGain) );
	stream.write(reinterpret_cast<const char*>(&reverbOutputGain), sizeof(reverbOutputGain) );
	stream.write(reinterpret_cast<const char*>(&isOpen), sizeof(isOpen) );


	// - WARNING: Must setup parts + poly --before-- manager (poly fastptrs)
	//PartialManager *partialManager;
	//Part *parts[9];

	for( int lcv=0; lcv<9; lcv++ ) {
		parts[lcv]->saveState(stream);
	}
	partialManager->saveState(stream);


	stream.write(reinterpret_cast<const char*>(&tmpBufPartialLeft), sizeof(tmpBufPartialLeft) );
	stream.write(reinterpret_cast<const char*>(&tmpBufPartialRight), sizeof(tmpBufPartialRight) );
	stream.write(reinterpret_cast<const char*>(&tmpBufMixLeft), sizeof(tmpBufMixLeft) );
	stream.write(reinterpret_cast<const char*>(&tmpBufMixRight), sizeof(tmpBufMixRight) );
	stream.write(reinterpret_cast<const char*>(&tmpBufReverbOutLeft), sizeof(tmpBufReverbOutLeft) );
	stream.write(reinterpret_cast<const char*>(&tmpBufReverbOutRight), sizeof(tmpBufReverbOutRight) );

	stream.write(reinterpret_cast<const char*>(&tmpNonReverbLeft), sizeof(tmpNonReverbLeft) );
	stream.write(reinterpret_cast<const char*>(&tmpNonReverbRight), sizeof(tmpNonReverbRight) );
	stream.write(reinterpret_cast<const char*>(&tmpReverbDryLeft), sizeof(tmpReverbDryLeft) );
	stream.write(reinterpret_cast<const char*>(&tmpReverbDryRight), sizeof(tmpReverbDryRight) );
	stream.write(reinterpret_cast<const char*>(&tmpReverbWetLeft), sizeof(tmpReverbWetLeft) );
	stream.write(reinterpret_cast<const char*>(&tmpReverbWetRight), sizeof(tmpReverbWetRight) );

	stream.write(reinterpret_cast<const char*>(&prerenderNonReverbLeft), sizeof(prerenderNonReverbLeft) );
	stream.write(reinterpret_cast<const char*>(&prerenderNonReverbRight), sizeof(prerenderNonReverbRight) );
	stream.write(reinterpret_cast<const char*>(&prerenderReverbDryLeft), sizeof(prerenderReverbDryLeft) );
	stream.write(reinterpret_cast<const char*>(&prerenderReverbDryRight), sizeof(prerenderReverbDryRight) );
	stream.write(reinterpret_cast<const char*>(&prerenderReverbWetLeft), sizeof(prerenderReverbWetLeft) );
	stream.write(reinterpret_cast<const char*>(&prerenderReverbWetRight), sizeof(prerenderReverbWetRight) );
	stream.write(reinterpret_cast<const char*>(&prerenderReadIx), sizeof(prerenderReadIx) );
	stream.write(reinterpret_cast<const char*>(&prerenderWriteIx), sizeof(prerenderWriteIx) );

	stream.write(reinterpret_cast<const char*>(&partialLimit), sizeof(partialLimit) );


	// - static data (user config)
	//myProp->saveState();


#ifdef WIN32_DEBUG
	// DEBUG
	rawDumpState( "temp-save", this, sizeof(*this) );
	rawDumpNo++;
#endif
}


void Synth::loadState( std::istream &stream )
{
	Bit8u reverbModel_idx;


#ifdef WIN32_DEBUG
	rawDumpNo = 0;
#endif


	// - static data
	//patchTempMemoryRegion->loadState(stream);
	//rhythmTempMemoryRegion->loadState(stream);
	//timbreTempMemoryRegion->loadState(stream);
	//patchesMemoryRegion->loadState(stream);
	//timbresMemoryRegion->loadState(stream);
	//systemMemoryRegion->loadState(stream);
	//displayMemoryRegion->loadState(stream);
	//resetMemoryRegion->loadState(stream);

	stream.read(reinterpret_cast<char*>(paddedTimbreMaxTable), sizeof(MemParams::PaddedTimbre) * sizeof(Bit8u) );
	stream.read(reinterpret_cast<char*>(&isEnabled), sizeof(isEnabled) );


	for( int lcv=0; lcv<controlROMMap->pcmCount; lcv++ ) {
		stream.read(reinterpret_cast<char*>(&pcmWaves[lcv].addr), sizeof(pcmWaves[lcv].addr) );
		stream.read(reinterpret_cast<char*>(&pcmWaves[lcv].len), sizeof(pcmWaves[lcv].len) );
		stream.read(reinterpret_cast<char*>(&pcmWaves[lcv].loop), sizeof(pcmWaves[lcv].loop) );

		// - static fastptr
		//ControlROMPCMStruct *controlROMPCMStruct
	}


	// - static data
	//const ControlROMMap *controlROMMap;
	//Bit8u controlROMData[CONTROL_ROM_SIZE];
	//float *pcmROMData;
	//int pcmROMSize; // This is in 16-bit samples, therefore half the number of bytes in the ROM
	
	stream.read(reinterpret_cast<char*>(&chantable), sizeof(chantable) );
	stream.read(reinterpret_cast<char*>(&renderedSampleCount), sizeof(renderedSampleCount) );

	// - static data
	//tables->loadState(stream);

	stream.read(reinterpret_cast<char*>(&mt32ram), sizeof(mt32ram) );
	stream.read(reinterpret_cast<char*>(&mt32default), sizeof(mt32default) );


	for( int lcv=0; lcv<4; lcv++ ) {
		reverbModels[lcv]->loadState(stream);
	}


	// - reloc ptr (!!)
	//ReverbModel *reverbModel;
	stream.read(reinterpret_cast<char*>(&reverbModel_idx), sizeof(reverbModel_idx) );
	reverbModel = reverbModels[reverbModel_idx];


	stream.read(reinterpret_cast<char*>(&reverbEnabled), sizeof(reverbEnabled) );
	stream.read(reinterpret_cast<char*>(&reverbOverridden), sizeof(reverbOverridden) );

	// - static functions
	//FloatToBit16sFunc la32FloatToBit16sFunc;
	//FloatToBit16sFunc reverbFloatToBit16sFunc;

	stream.read(reinterpret_cast<char*>(&outputGain), sizeof(outputGain) );
	stream.read(reinterpret_cast<char*>(&reverbOutputGain), sizeof(reverbOutputGain) );
	stream.read(reinterpret_cast<char*>(&isOpen), sizeof(isOpen) );


	// - WARNING: Must setup parts + poly --before-- manager (poly fastptrs)
	//PartialManager *partialManager;
	//Part *parts[9];

	for( int lcv=0; lcv<9; lcv++ ) {
		parts[lcv]->loadState(stream);
	}
	partialManager->loadState(stream);


	stream.read(reinterpret_cast<char*>(&tmpBufPartialLeft), sizeof(tmpBufPartialLeft) );
	stream.read(reinterpret_cast<char*>(&tmpBufPartialRight), sizeof(tmpBufPartialRight) );
	stream.read(reinterpret_cast<char*>(&tmpBufMixLeft), sizeof(tmpBufMixLeft) );
	stream.read(reinterpret_cast<char*>(&tmpBufMixRight), sizeof(tmpBufMixRight) );
	stream.read(reinterpret_cast<char*>(&tmpBufReverbOutLeft), sizeof(tmpBufReverbOutLeft) );
	stream.read(reinterpret_cast<char*>(&tmpBufReverbOutRight), sizeof(tmpBufReverbOutRight) );

	stream.read(reinterpret_cast<char*>(&tmpNonReverbLeft), sizeof(tmpNonReverbLeft) );
	stream.read(reinterpret_cast<char*>(&tmpNonReverbRight), sizeof(tmpNonReverbRight) );
	stream.read(reinterpret_cast<char*>(&tmpReverbDryLeft), sizeof(tmpReverbDryLeft) );
	stream.read(reinterpret_cast<char*>(&tmpReverbDryRight), sizeof(tmpReverbDryRight) );
	stream.read(reinterpret_cast<char*>(&tmpReverbWetLeft), sizeof(tmpReverbWetLeft) );
	stream.read(reinterpret_cast<char*>(&tmpReverbWetRight), sizeof(tmpReverbWetRight) );

	stream.read(reinterpret_cast<char*>(&prerenderNonReverbLeft), sizeof(prerenderNonReverbLeft) );
	stream.read(reinterpret_cast<char*>(&prerenderNonReverbRight), sizeof(prerenderNonReverbRight) );
	stream.read(reinterpret_cast<char*>(&prerenderReverbDryLeft), sizeof(prerenderReverbDryLeft) );
	stream.read(reinterpret_cast<char*>(&prerenderReverbDryRight), sizeof(prerenderReverbDryRight) );
	stream.read(reinterpret_cast<char*>(&prerenderReverbWetLeft), sizeof(prerenderReverbWetLeft) );
	stream.read(reinterpret_cast<char*>(&prerenderReverbWetRight), sizeof(prerenderReverbWetRight) );
	stream.read(reinterpret_cast<char*>(&prerenderReadIx), sizeof(prerenderReadIx) );
	stream.read(reinterpret_cast<char*>(&prerenderWriteIx), sizeof(prerenderWriteIx) );

	stream.read(reinterpret_cast<char*>(&partialLimit), sizeof(partialLimit) );


	// - static data (user config)
	//myProp->loadState();


#ifdef WIN32_DEBUG
	// DEBUG
	rawDumpState( "temp-load", this, sizeof(*this) );
	this->rawVerifyState( "temp-save", this );
	rawDumpNo++;
#endif
}

void *Synth::dumpRam() {
	return (void *) &mt32ram;
}


void Synth::loadRam( void *buf ) {
	memcpy( &mt32ram, buf, sizeof(mt32ram) );
}

}
