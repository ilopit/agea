#pragma once

#include <vulkan_render/types/vulkan_generic.h>
#include <vulkan_render/types/binding_table.h>
#include <vulkan_render/utils/vulkan_image.h>
#include <vulkan_render/render_graph_types.h>

#include <shader_system/shader_reflection.h>

#include <error_handling/error_handling.h>

#include <utils/id.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace kryga::render
{

struct shader_effect_create_info;
struct compute_shader_create_info;
class shader_effect_data;
class compute_shader_data;
class render_pass_builder;

// TODO: Rename render_pass to just "pass" - it handles both graphics and compute passes.
// The current name is misleading for compute workloads.
class render_pass
{
public:
    friend class render_pass_builder;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    render_pass() = default;
    explicit render_pass(const utils::id& name, rg_pass_type type = rg_pass_type::graphics);
    ~render_pass();

    KRG_gen_class_non_copyable(render_pass);

    // =========================================================================
    // Execution
    // =========================================================================

    bool
    begin(VkCommandBuffer cmd, uint64_t swapchain_image_index, uint32_t width, uint32_t height);

    bool
    end(VkCommandBuffer cmd);

    void
    execute(VkCommandBuffer cmd, uint64_t swapchain_image_index, uint32_t width, uint32_t height);

    // =========================================================================
    // Shader effect management (graphics passes)
    // =========================================================================

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

    // =========================================================================
    // Compute shader management (compute passes)
    // =========================================================================

    ::kryga::result_code
    create_compute_shader(const kryga::utils::id& id,
                          const compute_shader_create_info& info,
                          compute_shader_data*& csd);

    compute_shader_data*
    get_compute_shader(const kryga::utils::id& id);

    void
    destroy_compute_shader(const kryga::utils::id& id);

    // =========================================================================
    // Binding table API
    // =========================================================================

    binding_table&
    bindings();
    const binding_table&
    bindings() const;

    bool
    finalize_bindings(vk_utils::descriptor_layout_cache& layout_cache);
    bool
    are_bindings_finalized() const;
    bool
    validate_resources(const vulkan_render_graph& graph) const;

    void
    bind(const utils::id& name, vk_utils::vulkan_buffer& buf);
    void
    bind(const utils::id& name, vk_utils::vulkan_image& img, VkImageView view, VkSampler sampler);

    VkDescriptorSet
    get_descriptor_set(uint32_t set_index, vk_utils::descriptor_allocator& allocator);
    VkDescriptorSetLayout
    get_set_layout(uint32_t set_index) const;

    void
    begin_frame();

    // =========================================================================
    // Pass properties accessors
    // =========================================================================

    const utils::id&
    name() const;
    void
    set_name(const utils::id& name);

    rg_pass_type
    type() const;

    const std::vector<rg_resource_ref>&
    resources() const;
    std::vector<rg_resource_ref>&
    resources();

    void
    set_clear_color(VkClearColorValue color);

    void
    set_execute_callback(std::function<void(VkCommandBuffer)> callback);

    uint32_t
    order() const;
    void
    set_order(uint32_t order);

    // =========================================================================
    // Attachment info accessors
    // =========================================================================

    VkRenderPass
    vk() const;
    bool
    is_graphics() const;

    VkFormat
    get_color_format() const;
    VkFormat
    get_depth_format() const;
    uint32_t
    get_color_attachment_count() const;

    std::vector<vk_utils::vulkan_image_sptr>
    get_color_images() const;
    std::vector<vk_utils::vulkan_image_view_sptr>
    get_color_image_views() const;

    VkImageView
    get_depth_image_view(size_t idx) const;

    std::vector<vk_utils::vulkan_image>&
    get_depth_images();
    const std::vector<vk_utils::vulkan_image_view>&
    get_depth_image_views() const;

    // =========================================================================
    // Validation
    // =========================================================================

    bool
    validate_fragment_outputs(const reflection::interface_block& frag_outputs) const;

private:
    // Pass identity and type
    utils::id m_name;
    rg_pass_type m_type = rg_pass_type::graphics;

    // Render graph properties
    std::vector<rg_resource_ref> m_resources;
    VkClearColorValue m_clear_color = {0, 0, 0, 0};
    std::function<void(VkCommandBuffer)> m_execute;
    rg_pass_barriers m_barriers;
    uint32_t m_order = 0;

    // Attachment formats
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
    VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
    uint32_t m_color_attachment_count = 0;

    // Fixed dimensions (0 = use passed width/height from execute)
    uint32_t m_fixed_width = 0;
    uint32_t m_fixed_height = 0;

    // Vulkan resources
    VkRenderPass m_vk_render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    // Image resources
    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;
    std::vector<vk_utils::vulkan_image> m_depth_images;
    std::vector<vk_utils::vulkan_image_view> m_depth_image_views;

    // Shader storage
    std::unordered_map<kryga::utils::id, std::shared_ptr<shader_effect_data>> m_shader_effects;
    std::unordered_map<kryga::utils::id, std::shared_ptr<compute_shader_data>> m_compute_shaders;

    // Binding management
    binding_table m_binding_table;
};

using render_pass_sptr = std::shared_ptr<render_pass>;

}  // namespace kryga::render
