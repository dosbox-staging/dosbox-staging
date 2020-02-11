#ifndef DOSBOX_MIDI_MT32_H
#define DOSBOX_MIDI_MT32_H

#include "midi_handler.h"

#include "mixer.h"

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>

#if MT32EMU_VERSION_MAJOR != 2 || MT32EMU_VERSION_MINOR < 1
#error Incompatible mt32emu library version
#endif

struct SDL_Thread;

class MidiHandler_mt32 : public MidiHandler {
public:
	static MidiHandler_mt32 &GetInstance();

	const char *GetName() const override { return "mt32"; }
	bool Open(const char *conf) override;
	void Close() override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;

private:
	MixerChannel *chan;
	MT32Emu::Service *service;
	SDL_Thread *thread;
	SDL_mutex *lock;
	SDL_cond *framesInBufferChanged;
	Bit16s *audioBuffer;
	Bitu audioBufferSize;
	Bitu framesPerAudioBuffer;
	Bitu minimumRenderFrames;
	volatile Bitu renderPos, playPos, playedBuffers;
	volatile bool stopProcessing;
	bool open, noise, renderInThread;

	static void mixerCallBack(Bitu len);
	static int processingThread(void *);
	static void makeROMPathName(char pathName[], const char romDir[], const char fileName[], bool addPathSeparator);
	static mt32emu_report_handler_i getReportHandlerInterface();

	MidiHandler_mt32();
	~MidiHandler_mt32();

	Bit32u inline getMidiEventTimestamp() {
		return service->convertOutputToSynthTimestamp(Bit32u(playedBuffers * framesPerAudioBuffer + (playPos >> 1)));
	}

	void handleMixerCallBack(Bitu len);
	void renderingLoop();
};

#endif /* DOSBOX_MIDI_MT32_H */
