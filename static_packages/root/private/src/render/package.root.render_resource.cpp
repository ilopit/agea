#include "packages/root/package.root.h"

#include <vulkan_render/agea_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/vk_descriptors.h>

#include <utils/static_initializer.h>

namespace agea::root
{

AGEA_schedule_static_init(
    []() {
        package::instance().register_package_extention<package::package_render_resources_loader>();
    });

bool
package::package_render_resources_loader::load(core::static_package& s)
{
    auto vl = render::gpu_dynobj_builder()
                  .add_field(AID("vPosition"), render::gpu_type::g_vec3, 1)
                  .add_field(AID("vNormal"), render::gpu_type::g_vec3, 1)
                  .add_field(AID("vColor"), render::gpu_type::g_vec3, 1)
                  .add_field(AID("vTexCoord"), render::gpu_type::g_vec2, 1)
                  .finalize();

    auto val = render::gpu_dynobj_builder().add_array(AID("verts"), vl, 1, 4, 4).finalize();

    utils::buffer vert_buffer(val->get_object_size());

    {
        auto v = val->make_view<render::gpu_type>(vert_buffer.data());

        using v3 = glm::vec3;
        using v2 = glm::vec2;

        v.subobj(0, 0).write(v3{-1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 0.f});
        v.subobj(0, 1).write(v3{1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.0, 0.f});
        v.subobj(0, 2).write(v3{-1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 1.f});
        v.subobj(0, 3).write(v3{1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.f, 1.f});
    }

    utils::buffer index_buffer(6 * 4);
    auto v = index_buffer.make_view<uint32_t>();
    v.at(0) = 0;
    v.at(1) = 2;
    v.at(2) = 1;
    v.at(3) = 2;
    v.at(4) = 3;
    v.at(5) = 1;

    auto vertices = vert_buffer.make_view<render::gpu_vertex_data>();
    auto indices = index_buffer.make_view<render::gpu_index_data>();

    glob::vulkan_render_loader::getr().create_mesh(AID("plane_mesh"), vertices, indices);

    return true;
}

}  // namespace agea::root