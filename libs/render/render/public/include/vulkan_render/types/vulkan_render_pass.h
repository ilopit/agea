#pragma once

#include <vulkan_render/types/vulkan_generic.h>
#include <vulkan_render/types/binding_table.h>

#include <vulkan_render/utils/vulkan_image.h>

#include <vulkan_render/render_graph_types.h>

#include <shader_system/shader_reflection.h>

#include <error_handling/error_handling.h>

#include <utils/id.h>

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace kryga
{
namespace render
{

struct shader_effect_create_info;
class shader_effect_data;

class render_pass
{
public:
    friend class render_pass_builder;

    render_pass() = default;
    explicit render_pass(const utils::id& name, rg_pass_type type = rg_pass_type::graphics);
    ~render_pass();

    KRG_gen_class_non_copyable(render_pass);

    // Begin/end render pass (for graphics passes only)
    bool
    begin(VkCommandBuffer cmd, uint64_t swapchain_image_index, uint32_t width, uint32_t height);

    bool
    end(VkCommandBuffer cmd);

    // Execute the pass (handles graphics/compute/transfer appropriately)
    void
    execute(VkCommandBuffer cmd, uint64_t swapchain_image_index, uint32_t width, uint32_t height);

    VkRenderPass
    vk()
    {
        return m_vk_render_pass;
    }

    bool
    is_graphics() const
    {
        return m_type == rg_pass_type::graphics && m_vk_render_pass != VK_NULL_HANDLE;
    }

    std::vector<vk_utils::vulkan_image_sptr>
    get_color_images() const
    {
        return m_color_images;
    }

    std::vector<vk_utils::vulkan_image_view_sptr>
    get_color_image_views() const
    {
        return m_color_image_views;
    }

    ::kryga::result_code
    create_shader_effect(const kryga::utils::id& id,
                         const shader_effect_create_info& info,
                         shader_effect_data*& sed);

    ::kryga::result_code
    update_shader_effect(shader_effect_data& se_data, const shader_effect_create_info& info);

    shader_effect_data*
    get_shader_effect(const kryga::utils::id& id);

    void
    destroy_shader_effect(const kryga::utils::id& id);

    // Render graph pass properties (merged from rg_vk_pass)
    utils::id m_name;
    rg_pass_type m_type = rg_pass_type::graphics;
    std::vector<rg_resource_ref> m_resources;
    VkClearColorValue m_clear_color = {0, 0, 0, 0};
    std::function<void(VkCommandBuffer)> m_execute;
    rg_pass_barriers m_barriers;
    uint32_t m_order = 0;

    // Attachment format info (for shader compatibility validation)
    VkFormat
    get_color_format() const
    {
        return m_color_format;
    }

    VkFormat
    get_depth_format() const
    {
        return m_depth_format;
    }

    uint32_t
    get_color_attachment_count() const
    {
        return m_color_attachment_count;
    }

    bool
    has_depth_attachment() const
    {
        return m_depth_format != VK_FORMAT_UNDEFINED;
    }

    // Validate shader compatibility with this render pass
    // Returns true if compatible, false otherwise with error message in out_error
    bool
    validate_fragment_outputs(const reflection::interface_block& frag_outputs,
                              std::string& out_error) const;

    // Validate that shader bindings have corresponding resources declared in this render pass
    // Returns true if all shader bindings are satisfied, false otherwise with error in out_error
    bool
    validate_shader_resources(const reflection::shader_reflection& vertex_reflection,
                              const reflection::shader_reflection& frag_reflection,
                              std::string& out_error) const;

    // === Binding table API ===

    // Access binding table for declaration (only before finalize)
    binding_table&
    bindings()
    {
        KRG_check(!m_binding_table.is_finalized(), "Cannot modify finalized bindings");
        return m_binding_table;
    }

    const binding_table&
    bindings() const
    {
        return m_binding_table;
    }

    // Finalize bindings - must be called before adding shader effects
    bool
    finalize_bindings(vk_utils::descriptor_layout_cache& layout_cache)
    {
        return m_binding_table.finalize(layout_cache);
    }

    bool
    are_bindings_finalized() const
    {
        return m_binding_table.is_finalized();
    }

    // Per-frame binding shortcuts
    void
    bind(const utils::id& name, vk_utils::vulkan_buffer& buf)
    {
        m_binding_table.bind_buffer(name, buf);
    }

    void
    bind(const utils::id& name, vk_utils::vulkan_image& img, VkImageView view, VkSampler sampler)
    {
        m_binding_table.bind_image(name, img, view, sampler);
    }

    // Build descriptor set from bound resources
    VkDescriptorSet
    get_descriptor_set(uint32_t set_index, vk_utils::descriptor_allocator& allocator)
    {
        return m_binding_table.build_set(set_index, allocator);
    }

    // Reset bindings for new frame
    void
    begin_frame()
    {
        m_binding_table.begin_frame();
    }

    // Get descriptor set layout for a set index
    VkDescriptorSetLayout
    get_set_layout(uint32_t set_index) const
    {
        return m_binding_table.get_layout(set_index);
    }

private:
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
    VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
    uint32_t m_color_attachment_count = 0;
    std::vector<VkFramebuffer> m_framebuffers;

    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;

    std::vector<vk_utils::vulkan_image_view> m_depth_image_views;
    std::vector<vk_utils::vulkan_image> m_depth_images;

    VkRenderPass m_vk_render_pass = VK_NULL_HANDLE;

    std::unordered_map<kryga::utils::id, std::shared_ptr<shader_effect_data>> m_shader_effects;

    binding_table m_binding_table;
};

using render_pass_sptr = std::shared_ptr<render_pass>;

class render_pass_builder
{
public:
    enum class presets
    {
        swapchain,
        buffer,
        picking

    };

    render_pass_builder&
    set_color_format(VkFormat f)
    {
        m_color_format = f;
        return *this;
    }

    render_pass_builder&
    set_depth_format(VkFormat f)
    {
        m_depth_format = f;
        return *this;
    }

    render_pass_builder&
    set_preset(presets p)
    {
        m_preset = p;
        return *this;
    }

    render_pass_builder&
    set_color_images(const std::vector<vk_utils::vulkan_image_view_sptr>& ivs,
                     const std::vector<vk_utils::vulkan_image_sptr>& is);

    render_pass_builder&
    set_width_depth(uint32_t w, uint32_t h)
    {
        m_width = w;
        m_height = h;

        return *this;
    }

    render_pass_builder&
    set_enable_stencil(bool stencil)
    {
        m_enable_stencil = stencil;

        return *this;
    }

    render_pass_sptr
    build();

    std::array<VkSubpassDependency, 2>
    get_dependencies();

    std::array<VkAttachmentDescription, 2>
    get_attachments();

private:
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
    VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

    uint32_t m_width = 0U;
    uint32_t m_height = 0U;

    presets m_preset = presets::swapchain;

    bool m_enable_stencil = true;

    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;
};

}  // namespace render
}  // namespace kryga
