#include <assets_importer/assets_importer.h>

#include <utils/agea_log.h>
#include <utils/buffer.h>

#include <vulkan_render_types/vulkan_gpu_types.h>

#include <stb_unofficial/stb.h>

#include <iostream>

namespace agea
{
namespace asset_importer
{
namespace texture_importer
{

bool
extract_texture_from_image(const utils::path& obj_path,
                           utils::buffer& image,
                           uint32_t& w,
                           uint32_t& h)
{
    int tex_width = 0, tex_height = 0, tex_channels = 0;
    auto file = obj_path.str();
    void* pixels = stbi_load(file.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    if (!pixels)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    const uint64_t image_size = tex_width * tex_height * 4ULL;
    w = (uint32_t)tex_width;
    w = (uint32_t)tex_height;

    image.resize(image_size);

    memcpy(image.data(), pixels, (size_t)image_size);

    stbi_image_free(pixels);

    return true;
}

bool
extract_texture_from_buffer(utils::buffer& image_buffer,
                            utils::buffer& image,
                            uint32_t& w,
                            uint32_t& h)

{
    int tex_width = 0, tex_height = 0, tex_channels = 0;

    void* pixels = stbi_load_from_memory((stbi_uc*)image_buffer.data(), image_buffer.size(),
                                         &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    if (!pixels)
    {
        ALOG_LAZY_ERROR;
        return false;
    }
    const uint64_t image_size = tex_width * tex_height * 4ULL;
    w = (uint32_t)tex_width;
    w = (uint32_t)tex_height;

    image.resize(image_size);

    memcpy(image.data(), pixels, (size_t)image_size);

    stbi_image_free(pixels);

    return true;
}

}  // namespace texture_importer

}  // namespace asset_importer
}  // namespace agea