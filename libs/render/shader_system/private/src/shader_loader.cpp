#include "shader_system/shader_loader.h"

#include "shader_system/shader_compiler.h"

#include <vfs/io.h>

#include <utils/kryga_log.h>

#include <string>
#include <string_view>

namespace kryga::render
{

namespace
{

vfs::rid
cache_rid_for(const vfs::rid& spv_rid)
{
    std::string rel = "shader_cache/";
    rel += spv_rid.mount_point();
    rel += '/';
    rel += spv_rid.relative();
    return vfs::rid("rtcache", rel);
}

bool
strip_spv_suffix(std::string_view rel, std::string& out)
{
    constexpr std::string_view ext = ".spv";
    if (rel.size() <= ext.size() ||
        rel.substr(rel.size() - ext.size()) != ext)
    {
        return false;
    }
    out.assign(rel.data(), rel.size() - ext.size());
    return true;
}

}  // namespace

shader_load_result
shader_loader::load(const vfs::rid& spv_rid)
{
    kryga::utils::buffer out;

    if (vfs::load_buffer(spv_rid, out))
    {
        return out;
    }

    auto cache_rid = cache_rid_for(spv_rid);
    if (vfs::load_buffer(cache_rid, out))
    {
        return out;
    }

    std::string source_rel;
    if (!strip_spv_suffix(spv_rid.relative(), source_rel))
    {
        ALOG_ERROR("shader_loader::load: rid '{}' does not end in .spv", spv_rid.str());
        return std::unexpected(result_code::validation_error);
    }
    vfs::rid source_rid(spv_rid.mount_point(), source_rel);

    kryga::utils::buffer source;
    if (!vfs::load_buffer(source_rid, source))
    {
        ALOG_ERROR("shader_loader::load: source not found '{}'", source_rid.str());
        return std::unexpected(result_code::path_not_found);
    }

    auto compiled = shader_compiler::compile_shader(source, {});
    if (!compiled.has_value())
    {
        ALOG_ERROR("shader_loader::load: compile failed '{}'", source_rid.str());
        return std::unexpected(result_code::compilation_failed);
    }

    auto& spv = compiled.value().spirv;
    std::vector<uint8_t> blob(spv.data(), spv.data() + spv.size());

    if (!vfs::save_file(cache_rid, blob))
    {
        ALOG_WARN("shader_loader::load: failed to cache '{}'", cache_rid.str());
    }

    out.full_data() = std::move(blob);
    out.set_vpath(spv_rid.str());
    return out;
}

}  // namespace kryga::render
