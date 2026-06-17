// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos.h"
#include "utils/checks.h"

#include <SDL3/SDL_iostream.h>

CHECK_NARROWING();

// Adapted from PhysFS by icculus
//
// Source:
// https://github.com/icculus/physfs/blob/ac00f6b264cb76e4ec9b34dfbd6afae3fbbd53b3/extras/physfsrwops.c
//
struct DosFileIOData {
	uint16_t handle = 0;
};

static uint16_t get_dos_file_handle(void* userdata)
{
	assert(userdata);
	return static_cast<DosFileIOData*>(userdata)->handle;
}

static Sint64 SDLCALL dos_rwops_size(void* userdata)
{
	assert(userdata);

	uint32_t pos = 0;
	if (!DOS_SeekFile(get_dos_file_handle(userdata), &pos, DOS_SEEK_END)) {
		return -1;
	}
	return static_cast<Sint64>(pos);
}

static Sint64 SDLCALL dos_rwops_seek(void* userdata, Sint64 offset, SDL_IOWhence whence)
{
	assert(userdata);

	uint32_t seek_mode = 0;

	switch (whence) {
	case SDL_IO_SEEK_SET: seek_mode = DOS_SEEK_SET; break;
	case SDL_IO_SEEK_CUR: seek_mode = DOS_SEEK_CUR; break;
	case SDL_IO_SEEK_END: seek_mode = DOS_SEEK_END; break;
	default: LOG_ERR("DOS:RWOPS: Invalid 'whence' parameter"); return -1;
	}

	auto pos = check_cast<uint32_t>(offset);
	if (!DOS_SeekFile(get_dos_file_handle(userdata), &pos, seek_mode)) {
		return -1;
	}
	return static_cast<Sint64>(pos);
}

static size_t SDLCALL dos_rwops_read(void* userdata, void* ptr, size_t size, SDL_IOStatus* status)
{
	assert(userdata);
	assert(ptr);

	const auto handle = get_dos_file_handle(userdata);
	auto dest        = reinterpret_cast<uint8_t*>(ptr);

	size_t total_read = 0;
	while (total_read < size) {
		const size_t bytes_left = size - total_read;
		const size_t chunk_size = (bytes_left > UINT16_MAX) ? UINT16_MAX : bytes_left;

		const auto requested = check_cast<uint16_t>(chunk_size);
		uint16_t chunk_read  = requested;

		if (!DOS_ReadFile(handle, dest + total_read, &chunk_read)) {
			if (status) *status = SDL_IO_STATUS_ERROR;
			break;
		}

		total_read += chunk_read;
		if (chunk_read != requested) {
			if (status) *status = SDL_IO_STATUS_EOF;
			break;
		}
	}
	return total_read;
}

static size_t SDLCALL dos_rwops_write([[maybe_unused]] void* userdata,
                                      [[maybe_unused]] const void* ptr,
                                      [[maybe_unused]] size_t size,
                                      SDL_IOStatus* status)
{
	LOG_WARNING("DOS:RWOPS: Writing is not implemented");
	if (status) *status = SDL_IO_STATUS_ERROR;
	return 0;
}

static bool SDLCALL dos_rwops_close(void* userdata)
{
	assert(userdata);

	auto* data = static_cast<DosFileIOData*>(userdata);
	const bool result = DOS_CloseFile(data->handle);
	delete data;
	return result;
}

SDL_IOStream* create_sdl_rwops_for_dos_file(const uint16_t dos_file_handle)
{
	auto* data = new DosFileIOData{dos_file_handle};

	SDL_IOStreamInterface iface;
	SDL_INIT_INTERFACE(&iface);
	iface.size  = dos_rwops_size;
	iface.seek  = dos_rwops_seek;
	iface.read  = dos_rwops_read;
	iface.write = dos_rwops_write;
	iface.close = dos_rwops_close;

	SDL_IOStream* rw = SDL_OpenIO(&iface, data);
	if (!rw) {
		delete data;
		return nullptr;
	}
	return rw;
}
