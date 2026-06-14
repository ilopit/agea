#include "assets_importer/audio_importer.h"

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <packages/root/model/assets/audio_clip.h>

#include <core/construction_utils.h>
#include <core/reflection/reflection_type.h>
#include <core/package.h>

#include <serialization/serialization.h>

#include <miniaudio.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace kryga
{
namespace assets_importer
{
namespace
{

root::audio_format
format_from_extension(const utils::path& p)
{
    auto ext = p.fs().extension().string();
    std::transform(
        ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    if (ext == ".wav")
    {
        return root::audio_format::wav;
    }
    if (ext == ".mp3")
    {
        return root::audio_format::mp3;
    }
    if (ext == ".flac")
    {
        return root::audio_format::flac;
    }
    if (ext == ".ogg")
    {
        return root::audio_format::ogg_vorbis;
    }

    return root::audio_format::unknown;
}

}  // namespace

bool
convert_audio_to_aaud(const utils::path& audio_path,
                      const utils::path& dst_folder_path,
                      const utils::id& audio_id)
{
    utils::buffer data;
    if (!utils::buffer::load(audio_path, data))
    {
        ALOG_ERROR("audio_importer: failed to read source file {}", audio_path.str());
        return false;
    }

    // Probe metadata and validate the payload is decodable. miniaudio sniffs the
    // container from the bytes, so an undecodable file (e.g. OGG before Vorbis is
    // wired in) fails here rather than silently at play time.
    ma_decoder decoder;
    ma_result r = ma_decoder_init_memory(data.data(), (size_t)data.size(), nullptr, &decoder);
    if (r != MA_SUCCESS)
    {
        ALOG_ERROR(
            "audio_importer: miniaudio could not decode {} (unsupported or corrupt file; "
            "supported: WAV/MP3/FLAC/OGG) — ma_result={}",
            audio_path.str(),
            (int)r);
        return false;
    }

    uint32_t sample_rate = decoder.outputSampleRate;
    uint32_t channels = decoder.outputChannels;
    float duration = 0.0f;

    ma_uint64 frames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS && sample_rate != 0)
    {
        duration = (float)frames / (float)sample_rate;
    }
    ma_decoder_uninit(&decoder);

    auto fmt = format_from_extension(audio_path);

    auto obj_ext = audio_id.str() + ".aobj";
    auto data_ext = audio_id.str() + ".aaud";
    data.set_file(dst_folder_path / APATH("class/audio_clips") / data_ext);

    auto full_obj_path = dst_folder_path / APATH("class/audio_clips") / obj_ext;

    // Bind the asset id to the requested id so it matches the file stem the
    // package manifest indexes by.
    auto obj = core::alloc_empty_object<root::audio_clip>(audio_id);
    std::filesystem::create_directories(full_obj_path.parent().fs());

    core::package p(AID("dummy"));
    p.set_save_root_path(dst_folder_path);

    auto clip = obj->as<root::audio_clip>();
    clip->set_package(&p);
    clip->set_data_buffer(data);
    clip->set_sample_rate(sample_rate);
    clip->set_channels(channels);
    clip->set_duration_seconds(duration);
    clip->set_format(fmt);

    serialization::container sc;
    sc["proto_id"] = clip->get_type_id().str();
    sc["id"] = clip->get_id().str();
    auto& properties = clip->get_reflection()->m_serialization_properties;
    reflection::property_context__save ctx{nullptr, clip, &sc};
    for (auto& prop : properties)
    {
        ctx.p = prop.get();
        prop->save_handler(ctx);
    }
    if (!serialization::write_container(full_obj_path, sc))
    {
        ALOG_ERROR("audio_importer: failed to write {}", full_obj_path.str());
        return false;
    }

    ALOG_INFO("audio_importer: imported {} ({} Hz, {} ch, {:.2f}s)",
              audio_id.str(),
              sample_rate,
              channels,
              duration);
    return true;
}

}  // namespace assets_importer
}  // namespace kryga
