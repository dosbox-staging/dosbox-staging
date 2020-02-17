/*
 *  Copyright (C) 2002-2019  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dosbox.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "mem.h"
#include "mapper.h"
#include "pic.h"
#include "render.h"
#include "cross.h"

#if (C_SSHOT)
#include <png.h>
#endif
#if (C_SRECORD)
#include "../libs/zmbv/zmbv.cpp"
#endif

static std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState;

static const char version_text[] = "DOSBox " VERSION;

#define WAVE_BUF 16*1024
#define MIDI_BUF 4*1024

#if (C_SRECORD)

#define AVII_KEYFRAME             (1<<4)
#define AVIF_HASINDEX             (1<<4)
#define AVIF_ISINTERLEAVED        (1<<8)

#define AVI_MAX_SIZE              0x7FFFFFFF

struct AVITag {
	char sTag[4];
	AVITag(const char *n) {
		memcpy(sTag, n, sizeof(sTag));
	}
};

struct AVIChunk : AVITag {
	Bit32u dwSize;
	AVIChunk(const char *n, size_t s=AVI_MAX_SIZE)
	: AVITag(n) {
		var_write(&dwSize, (Bit32u)s);
	}
};

// utility class to set dwSize automatically
template <typename T>
struct AVIChunkT : AVIChunk {
	AVIChunkT(const char *n)
	: AVIChunk(n,sizeof(T)-sizeof(AVIChunk)) {}
};

struct AVIStreamFormatAudio {
	AVIChunkT<AVIStreamFormatAudio> ck;
	Bit16u wFormatTag;
	Bit16u nChannels;
	Bit32u nSamplesPerSec;
	Bit32u nAvgBytesPerSec;
	Bit16u nBlockAlign;
	Bit16u wBitsPerSample;
	AVIStreamFormatAudio() : ck("strf") {
		var_write(&wFormatTag, 1);
		var_write(&nChannels, 2);
		var_write(&nBlockAlign, 4);
		var_write(&wBitsPerSample, 16);
	}
};

struct AVIStreamFormatVideo {
	AVIChunkT<AVIStreamFormatVideo> ck;
	Bit32u biSize;
	Bit32u biWidth;
	Bit32u biHeight;
	Bit16u biPlanes;
	Bit16u biBitCount;
	Bit32u biCompression;
	Bit32u biSizeImage;
	Bit32u biXPelsPerMeter;
	Bit32u biYPelsPerMeter;
	Bit32u biClrUsed;
	Bit32u biClrImportant;
	AVIStreamFormatVideo() : ck("strf") {
		biSize = ck.dwSize;
		biPlanes = 0;
		biBitCount = 0;
		memcpy(&biCompression, CODEC_4CC, 4);
		biXPelsPerMeter = 0;
		biYPelsPerMeter = 0;
		biClrUsed = 0;
		biClrImportant = 0;
	}
};

struct AVIStreamHeader {
	AVIChunkT<AVIStreamHeader> ck;
	AVITag sFCC;
	Bit32u sFCCHandler;
	Bit32u dwFlags;
	Bit16u wPriority;
	Bit16u wLanguage;
	Bit32u dwInitialFrames;
	Bit32u dwScale;
	Bit32u dwRate;
	Bit32u dwStart;
	Bit32u dwLength;
	Bit32u dwSuggestedBufferSize;
	Bit32u dwQuality;
	Bit32u dwSampleSize;
	Bit16u wLeft,wTop,wRight,wBottom;
	AVIStreamHeader(const char *n) : ck("strh"), sFCC(n) {
		dwFlags = 0;
		wPriority = 0;
		wLanguage = 0;
		dwInitialFrames = 0;
		dwStart = 0;
		dwLength = 0xFFFFFFFF;
		dwSuggestedBufferSize = 0;
		dwQuality = 0xFFFFFFFF;
		wLeft = wTop = 0;
	}
};

struct AVIStreamHeaderVideo : AVIStreamHeader {
	AVIStreamHeaderVideo() : AVIStreamHeader("vids") {
		memcpy(&sFCCHandler, CODEC_4CC, 4);
		var_write(&dwScale, 1<<24);
		dwSampleSize = 0;
	}
};

struct AVIStreamHeaderAudio : AVIStreamHeader {
	AVIStreamHeaderAudio() : AVIStreamHeader("auds") {
		sFCCHandler = 0;
		var_write(&dwScale, 1);
		var_write(&dwSampleSize, 4);
		wRight = wBottom = 0;
	}
};

struct AVIMainHeader {
	AVIChunkT<AVIMainHeader> ck;
	Bit32u dwMicroSecPerFrame;
	Bit32u dwMaxBytesPerSec;
	Bit32u dwPaddingGranularity;
	Bit32u dwFlags;
	Bit32u dwTotalFrames;
	Bit32u dwInitialFrames;
	Bit32u dwStreams;
	Bit32u dwSuggestedBufferSize;
	Bit32u dwWidth;
	Bit32u dwHeight;
	Bit32u dwReserved[4];
	AVIMainHeader() : ck("avih") {
		dwMaxBytesPerSec = 0;
		dwPaddingGranularity = 0;
		var_write(&dwFlags, AVIF_ISINTERLEAVED);
		dwTotalFrames = 0xFFFFFFFF;
		dwInitialFrames = 0;
		var_write(&dwStreams, 2);
		dwSuggestedBufferSize = 0;
		dwReserved[0]=dwReserved[1]=dwReserved[2]=dwReserved[3]=0;
	}
};

template <typename T>
struct AVIList {
	AVIChunkT<T> ck;
	AVITag sName;
	AVIList(const char *n) : ck("LIST"), sName(n) {}
};

/* movi list - special case, initial size is MAX */
struct AVIListMovi : AVIChunk {
	AVITag sName;
	AVIListMovi() : AVIChunk("LIST"),sName("movi") {}
};

