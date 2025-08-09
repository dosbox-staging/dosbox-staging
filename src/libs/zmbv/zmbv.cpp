// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "zmbv.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "util/math_utils.h"
#include "util/mem_unaligned.h"
#include "support.h"
#include "checks.h"

CHECK_NARROWING();

constexpr uint8_t DBZV_VERSION_HIGH = 0;
constexpr uint8_t DBZV_VERSION_LOW  = 1;

constexpr uint8_t MAX_VECTOR = 16;

constexpr uint8_t Mask_KeyFrame     = 0x01;
constexpr uint8_t Mask_DeltaPalette = 0x02;

// Compression flags
constexpr uint8_t COMPRESSION_ZLIB     = 1;
constexpr int ZLIB_COMPRESSION_LEVEL   = 6;          // 0 to 9 (0 = no compression)
constexpr auto ZLIB_COMPRESSION_METHOD = Z_DEFLATED; // currently the only option
constexpr int ZLIB_MEM_LEVEL           = 9;          // 1 to 9 (default 8)
constexpr auto ZLIB_STRATEGY           = Z_FILTERED; // Z_DEFAULT_STRATEGY, Z_FILTERED,
                                                     // Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED

ZMBV_FORMAT BPPFormat(const int bpp)
{
	switch (bpp) {
	case 8: return ZMBV_FORMAT::BPP_8;
	case 15: return ZMBV_FORMAT::BPP_15;
	case 16: return ZMBV_FORMAT::BPP_16;
	case 24:
	case 32: return ZMBV_FORMAT::BPP_32;
	}
	return ZMBV_FORMAT::NONE;
}

uint8_t ZMBV_ToBytesPerPixel(const ZMBV_FORMAT format) {
	switch (format) {
	case ZMBV_FORMAT::BPP_8: return 1;

	case ZMBV_FORMAT::BPP_15:
	case ZMBV_FORMAT::BPP_16: return 2;

	case ZMBV_FORMAT::BPP_24:
	case ZMBV_FORMAT::BPP_32: return 4;

	default: assertm(false, "ZMBV: Unhandled format size"); break;
	}
	return 0;
}

int VideoCodec::NeededSize(const int _width, const int _height, const ZMBV_FORMAT _format)
{
	int f = ZMBV_ToBytesPerPixel(_format);

	f = f * _width * _height + 2 * (1 + (_width / 8)) * (1 + (_height / 8)) + 1024;

	return f + f / 1000;
}

bool VideoCodec::SetupBuffers(const ZMBV_FORMAT _format, const int blockwidth, const int blockheight)
{
	// Only BPP_8 is paletted
	palsize = (_format == ZMBV_FORMAT::BPP_8 ? 256 : 0);

	pixelsize = ZMBV_ToBytesPerPixel(_format);
	if (pixelsize == 0) {
		return false;
	}

	bufsize = static_cast<uint32_t>((height + 2 * MAX_VECTOR) * pitch * pixelsize + 2048);

	assert(bufsize > 0);
	const auto buf_sizes = static_cast<size_t>(bufsize);

	buf1 = std::vector<uint8_t>(buf_sizes, 0);
	buf2 = std::vector<uint8_t>(buf_sizes, 0);
	work = std::vector<uint8_t>(buf_sizes, 0);

	auto xblocks = (width / blockwidth);

	const auto xleft = width % blockwidth;
	if (xleft)
		xblocks++;

	auto yblocks = (height / blockheight);

	const auto yleft = height % blockheight;

	if (yleft)
		yblocks++;

	const auto blocks_needed = check_cast<uint32_t>(xblocks * yblocks);
	blocks.resize(blocks_needed);

	size_t i = 0;
	for (auto y = 0; y < yblocks; ++y) {
		for (auto x = 0; x < xblocks; ++x) {
			blocks[i].start = ((y * blockheight) + MAX_VECTOR) * pitch + (x * blockwidth) + MAX_VECTOR;
			if (xleft && x == (xblocks - 1)) {
				blocks[i].dx = xleft;
			} else {
				blocks[i].dx = blockwidth;
			}
			if (yleft && y == (yblocks - 1)) {
				blocks[i].dy = yleft;
			} else {
				blocks[i].dy = blockheight;
			}
			i++;
		}
	}

	oldframe = buf1.data();
	newframe = buf2.data();
	format   = _format;
	return true;
}

