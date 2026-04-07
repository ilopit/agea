#include "assets_importer/assets_importer.h"

#include "assets_importer/mesh_importer.h"
#include "assets_importer/texture_importer.h"
#include "assets_importer/uv2_generator.h"

#include <utils/kryga_log.h>

#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/texture.h>

#include <core/object_constructor.h>
#include <core/package.h>

#include <serialization/serialization.h>

namespace kryga
{
namespace
{

bool
convert_3do_to_amsh(const utils::path& obj_path,
                    const utils::path& dst_folder_path,
                    const utils::id& mesh_id)
{
    utils::buffer vertices;
    utils::buffer indices;

    if (!asset_importer::mesh_importer::extract_mesh_data_from_3do(
            obj_path, vertices.make_view<gpu::vertex_data>(), indices.make_view<gpu::uint>()))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    // Generate lightmap UV2 coordinates via xatlas
    {
        auto* verts_ptr = reinterpret_cast<const gpu::vertex_data*>(vertices.data());
        uint32_t vert_count = static_cast<uint32_t>(vertices.size() / sizeof(gpu::vertex_data));
        auto* inds_ptr = reinterpret_cast<const gpu::uint*>(indices.data());
        uint32_t ind_count = static_cast<uint32_t>(indices.size() / sizeof(gpu::uint));

        auto uv2_result = asset_importer::uv2_generator::generate_uv2(
            verts_ptr, vert_count, inds_ptr, ind_count);

        if (uv2_result.success)
        {
            // Replace vertex/index buffers with UV2-enriched versions (may have more
            // vertices due to UV seam splitting)
            vertices.resize(uv2_result.vertices.size() * sizeof(gpu::vertex_data));
            memcpy(vertices.data(), uv2_result.vertices.data(),
                   uv2_result.vertices.size() * sizeof(gpu::vertex_data));

            indices.resize(uv2_result.indices.size() * sizeof(gpu::uint));
            memcpy(indices.data(), uv2_result.indices.data(),
                   uv2_result.indices.size() * sizeof(gpu::uint));
        }
        else
        {
            ALOG_ERROR("assets_importer: UV2 generation failed for {}, lightmap UVs will be invalid",
                       mesh_id.str());
        }
    }

    auto obj_ext = mesh_id.str() + ".aobj";
    auto vert_ext = mesh_id.str() + ".avrt";
    auto ind_ext = mesh_id.str() + ".aind";
    vertices.set_file(dst_folder_path / APATH("class/meshes") / vert_ext);
    indices.set_file(dst_folder_path / APATH("class/meshes") / ind_ext);

    auto full_obj_path = dst_folder_path / APATH("class/meshes") / obj_ext;
    auto obj = core::object_constructor::alloc_empty_object<root::mesh>();

    std::filesystem::create_directories(full_obj_path.parent().fs());

    core::package p(AID("dummy"));
    p.set_save_root_path(dst_folder_path);

    auto mesh = obj->as<root::mesh>();
    mesh->set_package(&p);
    mesh->set_indices_buffer(indices);
    mesh->set_vertices_buffer(vertices);

    if (core::object_constructor::object_save(*mesh, full_obj_path) != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

bool
convert_image_to_atxt(const utils::path& obj_path,
                      const utils::path& dst_folder_path,
                      const utils::id& texture_id)
{
    utils::buffer base_color;

    uint32_t w = 0, h = 0;
    if (!asset_importer::texture_importer::extract_texture_from_image(obj_path, base_color, w, h))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    auto obj_ext = texture_id.str() + ".aobj";
    auto base_color_ext = texture_id.str() + ".atbc";
    base_color.set_file(dst_folder_path / APATH("class/textures") / base_color_ext);

    auto full_obj_path = dst_folder_path / APATH("class/textures") / obj_ext;

    auto obj = core::object_constructor::alloc_empty_object<root::texture>();
    std::filesystem::create_directories(full_obj_path.parent().fs());

    core::package p(AID("dummy"));
    p.set_save_root_path(dst_folder_path);

    auto txt = obj->as<root::texture>();
    txt->set_package(&p);
    txt->set_base_color(std::move(base_color));
    txt->set_width(w);
    txt->set_height(h);

    if (core::object_constructor::object_save(*txt, full_obj_path) != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

}  // namespace
}  // namespace kryga