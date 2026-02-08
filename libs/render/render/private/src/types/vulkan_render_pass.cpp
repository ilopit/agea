#include "vulkan_render/types/vulkan_render_pass.h"

#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"

#include <global_state/global_state.h>
#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"
#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_compute_shader_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"

#include <utils/kryga_log.h>

#include <vulkan/vulkan.h>

namespace kryga::render
{

// =============================================================================
// Lifecycle
// =============================================================================

render_pass::render_pass(const utils::id& name, rg_pass_type type)
    : m_name(name)
    , m_type(type)
{
}

render_pass::~render_pass()
{
    if (m_vk_render_pass != VK_NULL_HANDLE)
    {
        glob::glob_state().getr_render_device().delete_immediately(
            [=](VkDevice vkd, VmaAllocator)
            {
                vkDestroyRenderPass(vkd, m_vk_render_pass, nullptr);

                for (auto f : m_framebuffers)
                {
                    vkDestroyFramebuffer(vkd, f, nullptr);
                }
            });
    }

    m_color_image_views.clear();
    m_color_images.clear();
}

// =============================================================================
// Execution
// =============================================================================

bool
render_pass::begin(VkCommandBuffer cmd,
                   uint64_t swapchain_image_index,
                   uint32_t width,
                   uint32_t height)
{
    if (m_vk_render_pass == VK_NULL_HANDLE || m_framebuffers.empty())
    {
        return false;
    }

    auto fb_idx = swapchain_image_index % m_framebuffers.size();

    auto rp_info = vk_utils::make_renderpass_begin_info(m_vk_render_pass, VkExtent2D{width, height},
                                                        m_framebuffers[fb_idx]);

    VkClearValue clear_values[2];
    clear_values[0].color = m_clear_color;
    clear_values[1].depthStencil = {1.0f, 0};

    rp_info.clearValueCount = 2;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

bool
render_pass::end(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
    return true;
}

void
render_pass::execute(VkCommandBuffer cmd,
                     uint64_t swapchain_image_index,
                     uint32_t width,
                     uint32_t height)
{
    if (is_graphics())
    {
        begin(cmd, swapchain_image_index, width, height);

        if (m_execute)
        {
            m_execute(cmd);
        }

        end(cmd);
    }
    else
    {
        // Compute or transfer pass: just execute callback
        if (m_execute)
        {
            m_execute(cmd);
        }
    }
}

// =============================================================================
// Shader effect management
// =============================================================================

result_code
render_pass::create_shader_effect(const kryga::utils::id& id,
                                  const shader_effect_create_info& info,
                                  shader_effect_data*& sed)
{
    KRG_check(!get_shader_effect(id), "should never happens");

    auto effect = std::make_shared<shader_effect_data>(id);

    auto info_copy = info;
    info_copy.rp = this;

    auto rc = vulkan_shader_loader::create_shader_effect(*effect, info_copy);

    if (rc != result_code::ok)
    {
        effect->m_failed_load = true;
        effect->set_owner_render_pass(this);
        m_shader_effects[id] = effect;
        sed = effect.get();
        return rc;
    }

    // Validate shader bindings against binding table
    if (effect->m_vertex_stage && effect->m_frag_stage && m_binding_table.is_finalized())
    {
        bool validation_passed = m_binding_table.validate_shader(
            effect->m_vertex_stage->get_reflection(), effect->m_frag_stage->get_reflection());

        if (!validation_passed)
        {
            effect->m_failed_load = true;
            effect->set_owner_render_pass(this);
            m_shader_effects[id] = effect;
            sed = effect.get();
            return result_code::validation_error;
        }
    }

    effect->m_failed_load = false;
    effect->set_owner_render_pass(this);

    m_shader_effects[id] = effect;
    sed = effect.get();

    return rc;
}

result_code
render_pass::update_shader_effect(shader_effect_data& se_data,
                                  const shader_effect_create_info& info)
{
    KRG_check(get_shader_effect(se_data.get_id()), "should never happens");

    std::shared_ptr<render::shader_effect_data> old_se_data;

    auto info_copy = info;
    info_copy.rp = this;

    auto rc = vulkan_shader_loader::update_shader_effect(se_data, info_copy, old_se_data);

    se_data.m_failed_load = rc != result_code::ok;

    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    return rc;
}

shader_effect_data*
render_pass::get_shader_effect(const kryga::utils::id& id)
{
    auto itr = m_shader_effects.find(id);
    return itr != m_shader_effects.end() ? itr->second.get() : nullptr;
}

void
render_pass::destroy_shader_effect(const kryga::utils::id& id)
{
    auto itr = m_shader_effects.find(id);
    if (itr != m_shader_effects.end())
    {
        m_shader_effects.erase(itr);
    }
}

// =============================================================================
// Compute shader management
// =============================================================================

result_code
render_pass::create_compute_shader(const kryga::utils::id& id,
                                   const compute_shader_create_info& info,
                                   compute_shader_data*& csd)
{
    KRG_check(!get_compute_shader(id), "Compute shader already exists");

    auto shader = std::make_shared<compute_shader_data>(id);

    auto info_copy = info;
    info_copy.pass = this;

    auto rc = vulkan_compute_shader_loader::create_compute_shader(*shader, info_copy);

    if (rc != result_code::ok)
    {
        shader->m_failed_load = true;
        shader->set_owner_pass(this);
        m_compute_shaders[id] = shader;
        csd = shader.get();
        return rc;
    }

    // Validate shader bindings against binding table
    if (shader->m_compute_stage && m_binding_table.is_finalized())
    {
        bool validation_passed =
            m_binding_table.validate_shader(shader->m_compute_stage->get_reflection());

        if (!validation_passed)
        {
            shader->m_failed_load = true;
            shader->set_owner_pass(this);
            m_compute_shaders[id] = shader;
            csd = shader.get();
            return result_code::validation_error;
        }
    }

    shader->m_failed_load = false;
    shader->set_owner_pass(this);

    m_compute_shaders[id] = shader;
    csd = shader.get();

    return rc;
}

compute_shader_data*
render_pass::get_compute_shader(const kryga::utils::id& id)
{
    auto itr = m_compute_shaders.find(id);
    return itr != m_compute_shaders.end() ? itr->second.get() : nullptr;
}

void
render_pass::destroy_compute_shader(const kryga::utils::id& id)
{
    auto itr = m_compute_shaders.find(id);
    if (itr != m_compute_shaders.end())
    {
        m_compute_shaders.erase(itr);
    }
}

// =============================================================================
// Binding table API
// =============================================================================

binding_table&
render_pass::bindings()
{
    KRG_check(!m_binding_table.is_finalized(), "Cannot modify finalized bindings");
    return m_binding_table;
}

const binding_table&
render_pass::bindings() const
{
    return m_binding_table;
}

bool
render_pass::finalize_bindings(vk_utils::descriptor_layout_cache& layout_cache)
{
    return m_binding_table.finalize(layout_cache);
}

bool
render_pass::are_bindings_finalized() const
{
    return m_binding_table.is_finalized();
}

bool
render_pass::validate_resources(const vulkan_render_graph& graph) const
{
    return m_binding_table.validate_resources(graph);
}

void
render_pass::bind(const utils::id& name, vk_utils::vulkan_buffer& buf)
{
    m_binding_table.bind_buffer(name, buf);
}

void
render_pass::bind(const utils::id& name,
                  vk_utils::vulkan_image& img,
                  VkImageView view,
                  VkSampler sampler)
{
    m_binding_table.bind_image(name, img, view, sampler);
}

VkDescriptorSet
render_pass::get_descriptor_set(uint32_t set_index, vk_utils::descriptor_allocator& allocator)
{
    return m_binding_table.build_set(set_index, allocator);
}

VkDescriptorSetLayout
render_pass::get_set_layout(uint32_t set_index) const
{
    return m_binding_table.get_layout(set_index);
}

void
render_pass::begin_frame()
{
    m_binding_table.begin_frame();
}

// =============================================================================
// Pass properties accessors
// =============================================================================

const utils::id&
render_pass::name() const
{
    return m_name;
}

void
render_pass::set_name(const utils::id& name)
{
    m_name = name;
}

rg_pass_type
render_pass::type() const
{
    return m_type;
}

const std::vector<rg_resource_ref>&
render_pass::resources() const
{
    return m_resources;
}

std::vector<rg_resource_ref>&
render_pass::resources()
{
    return m_resources;
}

void
render_pass::set_clear_color(VkClearColorValue color)
{
    m_clear_color = color;
}

void
render_pass::set_execute_callback(std::function<void(VkCommandBuffer)> callback)
{
    m_execute = std::move(callback);
}

uint32_t
render_pass::order() const
{
    return m_order;
}

void
render_pass::set_order(uint32_t order)
{
    m_order = order;
}

// =============================================================================
// Attachment info accessors
// =============================================================================

VkRenderPass
render_pass::vk() const
{
    return m_vk_render_pass;
}

bool
render_pass::is_graphics() const
{
    return m_type == rg_pass_type::graphics && m_vk_render_pass != VK_NULL_HANDLE;
}

VkFormat
render_pass::get_color_format() const
{
    return m_color_format;
}

VkFormat
render_pass::get_depth_format() const
{
    return m_depth_format;
}

uint32_t
render_pass::get_color_attachment_count() const
{
    return m_color_attachment_count;
}

std::vector<vk_utils::vulkan_image_sptr>
render_pass::get_color_images() const
{
    return m_color_images;
}

std::vector<vk_utils::vulkan_image_view_sptr>
render_pass::get_color_image_views() const
{
    return m_color_image_views;
}

// =============================================================================
// Validation
// =============================================================================

bool
render_pass::validate_fragment_outputs(const reflection::interface_block& frag_outputs) const
{
    uint32_t output_count = 0;
    for (const auto& var : frag_outputs.variables)
    {
        (void)var;
        output_count++;
    }

    if (output_count != m_color_attachment_count)
    {
        ALOG_ERROR("Fragment shader has {} output(s), but render pass has {} color attachment(s)",
                   output_count, m_color_attachment_count);
        return false;
    }

    return true;
}

}  // namespace kryga::render
