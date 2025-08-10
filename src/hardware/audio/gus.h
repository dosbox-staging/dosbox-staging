// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio/mixer.h"
#include "hardware/dma.h"
#include "util/rwqueue.h"

#include <queue>

// Global Constants
// ----------------

constexpr auto GusOutputSampleRate = 44100;

// AdLib emulation state constant
constexpr uint8_t ADLIB_CMD_DEFAULT = 85;

// Environment variable names
const auto ultrasnd_env_name = "ULTRASND";
const auto ultradir_env_name = "ULTRADIR";

// Buffer and memory constants
constexpr uint32_t RAM_SIZE = 1024 * 1024; // 1 MB

// DMA transfer size and rate constants
constexpr uint32_t BYTES_PER_DMA_XFER = 8 * 1024;         // 8 KB per transfer
constexpr uint32_t ISA_BUS_THROUGHPUT = 32 * 1024 * 1024; // 32 MB/s
constexpr uint16_t DMA_TRANSFERS_PER_S = ISA_BUS_THROUGHPUT / BYTES_PER_DMA_XFER;
constexpr double MS_PER_DMA_XFER = MillisInSecond / DMA_TRANSFERS_PER_S;

// Voice-channel and state related constants
constexpr uint8_t MAX_VOICES          = 32;
constexpr uint8_t MIN_VOICES          = 14;
constexpr uint8_t VOICE_DEFAULT_STATE = 3;

// IRQ and DMA address lookup tables described in UltraSound Software
// Development Kit (SDK), sections 2.14 and 2.15. These tables are used for
// validation and are also read by the address selector IO call (0x20b). Their
// starting zero values and subsequent order are important (don't truncate or
// re-order their values)

constexpr std::array<uint8_t, 8> IrqAddresses = {0, 2, 5, 3, 7, 11, 12, 15};

constexpr std::array<uint8_t, 6> DmaAddresses = {0, 1, 3, 5, 6, 7};

// Pan position constants
constexpr uint8_t PAN_DEFAULT_POSITION = 7;
constexpr uint8_t PAN_POSITIONS = 16; // 0: -45-deg, 7: centre, 15: +45-deg

// Timer delay constants
constexpr double TIMER_1_DEFAULT_DELAY = 0.080;
constexpr double TIMER_2_DEFAULT_DELAY = 0.320;

// Volume scaling and dampening constants
constexpr auto DELTA_DB             = 0.002709201; // 0.0235 dB increments
constexpr int16_t VOLUME_INC_SCALAR = 512; // Volume index increment scalar
constexpr uint16_t VOLUME_LEVELS    = 4096;

// Interwave addressing constant
constexpr int16_t WAVE_WIDTH = 1 << 9; // Wave interpolation width (9 bits)

// IO address quantities
constexpr uint8_t READ_HANDLERS  = 8;
constexpr uint8_t WRITE_HANDLERS = 9;

// A group of parameters defining the Gus's voice IRQ control that's also shared
// (as a reference) into each instantiated voice.
struct VoiceIrq {
	uint32_t vol_state  = 0;
	uint32_t wave_state = 0;
	uint8_t status      = 0;
};

// A group of parameters used in the Voice class to track the Wave and Volume
// controls.
struct VoiceCtrl {
	uint32_t& irq_state;
	int32_t start = 0;
	int32_t end   = 0;
	int32_t pos   = 0;
	int32_t inc   = 0;
	uint16_t rate = 0;
	uint8_t state = VOICE_DEFAULT_STATE;
};

enum class SampleSize { Bits8, Bits16 };

// Collection types involving constant quantities
using pan_scalars_array_t = std::array<AudioFrame, PAN_POSITIONS>;
using ram_array_t         = std::vector<uint8_t>;
using read_io_array_t     = std::array<IO_ReadHandleObject, READ_HANDLERS>;
using vol_scalars_array_t = std::array<float, VOLUME_LEVELS>;
using write_io_array_t    = std::array<IO_WriteHandleObject, WRITE_HANDLERS>;

// A Voice is used by the Gus class and instantiates 32 of these.
// Each voice represents a single "mono" stream of audio having its own
// characteristics defined by the running program, such as:
//   - being 8bit or 16bit
//   - having a "position" along a left-right axis (panned)
//   - having its volume reduced by some amount (native-level down to 0)
//   - having start, stop, loop, and loop-backward controls
//   - informing the GUS DSP as to when an IRQ is needed to keep it playing
//
class Voice {
public:
	Voice(uint8_t num, VoiceIrq& irq) noexcept;
	Voice(Voice&&) = default;

	void RenderFrames(const ram_array_t& ram,
	                  const vol_scalars_array_t& vol_scalars,
	                  const pan_scalars_array_t& pan_scalars,
	                  std::vector<AudioFrame>& frames);

