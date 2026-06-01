#pragma once

#include <utils/path.h>
#include <utils/id.h>

#include <string>

namespace kryga
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

// True if the path points at an already-cooked kryga texture (.atbc) — i.e. in
// engine format, no decode/import needed.
bool
is_kryga_texture(const utils::path& p);

}  // namespace texture_importer
}  // namespace asset_importer
}  // namespace kryga