void VideoCodec::CreateVectorTable()
{
	VectorCount = 1;

	VectorTable[0].x = VectorTable[0].y = 0;
	for (auto s = 1; s <= 10; ++s) {
		for (auto y = 0 - s; y <= 0 + s; ++y) {
			for (auto x = 0 - s; x <= 0 + s; ++x) {
				if (abs(x) == s || abs(y) == s) {
					VectorTable[VectorCount].x = x;
					VectorTable[VectorCount].y = y;
					VectorCount++;
				}
			}
		}
	}
}

template <class P>
int VideoCodec::PossibleBlock(const int vx, const int vy, const FrameBlock & block)
{
	int ret = 0;
	P *pold = reinterpret_cast<P *>(oldframe) + block.start + (vy * pitch) + vx;
	P *pnew = reinterpret_cast<P *>(newframe) + block.start;
	;
	for (auto y = 0; y < block.dy; y += 4) {
		for (auto x = 0; x < block.dx; x += 4) {
			const auto test = 0 - ((pold[x] - pnew[x]) & 0x00ffffff);
			ret -= check_cast<int>(test >> 31);
		}
		pold += pitch * 4;
		pnew += pitch * 4;
	}
	return ret;
}

template <class P>
int VideoCodec::CompareBlock(const int vx, const int vy, const FrameBlock & block)
{
	int diff_count = 0;
	P *pold = reinterpret_cast<P *>(oldframe) + block.start + (vy * pitch) + vx;
	P *pnew = reinterpret_cast<P *>(newframe) + block.start;

	for (auto y = 0; y < block.dy; y++) {
		for (auto x = 0; x < block.dx; x++) {
			diff_count += ((pold[x] ^ pnew[x]) & 0x00ffffff) != 0;
		}
		pold += pitch;
		pnew += pitch;
	}
	return diff_count;
}

template <class P>
void VideoCodec::AddXorBlock(const int vx, const int vy, const FrameBlock & block)
{
	P *pold = reinterpret_cast<P *>(oldframe) + block.start + (vy * pitch) + vx;
	P *pnew = reinterpret_cast<P *>(newframe) + block.start;
	for (auto y = 0; y < block.dy; ++y) {
		for (auto x = 0; x < block.dx; ++x) {
			*reinterpret_cast<P *>(&work[workUsed]) = pnew[x] ^ pold[x];
			workUsed += sizeof(P);
		}
		pold += pitch;
		pnew += pitch;
	}
}

// align offset to the next 4-byte boundary
void VideoCodec::AlignWork(size_t & offset) {
	offset = (offset + blocks.size() * 2u + 3u) & ~3u;
}

template <class P>
void VideoCodec::AddXorFrame()
{
	auto vectors = &work[workUsed];

	AlignWork(workUsed);

	size_t b = 0;
	for (const auto & block : blocks) {

		int8_t bestvx   = 0;
		int8_t bestvy   = 0;
		auto bestchange = CompareBlock<P>(0, 0, block);
		auto possibles  = 64;

		for (auto v = 0; v < VectorCount && possibles; v++) {
			if (bestchange < 4)
				break;
			auto vx = VectorTable[v].x;
			auto vy = VectorTable[v].y;
			if (PossibleBlock<P>(vx, vy, block) < 4) {
				possibles--;
				// if (!possibles) Msg("Ran out of possibles, at
				// %d of %d best%d\n",v,VectorCount,bestchange);
				auto testchange = CompareBlock<P>(vx, vy, block);
				if (testchange < bestchange) {
					bestchange = testchange;
					bestvx     = check_cast<int8_t>(vx);
					bestvy     = check_cast<int8_t>(vy);
				}
			}
		}
		vectors[b * 2 + 0] = static_cast<uint8_t>(left_shift_signed(bestvx, 1));
		vectors[b * 2 + 1] = static_cast<uint8_t>(left_shift_signed(bestvy, 1));
		if (bestchange) {
			vectors[b * 2 + 0] |= 1;
			AddXorBlock<P>(bestvx, bestvy, block);
		}
		++b;
	}
}

