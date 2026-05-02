#pragma once

#include <vfs/rid.h>
#include <utils/buffer.h>
#include <error_handling/error_handling.h>

#include <expected>

namespace kryga::render
{

using shader_load_result = std::expected<kryga::utils::buffer, result_code>;

class shader_loader
{
public:
    // Resolution order:
    //   1. data:// (precooked .spv from tools/cook)
    //   2. rtcache://shader_cache/<rel> (previously runtime-compiled)
    //   3. data:// source (.vert/.frag/.comp) → glslc → cache to rtcache://
    static shader_load_result
    load(const vfs::rid& spv_rid);
};

}  // namespace kryga::render