template <class htype,class ftype>
struct AVIListStream {
	AVIList<AVIListStream> lst;
	htype hdr;
	ftype fmt;
	AVIListStream() : lst("strl") {}
};

struct AVIListHeader {
	AVIList<AVIListHeader> lst;
	AVIMainHeader hdr;
	AVIListStream<AVIStreamHeaderVideo,AVIStreamFormatVideo> vid;
	AVIListStream<AVIStreamHeaderAudio,AVIStreamFormatAudio> aud;
	AVIListHeader() : lst("hdrl") {}
};

/* Interesting metadata tags:
 * "ICMT" Comment/Description
 * "ICRD" Date
 * "INAM" Title
 * "ISFT" Sofware/Encoder (not read by many programs, so using ICMT here instead)
 * "IPRD" Product
 * "ISBJ" Subject
 * "ISRC" Source
 */
template <size_t len>
struct AVIMeta {
	AVIChunkT<AVIMeta> ck;
	char aTag[len];
	AVIMeta(const char *n, const char *s) : ck(n) {
		Bitu i;
		for (i=0; i < sizeof(aTag)-1; i++) {
			aTag[i] = *s;
			if (*s) ++s;
		}
		aTag[i] = '\0';
	}
};

struct AVIListInfo {
	AVIList<AVIListInfo> lst;
	// multiple of 4 to keep dword alignment
	AVIMeta<(sizeof(version_text)+3)&~3> comment;
	AVIListInfo() : lst("INFO"),
		comment("ICMT",version_text) {}
};

struct AVIRIFF : AVIChunk {
	AVITag sName;
	AVIListHeader hdr;
	AVIListInfo info;
	AVIListMovi movi;
	AVIRIFF() : AVIChunk("RIFF"), sName("AVI ") {}
};

struct AVIIndexEntry {
	AVITag dwChunkId;
	Bit32u dwFlags;
	Bit32u dwOffset;
	Bit32u dwSize;
	AVIIndexEntry(const AVIChunk &ck, Bit32u flags, size_t offset)
	: dwChunkId(ck) {
		var_write(&dwFlags, flags);
		var_write(&dwOffset, (Bit32u)offset);
		dwSize = ck.dwSize;
	}
};

class AVIFILE {
private:
	AVIRIFF riff;
	std::vector<AVIIndexEntry> idx;
	size_t data_length;
	FILE *handle;
	Bitu samples;
	Bit32u freq;
	size_t buffer_size[2];
	/* set new sampling rate, returns true if changed */
	bool SetFreq(Bit32u newfreq) {
		if (freq == newfreq) return false;
		freq = newfreq;
		var_write(&riff.hdr.aud.hdr.dwRate, freq);
		riff.hdr.aud.fmt.nSamplesPerSec = riff.hdr.aud.hdr.dwRate;
		var_write(&riff.hdr.aud.fmt.nAvgBytesPerSec, 4*freq);
		return true;
	}
	bool AddChunk(const AVIChunk &ck, const AVIIndexEntry &entry, const void *data, size_t length) {
		long pos = ftell(handle); // in case writing fails
		if (fwrite(&ck,sizeof(ck),1,handle)==1 && \
		    fwrite(data,1,length,handle)==length)
		{
			if (length&1) { // chunks must be aligned to 2-bytes
				fseek(handle,1,SEEK_CUR);
				length++;
			} else fflush(handle);
			idx.push_back(entry);
			data_length += length+sizeof(ck);
			return true;
		}

		fseek(handle, pos, SEEK_SET);
		return false;
	}
public:
	Bit32u frames;