bool VideoCodec::SetupCompress(const int _width, const int _height)
{
	width  = _width;
	height = _height;
	pitch  = _width + 2 * MAX_VECTOR;
	format = ZMBV_FORMAT::NONE;
	if (deflateInit2(&zstream, ZLIB_COMPRESSION_LEVEL, ZLIB_COMPRESSION_METHOD, ZLIB_MEM_LEVEL, ZLIB_MEM_LEVEL, ZLIB_STRATEGY) !=
	    Z_OK)
		return false;
	return true;
}

bool VideoCodec::SetupDecompress(const int _width, const int _height)
{
	width  = _width;
	height = _height;
	pitch  = _width + 2 * MAX_VECTOR;
	format = ZMBV_FORMAT::NONE;
	if (inflateInit(&zstream) != Z_OK)
		return false;
	return true;
}

bool VideoCodec::PrepareCompressFrame(int flags, const ZMBV_FORMAT _format, const uint8_t *pal, uint8_t *writeBuf,
                                      const uint32_t writeSize)
{
	if (_format != format) {
		if (!SetupBuffers(_format, 16, 16))
			return false;
		flags |= 1; // Force a keyframe
	}
	/* replace oldframe with new frame */
	auto copyFrame = newframe;
	newframe       = oldframe;
	oldframe       = copyFrame;

	compress.linesDone = 0;
	compress.writeSize = writeSize;
	compress.writeDone = 1;
	compress.writeBuf  = writeBuf;
	/* Set a pointer to the first byte which will contain info about this
	 * frame */
	auto &firstByte = compress.writeBuf[0];
	firstByte       = 0;

	// Reset the work buffer
	workUsed = 0;
	workPos  = 0;

	if (flags & 1) {
		/* Make a keyframe */
		firstByte |= Mask_KeyFrame;
		KeyframeHeader *header = (KeyframeHeader *)(compress.writeBuf + compress.writeDone);
		header->high_version   = DBZV_VERSION_HIGH;
		header->low_version    = DBZV_VERSION_LOW;
		header->compression    = COMPRESSION_ZLIB;

		// The public codec can't handle 24 bit content, so we convert
		// it to 32bit and indicate it in this format field.
		header->format = static_cast<uint8_t>(format == ZMBV_FORMAT::BPP_24 ? ZMBV_FORMAT::BPP_32 : format);
		header->blockwidth  = 16;
		header->blockheight = 16;
		compress.writeDone += keyframeHeaderBytes;
		/* Copy the new frame directly over */
		if (palsize) {
			if (pal)
				memcpy(&palette, pal, sizeof(palette));
			else
				memset(&palette, 0, sizeof(palette));
			/* keyframes get the full palette */
			for (size_t i = 0; i < palsize; i++) {
				work[workUsed++] = palette[i * 4 + 0];
				work[workUsed++] = palette[i * 4 + 1];
				work[workUsed++] = palette[i * 4 + 2];
			}
		}
		/* Restart deflate */
		deflateReset(&zstream);
	} else {
		const auto palette_bytes = palsize * 4;
		if (palsize && pal && memcmp(pal, palette, palette_bytes)) {
			firstByte |= Mask_DeltaPalette;
			for (size_t i = 0; i < palsize; i++) {
				work[workUsed++] = palette[i * 4 + 0] ^ pal[i * 4 + 0];
				work[workUsed++] = palette[i * 4 + 1] ^ pal[i * 4 + 1];
				work[workUsed++] = palette[i * 4 + 2] ^ pal[i * 4 + 2];
			}
			memcpy(&palette, pal, palette_bytes);
		}
	}
	return true;
}

void VideoCodec::CompressLines(const int lineCount, const uint8_t *lineData[])
{
	const auto linePitch = pitch * pixelsize;
	const auto lineWidth = width * pixelsize;

	auto destStart = newframe + pixelsize * (MAX_VECTOR + (compress.linesDone + MAX_VECTOR) * pitch);

	auto i = 0;
	while (i < lineCount && (compress.linesDone < height)) {
		memcpy(destStart, lineData[i], static_cast<size_t>(lineWidth));
		destStart += linePitch;
		i++;
		compress.linesDone++;
	}
}

