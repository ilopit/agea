#pragma once

#include <utils/path.h>
#include <utils/id.h>

#include <string>

namespace agea
{
namespace utils
{
struct buffer;
}

namespace asset_importer
{
namespace texture_importer
{

bool
extract_texture_from_image(const utils::path& obj_path,
                           utils::buffer& image,
                           uint32_t& w,
                           uint32_t& h);
bool
extract_texture_from_buffer(utils::buffer& image_buffer,
                            utils::buffer& image,
                            uint32_t& w,
                            uint32_t& h);

}  // namespace texture_importer
}  // namespace asset_importer
}  // namespace agea