	AVIFILE(FILE* _handle, Bitu width, Bitu height, float fps) : handle(_handle) {
		data_length = sizeof(AVITag); // "movi" tag
		frames = 0;
		samples = 0;
		freq = 0;
		buffer_size[0]=buffer_size[1] = 0;
		idx.reserve(4096);

		var_write(&riff.hdr.hdr.dwMicroSecPerFrame, (Bit32u)(1000000/fps));
		var_write(&riff.hdr.hdr.dwWidth, (Bit32u)width);
		var_write(&riff.hdr.hdr.dwHeight, (Bit32u)height);
		var_write(&riff.hdr.vid.hdr.dwRate, (Bit32u)((1<<24)*fps));
		var_write(&riff.hdr.vid.hdr.wRight, (Bit16u)width);
		var_write(&riff.hdr.vid.hdr.wBottom, (Bit16u)height);
		riff.hdr.vid.fmt.biWidth = riff.hdr.hdr.dwWidth;
		riff.hdr.vid.fmt.biHeight = riff.hdr.hdr.dwHeight;
		var_write(&riff.hdr.vid.fmt.biSizeImage, (Bit32u)(width*height*4));

		SetFreq(44100); // guess, don't know the frequency until first audio block is written
		fwrite(&riff, sizeof(riff), 1, handle);
	}
	~AVIFILE() {
		AVIChunk idx1("idx1",idx.size()*sizeof(AVIIndexEntry));

		var_write(&riff.movi.dwSize, (Bit32u)data_length);
		var_write(&riff.hdr.hdr.dwTotalFrames, frames);
		riff.hdr.vid.hdr.dwLength = riff.hdr.hdr.dwTotalFrames;
		var_write(&riff.hdr.vid.hdr.dwSuggestedBufferSize, (Bit32u)buffer_size[0]);
		var_write(&riff.hdr.aud.hdr.dwLength, (Bit32u)samples);
		var_write(&riff.hdr.aud.hdr.dwSuggestedBufferSize, (Bit32u)buffer_size[1]);

		// attempt to write index
		if (fwrite(&idx1,sizeof(idx1),1,handle)==1 && \
		    fwrite(&idx[0],sizeof(AVIIndexEntry),idx.size(),handle)==idx.size()) {
			// index was added, set HASINDEX flag
			var_write(&riff.hdr.hdr.dwFlags, AVIF_ISINTERLEAVED|AVIF_HASINDEX);
			// accumulate index size
			data_length += sizeof(AVIChunk)+sizeof(AVIIndexEntry)*idx.size();
		}

		var_write(&riff.dwSize, (Bit32u)(sizeof(riff)+data_length-sizeof(AVIChunk)-sizeof(AVITag)));

		fseek(handle, 0, SEEK_SET);
		fwrite(&riff, sizeof(riff), 1, handle);
		fclose(handle);
	}

	/* want to add s bytes of data, fails if new size exceeds 2GB */
	bool CanAdd(size_t s) {
		/* calculate maximum possible data size (all constants here)*/
		size_t max_size = AVI_MAX_SIZE;
		/* minus headers (excluding "movi" tag) */
		max_size -= (sizeof(riff)-sizeof(AVITag));
		/* minus index chunk header + data chunk header  */
		max_size -= 2*sizeof(AVIChunk);
		/* minus index entry */
		max_size -= sizeof(AVIIndexEntry);

		/* round up to multiple of 2 */
		s += s&1;
		/* add all data already written */
		s += data_length;
		/* add existing index entries */
		s += idx.size()*sizeof(AVIIndexEntry);

		return s < max_size;
	}

	/* add video data */
	bool AddVideo(const void *data, size_t length, Bit32u flags) {
		AVIChunk ck("00dc",length);
		AVIIndexEntry entry(ck, flags, data_length);

		if (AddChunk(ck, entry, data, length)) {
			if (length > buffer_size[0])
				buffer_size[0] = length;
			frames++;
			return true;
		}
		return false;
	}

	/* add audio data */
	bool AddAudio(const void *data, Bitu _samples, Bit32u new_freq) {
		size_t length = _samples*4;
		AVIChunk ck("01wb",length);
		/* Every audio block marked as keyframe, just in case */
		AVIIndexEntry entry(ck, AVII_KEYFRAME, data_length);

		if (!CanAdd(length)) return false;

		if (SetFreq(new_freq)) {
			/* rewrite audio headers */
			long pos = ftell(handle);
			fseek(handle, (long)(riff.hdr.aud.lst.ck.sTag-riff.sTag), SEEK_SET);
			fwrite(&riff.hdr.aud, sizeof(riff.hdr.aud), 1, handle);
			fseek(handle, pos, SEEK_SET);
		}

		if (AddChunk(ck, entry, data, length)) {
			if (length > buffer_size[1])
				buffer_size[1] = length;
			samples += _samples;
			return true;
		}
		return false;
	}
};
#endif // C_SRECORD

static struct {
	struct {
		FILE * handle;
		Bit16u buf[WAVE_BUF][2];
		Bitu used;
		Bit32u length;
		Bit32u freq;
	} wave; 
	struct {
		FILE * handle;
		Bit8u buffer[MIDI_BUF];
		Bitu used,done;
		Bit32u last;
	} midi;
	struct {
		Bitu rowlen;
	} image;
#if (C_SRECORD)
	struct {
		AVIFILE		*avi_out;
		Bit16s		audiobuf[WAVE_BUF][2];
		Bitu		audioused;
		Bit32u		audiorate;
		VideoCodec	*codec;
		Bitu		width, height, bpp;
		float		fps;
		int			bufSize;
		void		*buf;
		int		gop;
	} video;
#endif
} capture;

