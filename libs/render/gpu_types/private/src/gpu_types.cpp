#include "gpu_types/dynamic_layout/gpu_types.h"

namespace kryga
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
    case gpu_type::g_uvec4:
    case gpu_type::g_mat2:
        return 16;
    case gpu_type::g_uvec2:
        return 8;
    case gpu_type::g_uvec3:
        return 12;
    case gpu_type::g_mat3:
        return 9 * 4;
    case gpu_type::g_mat4:
        return 16 * 4;
    default:
        break;
    }

    KRG_never("Should be never here!");

    return 0;
}

const char*
gpu_type::name(gpu_type::id t)
{
    switch (t)
    {
    case gpu_type::g_float:
        return "gpu_float";
    case gpu_type::g_int:
        return "gpu_int";
    case gpu_type::g_unsigned:
        return "gpu_unsigned";
    case gpu_type::g_color:
        return "gpu_color";
    case gpu_type::g_double:
        return "gpu_double";
    case gpu_type::g_vec2:
        return "gpu_vec2";
    case gpu_type::g_vec3:
        return "gpu_vec3";
    case gpu_type::g_vec4:
        return "gpu_vec4";
    case gpu_type::g_mat2:
        return "gpu_mat2";
    case gpu_type::g_mat3:
        return "gpu_mat3";
    case gpu_type::g_mat4:
        return "gpu_mat4";
    case gpu_type::g_uvec2:
        return "gpu_uvec2";
    case gpu_type::g_uvec3:
        return "gpu_uvec3";
    case gpu_type::g_uvec4:
        return "gpu_uvec4";
    default:
        break;
    }

    return "nan";
}

std::shared_ptr<kryga::utils::dynobj_layout>
get_default_vertex_inout_layout()
{
    static auto l = gpu_dynobj_builder()
                        .set_id(AID("interface"))
                        .add_field(AID("in_position"), render::gpu_type::g_vec3, 1)
                        .add_field(AID("in_normal"), render::gpu_type::g_vec3, 1)
                        .add_field(AID("in_color"), render::gpu_type::g_vec3, 1)
                        .add_field(AID("in_tex_coord"), render::gpu_type::g_vec2, 1)
                        .add_field(AID("in_lightmap_uv"), render::gpu_type::g_vec2, 1)
                        .finalize();

    return l;
}

}  // namespace render
}  // namespace kryga
