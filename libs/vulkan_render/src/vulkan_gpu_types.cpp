#include "vulkan_render/types/vulkan_gpu_types.h"

namespace agea
{
namespace render
{

uint32_t
gpu_type::size(gpu_type::id t)
{
    switch (t)
    {
    case gpu_type::g_float:
    case gpu_type::g_int:
    case gpu_type::g_unsigned:
    case gpu_type::g_color:
        return 4;
    case gpu_type::g_double:
    case gpu_type::g_vec2:
        return 8;
    case gpu_type::g_vec3:
        return 12;
    case gpu_type::g_vec4:
    case gpu_type::g_mat2:
        return 16;
    case gpu_type::g_mat3:
        return 9 * 4;
    case gpu_type::g_mat4:
        return 16 * 4;
    default:
        break;
    }

    AGEA_never("Should be never here!");

    return 0;
}

std::shared_ptr<agea::utils::dynobj_layout>
get_default_vertex_inout_layout()
{
    static auto l = gpu_dynobj_builder()
                        .add_field(AID("in_position"), render::gpu_type::g_vec3, 1)
                        .add_field(AID("in_normal"), render::gpu_type::g_vec3, 1)
                        .add_field(AID("in_color"), render::gpu_type::g_vec3, 1)
                        .add_field(AID("in_tex_coord"), render::gpu_type::g_vec2, 1)
                        .finalize();

    return l;
}

}  // namespace render
}  // namespace agea