	uint8_t ReadVolState() const noexcept;
	uint8_t ReadWaveState() const noexcept;
	void ResetCtrls() noexcept;
	void WritePanPot(uint8_t pos) noexcept;
	void WriteVolRate(uint16_t rate) noexcept;
	void WriteWaveRate(uint16_t rate) noexcept;
	bool UpdateVolState(uint8_t state) noexcept;
	bool UpdateWaveState(uint8_t state) noexcept;

	VoiceCtrl vol_ctrl;
	VoiceCtrl wave_ctrl;

	uint32_t generated_8bit_ms  = 0;
	uint32_t generated_16bit_ms = 0;

private:
	Voice()                        = delete;
	Voice(const Voice&)            = delete; // prevent copying
	Voice& operator=(const Voice&) = delete; // prevent assignment
	bool CheckWaveRolloverCondition() noexcept;
	bool Is16Bit() const noexcept;
	float GetVolScalar(const vol_scalars_array_t& vol_scalars);
	float GetSample(const ram_array_t& ram) noexcept;
	int32_t PopWavePos() noexcept;
	float PopVolScalar(const vol_scalars_array_t& vol_scalars);
	float Read8BitSample(const ram_array_t& ram, int32_t addr) const noexcept;
	float Read16BitSample(const ram_array_t& ram, int32_t addr) const noexcept;
	uint8_t ReadCtrlState(const VoiceCtrl& ctrl) const noexcept;
	void IncrementCtrlPos(VoiceCtrl& ctrl, bool skip_loop) noexcept;
	bool UpdateCtrlState(VoiceCtrl& ctrl, uint8_t state) noexcept;

	// Control states
	enum CTRL : uint8_t {
		RESET         = 0x01,
		STOPPED       = 0x02,
		DISABLED      = RESET | STOPPED,
		BIT16         = 0x04,
		LOOP          = 0x08,
		BIDIRECTIONAL = 0x10,
		RAISEIRQ      = 0x20,
		DECREASING    = 0x40,
	};

	uint32_t irq_mask = 0;
	uint8_t& shared_irq_status;
	uint8_t pan_position = PAN_DEFAULT_POSITION;
};

// DRAM DMA Control Register (41h), section 2.6.1.1, page 12, of the UltraSound
// Software Development Kit (SDK) Version 2.22
//
union DmaControlRegister {
	uint8_t data = 0;
	bit_view<0, 1> is_enabled;
	bit_view<1, 1> is_direction_gus_to_host;
	bit_view<2, 1> is_channel_16bit;
	bit_view<3, 2> rate_divisor;
	bit_view<5, 1> wants_irq_on_terminal_count;

	// Note that bit 6's function differs when written versus read. When
	// written, bit 6 indicates that the DMA transfer's samples are 16-bit
	// (or 8-bit, if cleared). However when read, bit 6 indicates if a
	// terminal count IRQ is pending. There's no way for the application to
	// read back the 16-bit sample indicator; it's a one-shot write into the
	// GUS that sizes the DMA routine.
	//
	bit_view<6, 1> are_samples_16bit;
	bit_view<6, 1> has_pending_terminal_count_irq;

	bit_view<7, 1> are_samples_high_bit_inverted;
};

// Reset Register (4Ch), section 2.6.1.9, page 16, of the UltraSound Software
// Development Kit (SDK) Version 2.22
union ResetRegister {
	uint8_t data = 0;

	// 0 to stop and reset the card, 1 to start running.
	bit_view<0, 1> is_running;

	// 1 to use the DAC. The DACs will not run unless this is set.
	bit_view<1, 1> is_dac_enabled;

	// 1 to enable the card's IRQs. Must be set to get any of the
	// GF1-generated IRQs such as wavetable, volume, voices, etc.
	bit_view<2, 1> are_irqs_enabled;
};

// Default state: lines disabled, latches enabled
constexpr uint8_t MixControlRegisterDefaultState = 0b0000'1011;

// Mix Control Register (2X0), section 2.13, page 28, of the UltraSound Software
// Development Kit (SDK) Version 2.22
union MixControlRegister {
	uint8_t data = MixControlRegisterDefaultState;

	bit_view<0, 1> line_in_disabled;
	bit_view<1, 1> line_out_disabled;
	bit_view<2, 1> microphone_enabled;
	bit_view<3, 1> latches_enabled;
	bit_view<4, 1> channel1_irq_combined_with_channel2;
	bit_view<5, 1> midi_loopback_enabled;
	bit_view<6, 1> irq_control_selected;
	// bit 7 reserved
};

// IRQ and DMA Control Select Register (2XB), section 2.14-2.15, page 28-30, of
// the UltraSound Software Development Kit (SDK) Version 2.22
union AddressSelectRegister {
	uint8_t data = 0;
	bit_view<0, 3> channel1_selector;
	bit_view<3, 3> channel2_selector;
	// Note: If the channels are sharing, then channel 2's IRQ selector must
	// be set to 0 and bit 6 must be enabled.
	bit_view<6, 1> channel2_combined_with_channel1;
	// bit 7 reserved
};

