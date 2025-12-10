// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos.h"
#include "utils/checks.h"

#include <SDL_rwops.h>

CHECK_NARROWING();

// Adapted from PhysFS by icculus
//
// Source:
// https://github.com/icculus/physfs/blob/ac00f6b264cb76e4ec9b34dfbd6afae3fbbd53b3/extras/physfsrwops.c
//
static uint16_t get_dos_file_handle(struct SDL_RWops* rw)
{
	assert(rw);

	return check_cast<uint16_t>(
	        reinterpret_cast<uint64_t>(rw->hidden.unknown.data1));
}

static Sint64 SDLCALL dos_rwops_size(struct SDL_RWops* rw)
{
	assert(rw);

	uint32_t pos = 0;
	if (!DOS_SeekFile(get_dos_file_handle(rw), &pos, DOS_SEEK_END)) {
		return -1;
	}
	return static_cast<Sint64>(pos);
}

static Sint64 SDLCALL dos_rwops_seek(struct SDL_RWops* rw, Sint64 offset, int whence)
{
	assert(rw);

	uint32_t seek_mode = 0;

	switch (whence) {
	case RW_SEEK_SET: seek_mode = DOS_SEEK_SET; break;
	case RW_SEEK_CUR: seek_mode = DOS_SEEK_CUR; break;
	case RW_SEEK_END: seek_mode = DOS_SEEK_END; break;
	default: LOG_ERR("DOS:RWOPS: Invalid 'whence' parameter"); return -1;
	}

	auto pos = check_cast<uint32_t>(offset);
	if (!DOS_SeekFile(get_dos_file_handle(rw), &pos, seek_mode)) {
		return -1;
	}
	return static_cast<Sint64>(pos);
}

static size_t SDLCALL dos_rwops_read(struct SDL_RWops* rw, void* ptr,
                                     size_t size, size_t maxnum)
{
	assert(rw);
	assert(ptr);

	auto handle = get_dos_file_handle(rw);
	auto dest   = reinterpret_cast<uint8_t*>(ptr);

	size_t num_read = 0;

	while (num_read < maxnum) {
		size_t num_bytes_left = size;

		while (num_bytes_left > 0) {
			uint16_t num_bytes = (num_bytes_left > UINT16_MAX)
			                           ? UINT16_MAX
			                           : check_cast<uint16_t>(
			                                     num_bytes_left);

			uint16_t num_bytes_read = num_bytes;

			if (!DOS_ReadFile(handle, dest, &num_bytes_read)) {
				LOG_ERR("return -1");
				return -1;
			}
			if (num_bytes_read != num_bytes) {
				LOG_ERR("return num_read: %d", num_read);
				return num_read;
			}

			dest += num_bytes_read;
			num_bytes_left -= num_bytes_read;
		}

		++num_read;
	}

	return maxnum;
}

static size_t dos_rwops_write([[maybe_unused]] SDL_RWops* rw,
                              [[maybe_unused]] const void* ptr,
                              [[maybe_unused]] size_t size,
                              [[maybe_unused]] size_t num)
{
	LOG_WARNING("DOS:RWOPS: Writing is not implemented");
	return -1;
}

static int dos_rwops_close(SDL_RWops* rw)
{
	assert(rw);

	if (!DOS_CloseFile(get_dos_file_handle(rw))) {
		return -1;
	}

	SDL_FreeRW(rw);
	return 0;
}

SDL_RWops* create_sdl_rwops_for_dos_file(const uint16_t dos_file_handle)
{
	auto rw = SDL_AllocRW();
	if (!rw) {
		return nullptr;
	}

	rw->size  = dos_rwops_size;
	rw->seek  = dos_rwops_seek;
	rw->read  = dos_rwops_read;
	rw->write = dos_rwops_write;
	rw->close = dos_rwops_close;

	rw->hidden.unknown.data1 = reinterpret_cast<void*>(dos_file_handle);

	return rw;
}