FILE * OpenCaptureFile(const char * type,const char * ext) {
	if(capturedir.empty()) {
		LOG_MSG("Please specify a capture directory");
		return 0;
	}

	Bitu last=0;
	char file_start[16];
	dir_information * dir;
	/* Find a filename to open */
	dir = open_directory(capturedir.c_str());
	if (!dir) {
		//Try creating it first
		Cross::CreateDir(capturedir);
		dir=open_directory(capturedir.c_str());
		if(!dir) {
		
			LOG_MSG("Can't open dir %s for capturing %s",capturedir.c_str(),type);
			return 0;
		}
	}
	strcpy(file_start,RunningProgram);
	lowcase(file_start);
	strcat(file_start,"_");
	bool is_directory;
	char tempname[CROSS_LEN];
	bool testRead = read_directory_first(dir, tempname, is_directory );
	for ( ; testRead; testRead = read_directory_next(dir, tempname, is_directory) ) {
		char * test=strstr(tempname,ext);
		if (!test || strlen(test)!=strlen(ext)) 
			continue;
		*test=0;
		if (strncasecmp(tempname,file_start,strlen(file_start))!=0) continue;
		Bitu num=atoi(&tempname[strlen(file_start)]);
		if (num>=last) last=num+1;
	}
	close_directory( dir );
	char file_name[CROSS_LEN];
	sprintf(file_name,"%s%c%s%03" sBitfs(u) "%s",capturedir.c_str(),CROSS_FILESPLIT,file_start,last,ext);
	/* Open the actual file */
	FILE * handle=fopen(file_name,"wb");
	if (handle) {
		LOG_MSG("Capturing %s to %s",type,file_name);
	} else {
		LOG_MSG("Failed to open %s for capturing %s",file_name,type);
	}
	return handle;
}

#if (C_SRECORD)
static void CAPTURE_VideoEvent(bool pressed) {
	if (!pressed)
		return;
	if (CaptureState & CAPTURE_VIDEO) {
		/* Flush remaining audio */
		if ( capture.video.audioused ) {
			if (capture.video.avi_out) {
				/* if it can't be added, leave it to be written to the next segment */
				if (capture.video.avi_out->AddAudio(capture.video.audiobuf, capture.video.audioused, capture.video.audiorate)) {
					capture.video.audioused = 0;
				}
			}
		}
		/* Close the video */
		CaptureState &= ~CAPTURE_VIDEO;
		LOG_MSG("Stopped capturing video.");

		delete capture.video.avi_out;
		capture.video.avi_out = NULL;

		free( capture.video.buf );
		capture.video.buf = NULL;
		delete capture.video.codec;
		capture.video.codec = NULL;
	} else {
		CaptureState |= CAPTURE_VIDEO;
		capture.video.audioused=0;
	}
}
#endif

