# CD-DA audio

Many DOS games from the mid-1990s onwards used CD-DA (Red Book audio) tracks
for music. These are standard audio tracks on a CD-ROM that bypass the sound
card entirely, delivering studio-quality pre-recorded music directly from the
game disc. DOSBox Staging supports CD-DA playback from disc images with audio
tracks in various formats.

CD-DA playback requires no special configuration. Mount a disc image with
audio tracks using the [`MOUNT` command](../../storage.md#mounting-cd-rom-images)
and the game will handle playback automatically.

<!-- TODO physical CD-ROM -->

### Mixer channel

CD-DA audio is output to the **CDAUDIO** mixer channel for mounted CD-ROM
images.


### Supported disc image formats

CD-DA audio tracks are supported in the following disc image formats:

- **CUE/BIN** --- CUE sheet with binary data files. This is the most common
    format for disc images with audio tracks. Audio tracks can be stored as raw
    PCM in the BIN file or as separate compressed audio files (see below).

- **MDS/MDF** --- Alcohol 120% disc image format.

- **ISO** --- Standard ISO 9660 images. Note: ISO images only contain data
    tracks; they cannot contain audio tracks. Use CUE/BIN for discs with audio.


### Supported audio track formats

When using CUE sheets, audio tracks can reference compressed audio files
instead of raw PCM data in BIN files. The following audio formats are
supported:

- **FLAC** --- Free Lossless Audio Codec. Recommended for lossless compression.
- **Opus** --- Modern lossy codec with excellent quality at low bitrates.
- **Ogg Vorbis** --- Widely used lossy codec.
- **MP3** --- MPEG-1 Layer 3.
- **WAV** --- Uncompressed PCM audio.

The standard Red Book audio format is 44.1 kHz, 16-bit stereo PCM. Mono audio
tracks are automatically converted to stereo during playback.