// The Gravis UltraSound GF1 DSP (classic)
// This class:
//   - Registers, receives, and responds to port address inputs, which are used
//     by the emulated software to configure and control the GUS card.
//   - Reads or provides audio samples via direct memory access (DMA)
//   - Provides shared resources to all of the Voices, such as the volume
//     reducing table, constant-power panning table, and IRQ states.
//   - Accumulates the audio from each active voice into a floating point
//     audio frame.
//   - Populates an autoexec line (ULTRASND=...) with its port, irq, and dma
//     addresses.
//
class Gus {
public:
	Gus(const io_port_t port_pref, const uint8_t dma_pref, const uint8_t irq_pref,
	    const char* ultradir, const std::string& filter_prefs);

	virtual ~Gus();

	bool CheckTimer(size_t t);
	void MirrorAdLibCommandRegister(const uint8_t reg_value);
	void PrintStats();
	void PicCallback(const int requested_frames);

	struct Timer {
		double delay          = 0.0;
		uint8_t value         = 0xff;
		bool has_expired      = true;
		bool is_counting_down = false;
		bool is_masked        = false;
		bool should_raise_irq = false;
	};
	Timer timer_one = {TIMER_1_DEFAULT_DELAY};
	Timer timer_two = {TIMER_2_DEFAULT_DELAY};

	float frame_counter     = 0.0f;
	MixerChannelPtr channel = nullptr;
	RWQueue<AudioFrame> output_queue {1};

	std::function<bool()> PerformDmaTransfer = {};

private:
	Gus()                      = delete;
	Gus(const Gus&)            = delete; // prevent copying
	Gus& operator=(const Gus&) = delete; // prevent assignment

	void ActivateVoices(uint8_t requested_voices);
	void CheckIrq();
	void CheckVoiceIrq();
	uint32_t GetDmaOffset() noexcept;
	void UpdateDmaAddr(uint32_t offset) noexcept;
	void DmaCallback(const DmaChannel* chan, DmaEvent event);
	void StartDmaTransfers();

	template <SampleSize sample_size>
	bool SizedDmaTransfer();

	bool IsDmaXfer16Bit() noexcept;
	uint16_t ReadFromRegister();

	void SetupEnvironment(const uint16_t port, const char* ultradir_env_val);
	void ClearEnvironment();

	void PopulatePanScalars() noexcept;
	void PopulateVolScalars() noexcept;
	uint16_t ReadFromPort(io_port_t port, io_width_t width);

	void RegisterIoHandlers();

	const std::vector<AudioFrame>& RenderFrames(const int num_requested_frames);

	void Reset() noexcept;

	void RenderUpToNow();
	void UpdatePlaybackDmaAddress(const uint8_t new_address);
	void UpdateRecordingDmaAddress(const uint8_t new_address);
	void UpdateWaveMsw(int32_t& addr) const noexcept;
	void UpdateWaveLsw(int32_t& addr) const noexcept;
	void WriteToPort(io_port_t port, io_val_t value, io_width_t width);

	void WriteToRegister();

	// Collections
	std::queue<AudioFrame> fifo             = {};
	vol_scalars_array_t vol_scalars         = {{}};
	pan_scalars_array_t pan_scalars         = {{}};
	ram_array_t ram                         = {};
	read_io_array_t read_handlers           = {};
	write_io_array_t write_handlers         = {};
	std::vector<Voice> voices               = {};
	std::vector<AudioFrame> rendered_frames = {};
	std::mutex mutex                        = {};

	// Struct and pointer members
	VoiceIrq voice_irq            = {};
	Voice* target_voice           = nullptr;
	DmaChannel* dma_channel       = nullptr;

	// Playback related
	double last_rendered_ms = 0.0;
	double ms_per_render    = 0.0;
	int sample_rate_hz      = 0;

	uint8_t adlib_command_reg = ADLIB_CMD_DEFAULT;

	// Port address
	io_port_t port_base = 0;

	// Voice states
	uint32_t active_voice_mask = 0;
	uint16_t voice_index       = 0;
	uint8_t active_voices      = 0;
	uint8_t prev_logged_voices = 0;

	// RAM and register data
	uint32_t dram_addr        = 0;
	uint16_t register_data    = 0;
	uint8_t selected_register = 0;

	// Control states
	uint8_t sample_ctrl = 0;
	uint8_t timer_ctrl  = 0;

	// DMA states
	uint16_t dma_addr       = 0;
	uint8_t dma_addr_nibble = 0;

	uint8_t dma1      = 0; // playback DMA
	uint8_t dma2      = 0; // recording DMA

	// IRQ states
	uint8_t irq1       = 0; // playback IRQ
	uint8_t irq2       = 0; // MIDI IRQ
	uint8_t irq_status = 0;

	DmaControlRegister dma_control_register = {};
	ResetRegister reset_register = {};
	MixControlRegister mix_control_register = {};

	bool irq_previously_interrupted = false;
	bool should_change_irq_dma      = false;
};