void CAPTURE_AddImage(Bitu width, Bitu height, Bitu bpp, Bitu pitch, Bitu flags, float fps, const Bit8u * data, const Bit8u * pal) {
	Bitu i;
	Bit8u doubleRow[SCALER_MAXWIDTH*4];
	Bitu countWidth = width;

	if (flags & CAPTURE_FLAG_DBLH)
		height *= 2;
	if (flags & CAPTURE_FLAG_DBLW)
		width *= 2;

	if (height > SCALER_MAXHEIGHT)
		return;
	if (width > SCALER_MAXWIDTH)
		return;
#if (C_SSHOT)
	if (CaptureState & CAPTURE_IMAGE) {
		png_structp png_ptr = NULL;
		png_infop info_ptr = NULL;
		png_color palette[256];

		CaptureState &= ~CAPTURE_IMAGE;
		/* Open the actual file */
		FILE * fp=OpenCaptureFile("Screenshot",".png");
		if (fp) {
			/* First try to allocate the png structures */
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
			if (png_ptr) info_ptr = png_create_info_struct(png_ptr);
		}
		if (info_ptr) {
			/* Finalize the initing of png library */
			png_init_io(png_ptr, fp);
			png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);

			/* set other zlib parameters */
			png_set_compression_mem_level(png_ptr, 8);
			png_set_compression_strategy(png_ptr,Z_DEFAULT_STRATEGY);
			png_set_compression_window_bits(png_ptr, 15);
			png_set_compression_method(png_ptr, 8);
			png_set_compression_buffer_size(png_ptr, 8192);
		
			if (bpp==8) {
				png_set_IHDR(png_ptr, info_ptr, width, height,
					8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
					PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
				for (i=0;i<256;i++) {
					palette[i].red=pal[i*4+0];
					palette[i].green=pal[i*4+1];
					palette[i].blue=pal[i*4+2];
				}
				png_set_PLTE(png_ptr, info_ptr, palette,256);
			} else {
				png_set_bgr( png_ptr );
				png_set_IHDR(png_ptr, info_ptr, width, height,
					8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
					PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			}

#ifdef PNG_TEXT_SUPPORTED
			char ptext[] = "DOSBox" VERSION;
			char software[] = "Software";
			png_text text;
			text.compression = PNG_TEXT_COMPRESSION_NONE;
			text.key = software;
			text.text = ptext;
			png_set_text(png_ptr, info_ptr, &text, 1);
#endif
			png_write_info(png_ptr, info_ptr);

			for (i=0;i<height;i++) {
				const void *rowPointer;
				const void *srcLine;
				if (flags & CAPTURE_FLAG_DBLH)
					srcLine=(data+(i >> 1)*pitch);
				else
					srcLine=(data+(i >> 0)*pitch);
				rowPointer=srcLine;
				switch (bpp) {
				case 8:
					if (flags & CAPTURE_FLAG_DBLW) {
						for (Bitu x=0;x<countWidth;x++)
							doubleRow[x*2+0] =
							doubleRow[x*2+1] = ((Bit8u *)srcLine)[x];
						rowPointer = doubleRow;
					}
					break;
				case 15:
					if (flags & CAPTURE_FLAG_DBLW) {
						for (Bitu x=0;x<countWidth;x++) {
							Bitu pixel = ((Bit16u *)srcLine)[x];
#ifdef WORDS_BIGENDIAN
							doubleRow[x*6+0] = doubleRow[x*6+3] = ((pixel& 0x1f00) * 0x21) >>  10;
							doubleRow[x*6+1] = doubleRow[x*6+4] = (((pixel&0xe000)|((pixel&0x0003)<<16)) * 0x21) >> 15;
							doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0x007c) * 0x21) >>   4;
#else
							doubleRow[x*6+0] = doubleRow[x*6+3] = ((pixel& 0x001f) * 0x21) >>  2;
							doubleRow[x*6+1] = doubleRow[x*6+4] = ((pixel& 0x03e0) * 0x21) >>  7;
							doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0x7c00) * 0x21) >>  12;
#endif
						}
					} else {
						for (Bitu x=0;x<countWidth;x++) {
							Bitu pixel = ((Bit16u *)srcLine)[x];
#ifdef WORDS_BIGENDIAN
							doubleRow[x*3+0] = ((pixel& 0x1f00) * 0x21) >>  10;
							doubleRow[x*3+1] = (((pixel&0xe000)|((pixel&0x0003)<<16)) * 0x21) >> 15;
							doubleRow[x*3+2] = ((pixel& 0x007c) * 0x21) >>   4;
#else
							doubleRow[x*3+0] = ((pixel& 0x001f) * 0x21) >>  2;
							doubleRow[x*3+1] = ((pixel& 0x03e0) * 0x21) >>  7;
							doubleRow[x*3+2] = ((pixel& 0x7c00) * 0x21) >>  12;
#endif
						}
					}
					rowPointer = doubleRow;
					break;
				case 16:
					if (flags & CAPTURE_FLAG_DBLW) {
						for (Bitu x=0;x<countWidth;x++) {
							Bitu pixel = ((Bit16u *)srcLine)[x];
#ifdef WORDS_BIGENDIAN
							doubleRow[x*6+0] = doubleRow[x*6+3] = ((pixel& 0x1f00) * 0x21) >> 10;
							doubleRow[x*6+1] = doubleRow[x*6+4] = (((pixel&0xe000)|((pixel&0x0007)<<16)) * 0x41) >> 17;
							doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0x00f8) * 0x21) >> 5;
#else
							doubleRow[x*6+0] = doubleRow[x*6+3] = ((pixel& 0x001f) * 0x21) >> 2;
							doubleRow[x*6+1] = doubleRow[x*6+4] = ((pixel& 0x07e0) * 0x41) >> 9;
							doubleRow[x*6+2] = doubleRow[x*6+5] = ((pixel& 0xf800) * 0x21) >> 13;
#endif
						}
					} else {
						for (Bitu x=0;x<countWidth;x++) {
							Bitu pixel = ((Bit16u *)srcLine)[x];
#ifdef WORDS_BIGENDIAN
							doubleRow[x*3+0] = ((pixel& 0x1f00) * 0x21) >> 10;
							doubleRow[x*3+1] = (((pixel&0xe000)|((pixel&0x0007)<<16)) * 0x41) >> 17;
							doubleRow[x*3+2] = ((pixel& 0x00f8) * 0x21) >> 5;
#else
							doubleRow[x*3+0] = ((pixel& 0x001f) * 0x21) >>  2;
							doubleRow[x*3+1] = ((pixel& 0x07e0) * 0x41) >>  9;
							doubleRow[x*3+2] = ((pixel& 0xf800) * 0x21) >>  13;
#endif
						}
					}
					rowPointer = doubleRow;
					break;
				case 32:
					if (flags & CAPTURE_FLAG_DBLW) {
						for (Bitu x=0;x<countWidth;x++) {
							doubleRow[x*6+0] = doubleRow[x*6+3] = ((Bit8u *)srcLine)[x*4+0];
							doubleRow[x*6+1] = doubleRow[x*6+4] = ((Bit8u *)srcLine)[x*4+1];
							doubleRow[x*6+2] = doubleRow[x*6+5] = ((Bit8u *)srcLine)[x*4+2];
						}
					} else {
						for (Bitu x=0;x<countWidth;x++) {
							doubleRow[x*3+0] = ((Bit8u *)srcLine)[x*4+0];
							doubleRow[x*3+1] = ((Bit8u *)srcLine)[x*4+1];
							doubleRow[x*3+2] = ((Bit8u *)srcLine)[x*4+2];
						}
					}
					rowPointer = doubleRow;
					break;
				}
				png_write_row(png_ptr, (png_bytep)rowPointer);
				if (flags & CAPTURE_FLAG_DBLH) {
					png_write_row(png_ptr, (png_bytep)rowPointer);
					i++;
				}
			}
			/* Finish writing */
			png_write_end(png_ptr, 0);
		}
		/*Destroy PNG structs*/
		if (png_ptr) png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr:NULL);
		/*close file*/
		if (fp)	fclose(fp);
	}