int VideoCodec::FinishCompressFrame()
{
	assert(compress.writeBuf);
	const auto &firstByte = compress.writeBuf[0];
	if (firstByte & Mask_KeyFrame) {
		/* Add the full frame data */
		auto readFrame = newframe + pixelsize * (MAX_VECTOR + MAX_VECTOR * pitch);

		const int line_width = width * pixelsize;
		assert(line_width > 0);
		for (auto i = 0; i < height; ++i) {
			memcpy(&work[workUsed], readFrame, static_cast<size_t>(line_width));
			readFrame += pitch * pixelsize;
			workUsed += static_cast<uint32_t>(line_width);
		}
	} else {
		/* Add the delta frame data */
		switch (format) {
		case ZMBV_FORMAT::BPP_8: AddXorFrame<uint8_t>(); break;
		case ZMBV_FORMAT::BPP_15:
		case ZMBV_FORMAT::BPP_16: AddXorFrame<uint16_t>(); break;
		case ZMBV_FORMAT::BPP_24:
		case ZMBV_FORMAT::BPP_32: AddXorFrame<uint32_t>(); break;
		default: break;
		}
	}
	/* Create the actual frame with compression */
	zstream.next_in  = work.data();
	zstream.avail_in = check_cast<uint32_t>(workUsed);
	zstream.total_in = 0;

	zstream.next_out  = compress.writeBuf + compress.writeDone;
	zstream.avail_out = compress.writeSize - compress.writeDone;
	zstream.total_out = 0;

	int bytes_processed = 0;

	// Only tally bytes if the stream was OK
	if (deflate(&zstream, Z_SYNC_FLUSH) >= Z_OK) {
		bytes_processed = static_cast<int>(compress.writeDone + zstream.total_out);
	}
	return bytes_processed;
}

void VideoCodec::FinishVideo()
{
	// end the deflation stream
	deflateEnd(&zstream);
}

template <class P>
void VideoCodec::UnXorBlock(const int vx, const int vy, const FrameBlock & block)
{
	P *pold = reinterpret_cast<P *>(oldframe) + block.start + (vy * pitch) + vx;
	P *pnew = reinterpret_cast<P *>(newframe) + block.start;
	for (auto y = 0; y < block.dy; ++y) {
		for (auto x = 0; x < block.dx; ++x) {
			pnew[x] = pold[x] ^ *reinterpret_cast<P *>(&work[workPos]);
			workPos += sizeof(P);
		}
		pold += pitch;
		pnew += pitch;
	}
}

template <class P>
void VideoCodec::CopyBlock(const int vx, const int vy, const FrameBlock & block)
{
	P *pold = reinterpret_cast<P *>(oldframe) + block.start + (vy * pitch) + vx;
	P *pnew = reinterpret_cast<P *>(newframe) + block.start;
	for (auto y = 0; y < block.dy; ++y) {
		for (auto x = 0; x < block.dx; ++x) {
			pnew[x] = pold[x];
		}
		pold += pitch;
		pnew += pitch;
	}
}

template <class P>
void VideoCodec::UnXorFrame()
{
	auto vectors = &work[workPos];

	AlignWork(workPos);

	uint32_t b = 0;
	for (const auto & block : blocks) {
		const auto delta = vectors[b * 2 + 0] & 1;
		const auto vx    = vectors[b * 2 + 0] >> 1;
		const auto vy    = vectors[b * 2 + 1] >> 1;
		if (delta)
			UnXorBlock<P>(vx, vy, block);
		else
			CopyBlock<P>(vx, vy, block);
		++b;
	}
}

