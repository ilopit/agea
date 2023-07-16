#pragma once

namespace agea
{
namespace render
{
class mesh_data;
class material_data;
class texture_data;
class object_data;
class shader_effect_data;
class shader_data;
class sampler_data;
struct vertex_input_description;
struct texture_sampler_data;

enum class depth_stencil_mode
{
    none = 0,
    enable,
    outline
};

}  // namespace render
}  // namespace agea