#endif
#if (C_SRECORD)
	if (CaptureState & CAPTURE_VIDEO) {
		zmbv_format_t format;
		/* Disable capturing if any of the test fails */
		if (capture.video.avi_out) {
			if (capture.video.width!=width || capture.video.height!=height || \
				capture.video.bpp!=bpp || capture.video.fps != fps || \
				!capture.video.avi_out->CanAdd(4*capture.video.audioused+capture.video.bufSize)) {
				// need to switch files
				CAPTURE_VideoEvent(true);
				LOG_MSG("CAPTURE: Beginning new video segment");
				CaptureState |= CAPTURE_VIDEO;
			}
		}
		switch (bpp) {
		case 8:format = ZMBV_FORMAT_8BPP;break;
		case 15:format = ZMBV_FORMAT_15BPP;break;
		case 16:format = ZMBV_FORMAT_16BPP;break;
		case 32:format = ZMBV_FORMAT_32BPP;break;
		default:
			goto skip_video;
		}
		if (!capture.video.avi_out) {
			FILE *f = OpenCaptureFile("Video",".avi");
			if (f==NULL)
				goto skip_video;
			capture.video.avi_out = new AVIFILE(f, width, height, fps);
			capture.video.codec = new VideoCodec();
			if (!capture.video.codec)
				goto skip_video;
			if (!capture.video.codec->SetupCompress( (int)width, (int)height))
				goto skip_video;
			capture.video.bufSize = capture.video.codec->NeededSize((int)width, (int)height, format);
			capture.video.buf = malloc( capture.video.bufSize );
			if (!capture.video.buf)
				goto skip_video;

			capture.video.width = width;
			capture.video.height = height;
			capture.video.bpp = bpp;
			capture.video.fps = fps;
			capture.video.gop = 0;
			flags &= ~CAPTURE_FLAG_DUPLICATE;
		}
		int codecFlags,written;
		codecFlags = 0;
		if (flags & CAPTURE_FLAG_DUPLICATE) written = 0;
		else {
			if (capture.video.gop >= 300)
				capture.video.gop = 0;
			if (capture.video.gop==0)
				codecFlags = 1;
			if (!capture.video.codec->PrepareCompressFrame( codecFlags, format, (char *)pal, capture.video.buf, capture.video.bufSize))
				goto skip_video;

			for (i=0;i<height;i++) {
				const void *srcLine;
				if (flags & CAPTURE_FLAG_DBLH)
					srcLine=(data+(i >> 1)*pitch);
				else
					srcLine=(data+(i >> 0)*pitch);
				if (flags & CAPTURE_FLAG_DBLW) {
					Bitu x;
					Bitu countWidth = width >> 1;
					switch ( bpp) {
					case 8:
						for (x=0;x<countWidth;x++)
							((Bit8u *)doubleRow)[x*2+0] =
							((Bit8u *)doubleRow)[x*2+1] = ((Bit8u *)srcLine)[x];
						break;
					case 15:
					case 16:
						for (x=0;x<countWidth;x++)
							((Bit16u *)doubleRow)[x*2+0] =
							((Bit16u *)doubleRow)[x*2+1] = ((Bit16u *)srcLine)[x];
						break;
					case 32:
						for (x=0;x<countWidth;x++)
							((Bit32u *)doubleRow)[x*2+0] =
							((Bit32u *)doubleRow)[x*2+1] = ((Bit32u *)srcLine)[x];
						break;
					}
					srcLine=doubleRow;
				}
				if (flags & CAPTURE_FLAG_DBLH) {
					const void *rowPointer[2];
					rowPointer[0]=rowPointer[1]=srcLine;
					capture.video.codec->CompressLines(2, rowPointer);
					i++;
				} else
					capture.video.codec->CompressLines(1, &srcLine);
			}
			written = capture.video.codec->FinishCompressFrame();
			if (written < 0)
				goto skip_video;
		}
		if (capture.video.avi_out->AddVideo(capture.video.buf, written, codecFlags & 1 ? AVII_KEYFRAME : 0)) {
//			LOG_MSG("Frame %d video %d audio %d",capture.video.avi_out->frames, written, capture.video.audioused *4 );
			capture.video.gop++;
			if (!capture.video.audioused) return;
			Bitu samples = capture.video.audioused;
			capture.video.audioused = 0;
			if (capture.video.avi_out->AddAudio(capture.video.audiobuf, samples, capture.video.audiorate))
				return;
			LOG_MSG("Failed to write audio data");
		} else LOG_MSG("Failed to write video data");
	} else return;
