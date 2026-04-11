#include "vulkan_render/bake/bake_types.h"

#include <global_state/global_state.h>
#include <serialization/serialization.h>
#include <vfs/vfs.h>
#include <utils/kryga_log.h>

namespace kryga
{
namespace render
{
namespace bake
{

void
bake_config::apply_preset(bake_preset preset)
{
    switch (preset)
    {
    case bake_preset::low:
        resolution = 512;
        samples_per_texel = 16;
        bounce_count = 1;
        denoise_iterations = 1;
        texels_per_unit = 2.0f;
        min_tile = 8;
        max_tile = 128;
        bake_ao = false;
        shadow_bias = 0.05f;
        shadow_samples = 1;
        shadow_spread = 0.0f;
        dilate_iterations = 2;
        break;

    case bake_preset::medium:
        resolution = 1024;
        samples_per_texel = 64;
        bounce_count = 2;
        denoise_iterations = 2;
        texels_per_unit = 4.0f;
        min_tile = 16;
        max_tile = 256;
        bake_ao = true;
        ao_radius = 2.0f;
        ao_intensity = 1.0f;
        shadow_bias = 0.05f;
        shadow_samples = 16;
        shadow_spread = 0.015f;
        dilate_iterations = 3;
        break;

    case bake_preset::high:
        resolution = 2048;
        samples_per_texel = 256;
        bounce_count = 3;
        denoise_iterations = 3;
        texels_per_unit = 8.0f;
        min_tile = 32;
        max_tile = 512;
        bake_ao = true;
        ao_radius = 3.0f;
        ao_intensity = 1.0f;
        shadow_bias = 0.03f;
        shadow_samples = 32;
        shadow_spread = 0.02f;
        dilate_iterations = 4;
        break;

    case bake_preset::maximum:
        resolution = 4096;
        samples_per_texel = 1024;
        bounce_count = 4;
        denoise_iterations = 4;
        texels_per_unit = 16.0f;
        min_tile = 64;
        max_tile = 1024;
        bake_ao = true;
        ao_radius = 4.0f;
        ao_intensity = 1.2f;
        shadow_bias = 0.02f;
        shadow_samples = 64;
        shadow_spread = 0.025f;
        dilate_iterations = 4;
        break;
    }

    bake_direct = true;
    bake_indirect = true;
}

// ============================================================================
// Load / Save
// ============================================================================

namespace
{
template <typename T>
void
extract_field(const YAML::Node& node, const char* key, T& field)
{
    auto v = node[key];
    if (!v || !v.IsScalar())
    {
        return;
    }
    field = v.as<T>();
}
}  // namespace

bool
bake_config::load(const utils::path& path)
{
    serialization::container container;
    if (!serialization::read_container(path, container))
    {
        ALOG_WARN("Failed to load bake config from '{}', using defaults", path.str());
        return false;
    }

    extract_field(container, "resolution", resolution);
    extract_field(container, "samples_per_texel", samples_per_texel);
    extract_field(container, "bounce_count", bounce_count);
    extract_field(container, "denoise_iterations", denoise_iterations);
    extract_field(container, "ao_radius", ao_radius);
    extract_field(container, "ao_intensity", ao_intensity);
    extract_field(container, "bake_direct", bake_direct);
    extract_field(container, "bake_indirect", bake_indirect);
    extract_field(container, "bake_ao", bake_ao);
    extract_field(container, "save_png", save_png);
    extract_field(container, "texels_per_unit", texels_per_unit);
    extract_field(container, "min_tile", min_tile);
    extract_field(container, "max_tile", max_tile);
    extract_field(container, "shadow_bias", shadow_bias);
    extract_field(container, "shadow_samples", shadow_samples);
    extract_field(container, "shadow_spread", shadow_spread);
    extract_field(container, "dilate_iterations", dilate_iterations);

    ALOG_INFO("Loaded bake config from '{}'", path.str());
    return true;
}

bool
bake_config::save(const utils::path& path) const
{
    YAML::Node root;
    root["resolution"] = resolution;
    root["samples_per_texel"] = samples_per_texel;
    root["bounce_count"] = bounce_count;
    root["denoise_iterations"] = denoise_iterations;
    root["ao_radius"] = ao_radius;
    root["ao_intensity"] = ao_intensity;
    root["bake_direct"] = bake_direct;
    root["bake_indirect"] = bake_indirect;
    root["bake_ao"] = bake_ao;
    root["save_png"] = save_png;
    root["texels_per_unit"] = texels_per_unit;
    root["min_tile"] = min_tile;
    root["max_tile"] = max_tile;
    root["shadow_bias"] = shadow_bias;
    root["shadow_samples"] = shadow_samples;
    root["shadow_spread"] = shadow_spread;
    root["dilate_iterations"] = dilate_iterations;

    if (!serialization::write_container(path, root))
    {
        ALOG_ERROR("Failed to save bake config to '{}'", path.str());
        return false;
    }

    ALOG_INFO("Saved bake config to '{}'", path.str());
    return true;
}

bool
bake_config::load_with_cache(const vfs::rid& base, const vfs::rid& cache)
{
    auto& vfs = glob::glob_state().getr_vfs();
    if (vfs.exists(cache))
    {
        auto rp = vfs.real_path(cache);
        if (rp.has_value() && load(APATH(rp.value())))
        {
            return true;
        }
    }

    auto rp = vfs.real_path(base);
    if (rp.has_value())
    {
        return load(APATH(rp.value()));
    }
    return false;
}

bool
bake_config::save_to_cache(const vfs::rid& cache) const
{
    glob::glob_state().getr_vfs().create_directories(
        vfs::rid(cache.mount_point(), ""));

    YAML::Node root;
    root["resolution"] = resolution;
    root["samples_per_texel"] = samples_per_texel;
    root["bounce_count"] = bounce_count;
    root["denoise_iterations"] = denoise_iterations;
    root["ao_radius"] = ao_radius;
    root["ao_intensity"] = ao_intensity;
    root["bake_direct"] = bake_direct;
    root["bake_indirect"] = bake_indirect;
    root["bake_ao"] = bake_ao;
    root["save_png"] = save_png;
    root["texels_per_unit"] = texels_per_unit;
    root["min_tile"] = min_tile;
    root["max_tile"] = max_tile;
    root["shadow_bias"] = shadow_bias;
    root["shadow_samples"] = shadow_samples;
    root["shadow_spread"] = shadow_spread;
    root["dilate_iterations"] = dilate_iterations;

    if (!serialization::write_container(cache, root))
    {
        ALOG_ERROR("Failed to save bake config to '{}'", cache.str());
        return false;
    }

    ALOG_INFO("Saved bake config to '{}'", cache.str());
    return true;
}

}  // namespace bake
}  // namespace render
}  // namespace kryga
