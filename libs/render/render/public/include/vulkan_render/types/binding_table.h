#pragma once

#include "vulkan_render/types/vulkan_generic.h"
#include "vulkan_render/vk_descriptors.h"

#include <shader_system/shader_reflection.h>

#include <utils/id.h>
#include <utils/check.h>

#include <vulkan/vulkan.h>

#include <array>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kryga::render
{

// Forward declarations
class vulkan_render_graph;
class shader_effect_data;

namespace vk_utils
{
class vulkan_buffer;
class vulkan_image;
}  // namespace vk_utils

enum class binding_scope
{
    per_pass,     // bound once per pass via descriptors
    per_material  // bound per draw call (textures) - validated but not managed
};

struct binding_spec
{
    utils::id name;
    uint32_t set_index = 0;
    uint32_t binding_index = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    VkShaderStageFlags stages = 0;
    binding_scope scope = binding_scope::per_pass;
};

class binding_table
{
public:
    binding_table() = default;
    ~binding_table() = default;

    // === Declaration phase (mutable until finalized) ===

    binding_table&
    add(const utils::id& name,
        uint32_t set,
        uint32_t binding,
        VkDescriptorType type,
        VkShaderStageFlags stages,
        binding_scope scope = binding_scope::per_pass);

    // Finalize - builds layouts, no more modifications allowed
    bool
    finalize(vk_utils::descriptor_layout_cache& layout_cache);

    // === Validation ===

    // Validate shader bindings against this table
    // Returns true if all shader bindings are satisfied, logs errors on failure
    bool
    validate_shader(const reflection::shader_reflection& vertex_refl,
                    const reflection::shader_reflection& frag_refl) const;

    // Validate single shader stage (for compute shaders)
    bool
    validate_shader(const reflection::shader_reflection& refl) const;

    // Validate material has required per_material bindings
    // material_bindings: map of binding name -> descriptor type
    bool
    validate_material_bindings(
        const std::unordered_map<utils::id, VkDescriptorType>& material_bindings) const;

    // Validate per_pass bindings exist in the render graph's resource registry.
    bool
    validate_resources(const vulkan_render_graph& graph, const char* pass_name = nullptr) const;

    // === Binding phase (per-frame for per_pass scope) ===

    void
    bind_buffer(const utils::id& name, vk_utils::vulkan_buffer& buf);

    void
    bind_image(const utils::id& name,
               vk_utils::vulkan_image& img,
               VkImageView view,
               VkSampler sampler);

    // Build descriptor set from currently bound resources
    // Returns VK_NULL_HANDLE if missing bindings or not finalized
    VkDescriptorSet
    build_set(uint32_t set_index, vk_utils::descriptor_allocator& allocator);

    // Reset bound resources for new frame
    void
    begin_frame();

    // === Accessors ===

    VkDescriptorSetLayout
    get_layout(uint32_t set_index) const
    {
        KRG_check(m_finalized, "Binding table not finalized");
        return set_index < DESCRIPTORS_SETS_COUNT ? m_layouts[set_index] : VK_NULL_HANDLE;
    }

    const binding_spec*
    find_binding(const utils::id& name) const;

    const binding_spec*
    find_binding(uint32_t set_index, uint32_t binding_index) const;

    bool
    is_finalized() const
    {
        return m_finalized;
    }

    const std::vector<binding_spec>&
    get_bindings() const
    {
        return m_bindings;
    }

    // Get all per_pass bindings for a specific set
    std::vector<const binding_spec*>
    get_pass_bindings(uint32_t set_index) const;

private:
    bool
    validate_single_binding(const reflection::binding& b,
                            VkShaderStageFlags stage,
                            const char* stage_name) const;

    struct bound_resource
    {
        vk_utils::vulkan_buffer* buffer = nullptr;
        vk_utils::vulkan_image* image = nullptr;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    std::vector<binding_spec> m_bindings;
    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> m_layouts{};
    std::unordered_map<utils::id, bound_resource> m_bound;

    bool m_finalized = false;
};

}  // namespace kryga::render