skip_video:
	/* something went wrong, shut it down */
	CAPTURE_VideoEvent(true);
#endif
	return;
}


#if (C_SSHOT)
static void CAPTURE_ScreenShotEvent(bool pressed) {
	if (!pressed)
		return;
	CaptureState |= CAPTURE_IMAGE;
}
#endif


/* WAV capturing */
static Bit8u wavheader[]={
	'R','I','F','F',	0x0,0x0,0x0,0x0,		/* Bit32u Riff Chunk ID /  Bit32u riff size */
	'W','A','V','E',	'f','m','t',' ',		/* Bit32u Riff Format  / Bit32u fmt chunk id */
	0x10,0x0,0x0,0x0,	0x1,0x0,0x2,0x0,		/* Bit32u fmt size / Bit16u encoding/ Bit16u channels */
	0x0,0x0,0x0,0x0,	0x0,0x0,0x0,0x0,		/* Bit32u freq / Bit32u byterate */
	0x4,0x0,0x10,0x0,	'd','a','t','a',		/* Bit16u byte-block / Bit16u bits / Bit16u data chunk id */
	0x0,0x0,0x0,0x0,							/* Bit32u data size */
};

void CAPTURE_AddWave(Bit32u freq, Bitu len, Bit16s * data) {
#if (C_SRECORD)
	if (CaptureState & CAPTURE_VIDEO) {
		Bitu left = WAVE_BUF - capture.video.audioused;
		if (len > WAVE_BUF)
			LOG_MSG("CAPTURE: WAVE_BUF too small");
		/* if framerate is very low (and audiorate is high) the audiobuffer may overflow */
		if (left < len && capture.video.avi_out) {
			if (capture.video.avi_out->AddAudio(capture.video.audiobuf, capture.video.audioused, capture.video.audiorate)) {
				capture.video.audioused = 0;
				left = WAVE_BUF;
			}
		}
		if (left > len)
			left = len;
		// samples are native format, need to be little endian
		Bit16u *buf = (Bit16u*)capture.video.audiobuf[capture.video.audioused];
		Bit16u *read = (Bit16u*)data;
		capture.video.audioused += left;
#ifdef WORDS_BIGENDIAN
		do {
			var_write(buf++, *read++);
			var_write(buf++, *read++);
		} while (--left);
#else
		memcpy(buf, read, left*4);
#endif
		capture.video.audiorate = freq;
	}
#endif
	if (CaptureState & CAPTURE_WAVE) {
		if (!capture.wave.handle) {
			capture.wave.handle=OpenCaptureFile("Wave Output",".wav");
			if (!capture.wave.handle) {
				CaptureState &= ~CAPTURE_WAVE;
				return;
			}
			capture.wave.length = 0;
			capture.wave.used = 0;
			capture.wave.freq = freq;
			fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		}
		Bit16u * read = (Bit16u*)data;
		while (len > 0 ) {
			Bitu left = WAVE_BUF - capture.wave.used;
			if (!left) {
				fwrite(capture.wave.buf,1,4*WAVE_BUF,capture.wave.handle);
				capture.wave.length += 4*WAVE_BUF;
				capture.wave.used = 0;
				left = WAVE_BUF;
			}
			if (left > len)
				left = len;
			Bit16u *buf = capture.wave.buf[capture.wave.used];
			capture.wave.used += left;
			len -= left;
#ifdef WORDS_BIGENDIAN
			do {
				var_write(buf++, *read++);
				var_write(buf++, *read++);
			} while (--left);
#else
			memcpy(buf, read, left*4);
#endif
		}
	}
}
static void CAPTURE_WaveEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.wave.handle) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		fwrite(capture.wave.buf,1,capture.wave.used*4,capture.wave.handle);
		capture.wave.length+=capture.wave.used*4;
		/* Fill in the header with useful information */
		host_writed(&wavheader[0x04],capture.wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],capture.wave.freq);
		host_writed(&wavheader[0x1C],capture.wave.freq*4);
		host_writed(&wavheader[0x28],capture.wave.length);
		
		fseek(capture.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		fclose(capture.wave.handle);
		capture.wave.handle=0;
		CaptureState |= CAPTURE_WAVE;
	} 
	CaptureState ^= CAPTURE_WAVE;
}

/* MIDI capturing */

