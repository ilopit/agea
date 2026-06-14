#pragma once

#include <utils/path.h>
#include <utils/id.h>

namespace kryga
{
namespace assets_importer
{

// Import an encoded audio file (WAV/MP3/FLAC/OGG) into a cooked audio_clip
// asset. Writes <dst>/class/audio_clips/<id>.aobj (descriptor) + <id>.aaud (the
// raw encoded bytes). Metadata (sample rate / channels / duration) is probed
// with miniaudio; import fails if miniaudio cannot decode the file.
bool
convert_audio_to_aaud(const utils::path& audio_path,
                      const utils::path& dst_folder_path,
                      const utils::id& audio_id);

}  // namespace assets_importer
}  // namespace kryga
