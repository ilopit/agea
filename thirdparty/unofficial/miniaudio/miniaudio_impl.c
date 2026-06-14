// Single translation unit that compiles miniaudio's implementation.
//
// Format support: WAV + MP3 + FLAC are built in by default (dr_wav / dr_mp3 /
// dr_flac). OGG Vorbis is enabled via stb_vorbis using miniaudio's documented
// integration order:
//   1. stb_vorbis HEADER ONLY first — declares the `stb_vorbis` type and defines
//      STB_VORBIS_INCLUDE_STB_VORBIS_H so miniaudio turns on its MA_HAS_VORBIS path.
//   2. miniaudio implementation (this pulls in <windows.h> on Win32).
//   3. stb_vorbis IMPLEMENTATION last — its short internal macros (L/C/R, etc.)
//      must NOT be in scope when <windows.h> is parsed, or winnt.h fails to compile.
// We use miniaudio's version-matched copy from extras/ rather than the standalone
// stb one to avoid drift.
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
