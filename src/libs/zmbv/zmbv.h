// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ZMBV_H
#define DOSBOX_ZMBV_H

#include <cstdint>
#include <vector>

#include "dosbox_config.h"

#if defined(C_SYSTEM_ZLIB_NG)
#include <zlib-ng.h>
#define deflateInit2 zng_deflateInit2
#define deflateReset zng_deflateReset
#define deflate zng_deflate
#define deflateEnd zng_deflateEnd
#define inflateInit zng_inflateInit
#define inflateReset zng_inflateReset
#define inflate zng_inflate
#define z_stream zng_stream
#else
#include <zlib.h>
#endif

#define CODEC_4CC "ZMBV"

enum class ZMBV_FORMAT : uint8_t {
	NONE = 0x00,
	BPP_1 = 0x01,
	BPP_2 = 0x02,
	BPP_4 = 0x03,
	BPP_8 = 0x04,
	BPP_15 = 0x05,
	BPP_16 = 0x06,
	BPP_24 = 0x07,
	BPP_32 = 0x08,
};

void Msg(const char fmt[], ...);

class VideoCodec {
private:
	struct FrameBlock {
		int start = 0;
		int dx = 0;
		int dy = 0;
	};
	struct CodecVector {
		int x = 0;
		int y = 0;
		int slot = 0;
	};
	struct KeyframeHeader {
		uint8_t high_version = 0;
		uint8_t low_version = 0;
		uint8_t compression = 0;
		uint8_t format = 0;
		uint8_t blockwidth = 0;
		uint8_t blockheight = 0;
	};

	struct Compress {
		int linesDone = 0;
		uint32_t writeSize = 0;
		uint32_t writeDone = 0;
		uint8_t *writeBuf = nullptr;
	};


	static constexpr uint8_t keyframeHeaderBytes = {sizeof(KeyframeHeader)};

	CodecVector VectorTable[512] = {};
	int VectorCount = 0;

	uint8_t *oldframe = nullptr;
	uint8_t *newframe = nullptr;
	std::vector<uint8_t> buf1 = {};
	std::vector<uint8_t> buf2 = {};
	std::vector<uint8_t> work = {};
	uint32_t bufsize = 0;

	std::vector<FrameBlock> blocks = {};
	size_t workUsed = 0;
	size_t workPos = 0;

	uint32_t palsize = 0;
	uint8_t palette[256 * 4] = {0};
	int height = 0;
	int width = 0;
	int pitch = 0;
	ZMBV_FORMAT format = ZMBV_FORMAT::NONE;
	int pixelsize = 0;

	Compress compress = {};
	z_stream zstream = {};

	// methods
	void CreateVectorTable();
	bool SetupBuffers(ZMBV_FORMAT format, int blockwidth, int blockheight);

	template <class P>
	void AddXorFrame();
	template <class P>
	void UnXorFrame();
	template <class P>
	int PossibleBlock(int vx, int vy, const FrameBlock & block);
	template <class P>
	int CompareBlock(int vx, int vy, const FrameBlock & block);
	template <class P>
	void AddXorBlock(int vx, int vy, const FrameBlock & block);
	template <class P>
	void UnXorBlock(int vx, int vy, const FrameBlock & block);
	template <class P>
	void CopyBlock(int vx, int vy, const FrameBlock & block);

	void AlignWork(size_t & offset);

public:
	VideoCodec();

	VideoCodec(const VideoCodec &) = delete;            // prevent copy
	VideoCodec &operator=(const VideoCodec &) = delete; // prevent assignment

	bool SetupCompress(int _width, int _height);
	bool SetupDecompress(int _width, int _height);
	ZMBV_FORMAT BPPFormat(int bpp);
	int NeededSize(int _width, int _height, ZMBV_FORMAT _format);

	void CompressLines(const int lineCount, const uint8_t *lineData[]);
	bool PrepareCompressFrame(int flags, ZMBV_FORMAT _format, const uint8_t *pal, uint8_t *writeBuf, uint32_t writeSize);
	int FinishCompressFrame();
	void FinishVideo();
	bool DecompressFrame(uint8_t *framedata, int size);
	void Output_UpsideDown_24(uint8_t *output);
};

uint8_t ZMBV_ToBytesPerPixel(const ZMBV_FORMAT format);

#endif
