#pragma once

// All concrete render commands, relocated here from the package render-override
// .cpp files and engine/animation so the central dispatch can see every type. Each is
// a POD tagged command (no vtable): it carries cmd_kind (via render_command_base)
// stamped by alloc_cmd<T> from k_kind. These structs are PURE DATA — no methods.
// Behavior lives in free process(cmd, ctx) overloads in render_command_processor.cpp,
// dispatched by render_command_processor::apply.
//
// All live in `namespace kryga` (not root/base) so the emission sites in
// kryga::root / kryga::root still resolve them by enclosing-namespace lookup.

#include "render_translator/render_command.h"

#include <utils/id.h>
#include <utils/buffer.h>
#include <utils/dynamic_object_builder.h>

#include <render_types/render_handle.h>

#include <core/object_layer_flags.h>

#include <vulkan_render/vulkan_render_loader.h>  // render::lightmap_uv

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace kryga
{

namespace render
{
struct vulkan_render_data;
}

// ============================================================================
// Common — per-frame value updates (were render_commands_common.h)
// ============================================================================

struct update_transform_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::update_transform;

    render::types::render_object_handle obj_handle;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 bounding_sphere_center{0.0f};
    float bounding_radius = 0.0f;
};

struct set_outline_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::set_outline;

    render::types::render_object_handle obj_handle;
    bool outlined = false;
};

// ============================================================================
// Meshes / textures / shader effects / materials (were packages/root)
// ============================================================================

struct create_mesh_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_mesh;

    utils::id id;
    render::types::mesh_handle handle;  // pre-reserved by the builder (handle model)
    std::shared_ptr<utils::buffer> vertices;
    std::shared_ptr<utils::buffer> indices;
    bool skinned = false;
};

struct destroy_mesh_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_mesh;

    render::types::mesh_handle handle;
};

struct create_texture_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_texture;

    utils::id id;
    render::types::texture_handle handle;  // pre-reserved by the builder
    std::shared_ptr<utils::buffer> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_kryga_format = false;
};

struct destroy_texture_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_texture;

    render::types::texture_handle handle;
};

struct create_shader_effect_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_shader_effect;

    utils::id id;
    std::shared_ptr<utils::buffer> vert;
    std::shared_ptr<utils::buffer> frag;
    bool is_vert_binary = false;
    bool is_frag_binary = false;
    bool wire_topology = false;
    bool enable_alpha = false;
    std::unordered_map<std::string, uint32_t> spec_constants;
};

struct destroy_shader_effect_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_shader_effect;

    utils::id id;
};

struct texture_slot_info
{
    uint32_t slot = 0;
    render::types::texture_handle texture_handle;
    uint8_t static_sampler_index = 0;
};

struct create_material_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_material;

    utils::id id;
    render::types::material_handle handle;  // pre-reserved by the builder (handle model)
    utils::id type_id;
    utils::id shader_effect_id;
    std::vector<texture_slot_info> texture_slots;
    utils::dynobj gpu_data;
};

struct update_material_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::update_material;

    render::types::material_handle handle;
    utils::id shader_effect_id;
    std::vector<texture_slot_info> texture_slots;
    utils::dynobj gpu_data;
};

struct destroy_material_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_material;

    render::types::material_handle handle;
};

// ============================================================================
// Objects / chunk meshes / lights / skinned meshes (were packages/base)
// ============================================================================

struct create_object_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_object;

    utils::id id;  // passenger: lightmap binding key + diagnostics
    render::types::render_object_handle obj_handle;
    render::types::mesh_handle mesh;
    render::types::material_handle material;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 bounding_sphere_center{0.0f};
    float bounding_radius = 0.0f;
    uint32_t bone_count = 0;
    std::string queue_id;
    // Lightmap binding is resolved on the render thread at execute time from the
    // loader's per-level registry (populated by create_lightmap_cmd), not baked in
    // on the main thread — empty = not lightmapped (skinned/destructible objects).
    utils::id lightmap_level_id;
    core::object_layer_flags layer_flags;
};

struct update_object_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::update_object;

    utils::id id;  // passenger: lightmap binding key
    render::types::render_object_handle obj_handle;
    render::types::mesh_handle mesh;
    render::types::material_handle material;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 bounding_sphere_center{0.0f};
    float bounding_radius = 0.0f;
    std::string queue_id;
    utils::id lightmap_level_id;
    core::object_layer_flags layer_flags;
};

