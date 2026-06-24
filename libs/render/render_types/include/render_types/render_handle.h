#pragma once

#include <utils/handle.h>
#include <utils/laned_pool.h>

namespace kryga
{
namespace render
{
// Payload forward declarations for the allocator aliases below. The allocator
// only holds a laned_storage<Kind, T>* dispatch token -- an incomplete T is
// fine for holders (render_translator, the renderer); methods that reach the
// storage (ctor/detach/preallocate) instantiate where the full types exist.
class mesh_data;
class material_data;
class texture_data;
class vulkan_render_data;
class vulkan_directional_light_data;
class vulkan_universal_light_data;
struct ui_text_entry;

namespace types
{

// ---------------------------------------------------------------------------
// resource_kind
//
// The 8-bit type tag carried in every render-resource handle. Lives in this
// small, Vulkan-free lib (render_types) so BOTH the model layer (which holds
// handles on its asset objects, e.g. root::mesh::m_handle) and the render layer
// can name them without the model gaining a Vulkan/gpu_types dependency.
//
// Exposure is a separate policy from existence: every kind below gets a handle
// internally, but only mesh / texture / material / object handle *types* appear
// in model-facing APIs. lightmap / probe handles exist render-side only.
// ---------------------------------------------------------------------------
enum class resource_kind : uint8_t
{
    mesh = 0,
    texture = 1,
    material = 2,
    object = 3,  // per-instance render_data slot

    // light slots: the handle index is the GPU SSBO slot. Model-side the light
    // component holds the handle (like a mesh component holds its object handle).
    directional_light = 4,
    universal_light = 5,  // point + spot (unified)

    // internal — handle exists, never handed to the model:
    lightmap = 6,
    probe = 7,

    // CPU-side UI text draw entries (render-thread storage). Not a GPU slot;
    // pooled here so widgets hold a handle like any other render resource.
    ui_text = 8,
};

// Model-exposed handle types.
using mesh_handle = utils::handle<static_cast<uint8_t>(resource_kind::mesh)>;
using texture_handle = utils::handle<static_cast<uint8_t>(resource_kind::texture)>;
using material_handle = utils::handle<static_cast<uint8_t>(resource_kind::material)>;
using render_object_handle = utils::handle<static_cast<uint8_t>(resource_kind::object)>;
using directional_light_handle =
    utils::handle<static_cast<uint8_t>(resource_kind::directional_light)>;
using universal_light_handle = utils::handle<static_cast<uint8_t>(resource_kind::universal_light)>;
using ui_text_handle = utils::handle<static_cast<uint8_t>(resource_kind::ui_text)>;

// Storage + allocator types, one pair per (kind, payload) — mirrors the handle
// aliases above so holders (the loader, render_translator, the renderer's system
// pools, test fixtures) never spell the template arguments by hand.
//
// Lane convention (who claims which lane of a shared storage):
//   meshes/materials: ONE storage each, system allocator (renderer) lane 0,
//                     content allocator (render_translator) lane 1.
//   everything else:  single-lane storages, the lone allocator on lane 0.
constexpr uint32_t k_system_lane = 0;
constexpr uint32_t k_content_lane = 1;

using mesh_storage = utils::laned_storage<static_cast<uint8_t>(resource_kind::mesh), mesh_data>;
using material_storage =
    utils::laned_storage<static_cast<uint8_t>(resource_kind::material), material_data>;
// Two texture pools share the kind but NOT the payload: the bindless pool
// stores texture_data by value, the content handle map stores texture_data*.
using bindless_texture_storage =
    utils::laned_storage<static_cast<uint8_t>(resource_kind::texture), texture_data>;
using texture_ref_storage =
    utils::laned_storage<static_cast<uint8_t>(resource_kind::texture), texture_data*>;
using object_storage =
    utils::laned_storage<static_cast<uint8_t>(resource_kind::object), vulkan_render_data>;
using dir_light_storage =
    utils::laned_storage<static_cast<uint8_t>(resource_kind::directional_light),
                         vulkan_directional_light_data>;
using uni_light_storage = utils::laned_storage<static_cast<uint8_t>(resource_kind::universal_light),
                                               vulkan_universal_light_data>;
using ui_text_storage =
    utils::laned_storage<static_cast<uint8_t>(resource_kind::ui_text), ui_text_entry>;

using mesh_allocator = utils::lane_allocator<static_cast<uint8_t>(resource_kind::mesh), mesh_data>;
using material_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::material), material_data>;
using texture_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::texture), texture_data*>;
using bindless_texture_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::texture), texture_data>;
using render_object_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::object), vulkan_render_data>;
using directional_light_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::directional_light),
                          vulkan_directional_light_data>;
using universal_light_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::universal_light),
                          vulkan_universal_light_data>;
using ui_text_allocator =
    utils::lane_allocator<static_cast<uint8_t>(resource_kind::ui_text), ui_text_entry>;

}  // namespace types
}  // namespace render
}  // namespace kryga