bool VideoCodec::DecompressFrame(uint8_t *framedata, int size)
{
	auto data = framedata;

	const auto tag = *data++;

	if (--size <= 0)
		return false;
	if (tag & Mask_KeyFrame) {
		KeyframeHeader *header = (KeyframeHeader *)data;

		size -= keyframeHeaderBytes;
		data += keyframeHeaderBytes;

		if (size <= 0)
			return false;
		if (header->low_version != DBZV_VERSION_LOW || header->high_version != DBZV_VERSION_HIGH)
			return false;
		if (format != (ZMBV_FORMAT)header->format &&
		    !SetupBuffers((ZMBV_FORMAT)header->format, header->blockwidth, header->blockheight))
			return false;
		if (inflateReset(&zstream) != Z_OK)
			return false;
	}
	zstream.next_in  = data;
	zstream.avail_in = check_cast<uint32_t>(size);
	zstream.total_in = 0;

	zstream.next_out  = work.data();
	zstream.avail_out = bufsize;
	zstream.total_out = 0;

	// decompress all the pending data. Note that if Z_FINISH isn't used,
	// then inflate can also return Z_OK, which indicates multiple inflation
	// passes are needed.
	if (inflate(&zstream, Z_FINISH) != Z_STREAM_END)
		return false;

	workUsed = check_cast<uint32_t>(zstream.total_out);
	workPos  = 0;
	if (tag & Mask_KeyFrame) {
		if (palsize) {
			for (size_t i = 0; i < palsize; ++i) {
				palette[i * 4 + 0] = work[workPos++];
				palette[i * 4 + 1] = work[workPos++];
				palette[i * 4 + 2] = work[workPos++];
			}
		}
		newframe = buf1.data();
		oldframe = buf2.data();

		auto writeframe       = newframe + pixelsize * (MAX_VECTOR + MAX_VECTOR * pitch);
		const auto line_width = check_cast<uint32_t>(width * pixelsize);
		assert(line_width > 0);
		for (auto i = 0; i < height; ++i) {
			memcpy(writeframe, &work[workPos], line_width);
			writeframe += pitch * pixelsize;
			workPos += line_width;
		}
	} else {
		data     = oldframe;
		oldframe = newframe;
		newframe = data;
		if (tag & Mask_DeltaPalette) {
			for (size_t i = 0; i < palsize; ++i) {
				palette[i * 4 + 0] = check_cast<uint8_t>(palette[i * 4 + 0] ^ work[workPos++]);
				palette[i * 4 + 1] = check_cast<uint8_t>(palette[i * 4 + 1] ^ work[workPos++]);
				palette[i * 4 + 2] = check_cast<uint8_t>(palette[i * 4 + 2] ^ work[workPos++]);
			}
		}
		switch (format) {
		case ZMBV_FORMAT::BPP_8: UnXorFrame<uint8_t>(); break;
		case ZMBV_FORMAT::BPP_15:
		case ZMBV_FORMAT::BPP_16: UnXorFrame<uint16_t>(); break;
		case ZMBV_FORMAT::BPP_24:
		case ZMBV_FORMAT::BPP_32: UnXorFrame<uint32_t>(); break;
		default: break;
		}
	}
	return true;
}

void VideoCodec::Output_UpsideDown_24(uint8_t *output)
{
	auto w  = output;

	assert(width >= 0);
	const auto line_width = static_cast<uint32_t>(width);

	int pad = line_width & 3;

	for (auto i = height - 1; i >= 0; --i) {
		auto r = newframe + pixelsize * (MAX_VECTOR + (i + MAX_VECTOR) * pitch);
		switch (format) {
		case ZMBV_FORMAT::BPP_8:
			for (uint32_t j = 0; j < line_width; j++) {
				const auto c = r[j];

				*w++ = palette[c * 4 + 2];
				*w++ = palette[c * 4 + 1];
				*w++ = palette[c * 4 + 0];
			}
			break;
		case ZMBV_FORMAT::BPP_15:
			for (uint32_t j = 0; j < line_width; ++j) {
				const auto c = read_unaligned_uint16_at(r, j);

				*w++ = check_cast<uint8_t>(((c & 0x001f) * 0x21) >> 2);
				*w++ = check_cast<uint8_t>(((c & 0x03e0) * 0x21) >> 7);
				*w++ = check_cast<uint8_t>(((c & 0x7c00) * 0x21) >> 12);
			}
			break;
		case ZMBV_FORMAT::BPP_16:
			for (uint32_t j = 0; j < line_width; ++j) {
				const auto c = read_unaligned_uint16_at(r, j);

				*w++ = check_cast<uint8_t>(((c & 0x001f) * 0x21) >> 2);
				*w++ = check_cast<uint8_t>(((c & 0x07e0) * 0x41) >> 9);
				*w++ = check_cast<uint8_t>(((c & 0xf800) * 0x21) >> 13);
			}
			break;
		case ZMBV_FORMAT::BPP_24:
		case ZMBV_FORMAT::BPP_32:
			for (uint32_t j = 0; j < line_width; ++j) {
				*w++ = r[j * 4 + 0];
				*w++ = r[j * 4 + 1];
				*w++ = r[j * 4 + 2];
			}
			break;
		default: break;
		}

		// Maintain 32-bit alignment for scanlines.
		w += pad;
	}
}

VideoCodec::VideoCodec()
{
	CreateVectorTable();
	memset(&zstream, 0, sizeof(zstream));
}