struct destroy_object_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_object;

    render::types::render_object_handle obj_handle;
};

struct create_chunk_mesh_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_chunk_mesh;

    utils::id id;
    render::types::mesh_handle handle;  // pre-reserved by the builder (handle model)
    std::shared_ptr<utils::buffer> vertices;
    std::shared_ptr<utils::buffer> indices;
};

enum class light_kind : uint8_t
{
    directional,
    point,
    spot
};

struct create_light_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_light;

    utils::id id;
    render::types::directional_light_handle dir_handle;  // set when kind==directional
    render::types::universal_light_handle uni_handle;    // set when kind==point/spot
    light_kind kind = light_kind::point;
    glm::vec3 position{0.0f};
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{0.0f};
    glm::vec3 specular{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float radius = 0.0f;
    float cut_off = 0.0f;
    float outer_cut_off = 0.0f;
};

struct update_light_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::update_light;

    utils::id id;
    render::types::directional_light_handle dir_handle;
    render::types::universal_light_handle uni_handle;
    light_kind kind = light_kind::point;
    glm::vec3 position{0.0f};
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{0.0f};
    glm::vec3 specular{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float radius = 0.0f;
    float cut_off = 0.0f;
    float outer_cut_off = 0.0f;
};

struct select_directional_light_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::select_directional_light;

    render::types::directional_light_handle handle;
};

struct destroy_light_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_light;

    render::types::directional_light_handle dir_handle;
    render::types::universal_light_handle uni_handle;
    light_kind kind = light_kind::point;
};

struct create_skinned_mesh_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_skinned_mesh;

    utils::id id;
    render::types::mesh_handle handle;
    std::shared_ptr<utils::buffer> vertices;
    std::shared_ptr<utils::buffer> indices;
};

// ============================================================================
// Lightmap (was engine) / bones (was animation)
// ============================================================================

struct create_lightmap_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::create_lightmap;

    utils::id level_id;
    utils::id tex_id;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels;
    std::unordered_map<utils::id, render::lightmap_uv> entries;
};

struct destroy_lightmap_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::destroy_lightmap;

    utils::id level_id;
};

struct bone_instance_update
{
    render::vulkan_render_data* rd;
    uint32_t offset;
    uint32_t count;
};

struct apply_bones_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::apply_bones;

    std::vector<glm::mat4> matrices{};
    std::vector<bone_instance_update> updates{};
};

// ============================================================================
// UI panels (packages/ui retained-mode widgets)
//
// Raw pixel rect (top-left origin) + flat color. The pixel->NDC conversion and
// viewport lookup are deferred to the render thread (process(), where the live
// viewport size is known), so the model thread never reads render state.
// ============================================================================

struct ui_panel_upsert_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::ui_panel_upsert;

    utils::id id;
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
    glm::vec3 color{0.0f};
    float opacity = 1.0f;
    bool visible = true;
};

struct ui_panel_destroy_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::ui_panel_destroy;

    utils::id id;
};

// ============================================================================
// UI text (packages/ui ui_text widget)
//
// Carries the raw string + pixel anchor; glyph layout (per-glyph quads from the
// font atlas metrics) and the pixel->NDC conversion are deferred to the render
// thread (draw_ui_text), where the live viewport + baked metrics are known.
// anchor: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right (matches
// kryga::ui::ui_text_anchor; right/bottom anchors align the text to that edge).
// ============================================================================

struct ui_text_upsert_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::ui_text_upsert;

    static constexpr int k_max_len = 64;

    render::types::ui_text_handle handle;  // pre-reserved by the builder (handle model)
    int32_t x = 0;
    int32_t y = 0;
    uint32_t anchor = 0;
    float font_size = 24.0f;  // pixel height
    glm::vec4 color{1.0f};
    utils::id font;  // baked font id; empty -> loader default
    bool visible = true;
    char text[k_max_len] = {};  // null-terminated, truncated to k_max_len-1
};

struct ui_text_destroy_cmd : render_cmd::render_command_base
{
    static constexpr auto k_kind = render_cmd::render_cmd_kind::ui_text_destroy;

    render::types::ui_text_handle handle;
};

}  // namespace kryga