static Bit8u midi_header[]={
	'M','T','h','d',			/* Bit32u, Header Chunk */
	0x0,0x0,0x0,0x6,			/* Bit32u, Chunk Length */
	0x0,0x0,					/* Bit16u, Format, 0=single track */
	0x0,0x1,					/* Bit16u, Track Count, 1 track */
	0x01,0xf4,					/* Bit16u, Timing, 2 beats/second with 500 frames */
	'M','T','r','k',			/* Bit32u, Track Chunk */
	0x0,0x0,0x0,0x0,			/* Bit32u, Chunk Length */
	//Track data
};


static void RawMidiAdd(Bit8u data) {
	capture.midi.buffer[capture.midi.used++]=data;
	if (capture.midi.used >= MIDI_BUF ) {
		capture.midi.done += capture.midi.used;
		fwrite(capture.midi.buffer,1,MIDI_BUF,capture.midi.handle);
		capture.midi.used = 0;
	}
}

static void RawMidiAddNumber(Bit32u val) {
	if (val & 0xfe00000) RawMidiAdd((Bit8u)(0x80|((val >> 21) & 0x7f)));
	if (val & 0xfffc000) RawMidiAdd((Bit8u)(0x80|((val >> 14) & 0x7f)));
	if (val & 0xfffff80) RawMidiAdd((Bit8u)(0x80|((val >> 7) & 0x7f)));
	RawMidiAdd((Bit8u)(val & 0x7f));
}

void CAPTURE_AddMidi(bool sysex, Bitu len, Bit8u * data) {
	if (!capture.midi.handle) {
		capture.midi.handle=OpenCaptureFile("Raw Midi",".mid");
		if (!capture.midi.handle) {
			return;
		}
		fwrite(midi_header,1,sizeof(midi_header),capture.midi.handle);
		capture.midi.last=PIC_Ticks;
	}
	Bit32u delta=PIC_Ticks-capture.midi.last;
	capture.midi.last=PIC_Ticks;
	RawMidiAddNumber(delta);
	if (sysex) {
		RawMidiAdd( 0xf0 );
		RawMidiAddNumber( len );
	}
	for (Bitu i=0;i<len;i++) 
		RawMidiAdd(data[i]);
}

static void CAPTURE_MidiEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.midi.handle) {
		LOG_MSG("Stopping raw midi saving and finalizing file.");
		//Delta time
		RawMidiAdd(0x00);
		//End of track event
		RawMidiAdd(0xff);
		RawMidiAdd(0x2F);
		RawMidiAdd(0x00);
		/* clear out the final data in the buffer if any */
		fwrite(capture.midi.buffer,1,capture.midi.used,capture.midi.handle);
		capture.midi.done+=capture.midi.used;
		fseek(capture.midi.handle,18, SEEK_SET);
		Bit8u size[4];
		size[0]=(Bit8u)(capture.midi.done >> 24);
		size[1]=(Bit8u)(capture.midi.done >> 16);
		size[2]=(Bit8u)(capture.midi.done >> 8);
		size[3]=(Bit8u)(capture.midi.done >> 0);
		fwrite(&size,1,4,capture.midi.handle);
		fclose(capture.midi.handle);
		capture.midi.handle=0;
		CaptureState &= ~CAPTURE_MIDI;
		return;
	} 
	CaptureState ^= CAPTURE_MIDI;
	if (CaptureState & CAPTURE_MIDI) {
		LOG_MSG("Preparing for raw midi capture, will start with first data.");
		capture.midi.used=0;
		capture.midi.done=0;
		capture.midi.handle=0;
	} else {
		LOG_MSG("Stopped capturing raw midi before any data arrived.");
	}
}

class HARDWARE:public Module_base{
public:
	HARDWARE(Section* configuration):Module_base(configuration){
		Section_prop * section = static_cast<Section_prop *>(configuration);
		Prop_path* proppath= section->Get_path("captures");
		capturedir = proppath->realpath;
		CaptureState = 0;
		MAPPER_AddHandler(CAPTURE_WaveEvent,MK_f6,MMOD1,"recwave","Rec Wave");
		MAPPER_AddHandler(CAPTURE_MidiEvent,MK_f8,MMOD1|MMOD2,"caprawmidi","Cap MIDI");
#if (C_SSHOT)
		MAPPER_AddHandler(CAPTURE_ScreenShotEvent,MK_f5,MMOD1,"scrshot","Screenshot");
#endif
#if (C_SRECORD)
		MAPPER_AddHandler(CAPTURE_VideoEvent,MK_f5,MMOD1|MMOD2,"video","Video");
#endif
	}
	~HARDWARE(){
#if (C_SRECORD)
		if (capture.video.avi_out) CAPTURE_VideoEvent(true);
#endif
		if (capture.wave.handle) CAPTURE_WaveEvent(true);
		if (capture.midi.handle) CAPTURE_MidiEvent(true);
	}
};

static HARDWARE* test;

void HARDWARE_Destroy(Section * sec) {
	delete test;
}

void HARDWARE_Init(Section * sec) {
	test = new HARDWARE(sec);
	sec->AddDestroyFunction(&HARDWARE_Destroy,true);
}
