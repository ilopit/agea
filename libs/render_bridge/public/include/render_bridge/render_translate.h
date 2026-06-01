#pragma once

// Stateless model→render translation helpers. These build render-side data
// (create-infos, queue ids, packed GPU bytes, sampler indices) from model
// objects. They hold no state and touch no per-frame queue/arena — that's why
// they're free functions here rather than methods on render_bridge, which owns
// the stateful command lifecycle (build/destroy/transform + dependency graph).

#include <core/reflection/reflection_type.h>  // gpu_texture_slot_ref
#include <gpu_types/gpu_generic_constants.h>  // KGPU_MAX_TEXTURE_SLOTS

#include <utils/dynamic_object.h>

#include <vulkan_render/vulkan_render_loader_create_infos.h>  // render::shader_effect_create_info

#include <cstdint>
#include <string>
#include <unordered_map>

namespace kryga
{
namespace root
{
class smart_object;
class shader_effect;
class sampler;
}  // namespace root

namespace render
{
class material_data;
class mesh_data;
}  // namespace render

struct collected_gpu_data
{
    utils::dynobj gpu_data;
    reflection::gpu_texture_slot_ref texture_slots[KGPU_MAX_TEXTURE_SLOTS];
    uint32_t texture_slot_count = 0;
};

namespace render_translate
{

render::shader_effect_create_info
make_se_ci(root::shader_effect& se_model);

std::string
make_qid(render::material_data& mt_data, render::mesh_data& m_data);

std::string
make_qid_from_model(root::smart_object& mat_model, root::smart_object& mesh_model);

uint8_t
map_sampler_to_static_index(const root::sampler& smp);

collected_gpu_data
collect_gpu_data(root::smart_object& so);

// Collect all bool properties in "Specialization" category as {name → value} pairs
std::unordered_map<std::string, uint32_t>
collect_spec_constants(root::smart_object& so);

void
set_material_texture_bindings(utils::dynobj& gpu_data,
                              const uint32_t* texture_indices,
                              const uint32_t* sampler_indices,
                              uint32_t slot_count);

}  // namespace render_translate
}  // namespace kryga
