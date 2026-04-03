#include "vulkan_render/types/binding_table.h"

#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"
#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>

#include <utils/kryga_log.h>

#include <algorithm>

namespace kryga::render
{

namespace
{

const char*
descriptor_type_to_string(VkDescriptorType type)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        return "SAMPLER";
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "COMBINED_IMAGE_SAMPLER";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "SAMPLED_IMAGE";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "STORAGE_IMAGE";
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "UNIFORM_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "STORAGE_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "UNIFORM_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "STORAGE_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return "UNIFORM_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return "STORAGE_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "INPUT_ATTACHMENT";
    default:
        return "UNKNOWN";
    }
}

bool
is_buffer_descriptor(VkDescriptorType type)
{
    return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
           type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
           type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
           type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
}

bool
is_image_descriptor(VkDescriptorType type)
{
    return type == VK_DESCRIPTOR_TYPE_SAMPLER ||
           type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
           type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
}

bool
are_types_compatible(VkDescriptorType table_type, VkDescriptorType shader_type)
{
    // Exact match
    if (table_type == shader_type)
    {
        return true;
    }

    // Dynamic variants are compatible with non-dynamic
    if (table_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
        shader_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        return true;
    }
    if (table_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
        shader_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    {
        return true;
    }

    return false;
}

}  // namespace

binding_table&
binding_table::add(const utils::id& name,
                   uint32_t set,
                   uint32_t binding,
                   VkDescriptorType type,
                   VkShaderStageFlags stages,
                   binding_scope scope)
{
    KRG_check(!m_finalized, "Cannot modify finalized binding table");
    KRG_check(set < DESCRIPTORS_SETS_COUNT, "Set index out of range");

    // Check for duplicate name
    for (const auto& b : m_bindings)
    {
        KRG_check(b.name != name, "Duplicate binding name");
        KRG_check(!(b.set_index == set && b.binding_index == binding),
                  "Duplicate set/binding index");
    }

    m_bindings.push_back({name, set, binding, type, stages, scope});
    return *this;
}

bool
binding_table::finalize(vk_utils::descriptor_layout_cache& layout_cache)
{
    if (m_finalized)
    {
        return true;
    }

    // Group bindings by set index
    std::array<std::vector<VkDescriptorSetLayoutBinding>, DESCRIPTORS_SETS_COUNT> set_bindings;

    for (const auto& spec : m_bindings)
    {
        VkDescriptorSetLayoutBinding vk_binding{};
        vk_binding.binding = spec.binding_index;
        vk_binding.descriptorType = spec.type;
        vk_binding.descriptorCount = 1;
        vk_binding.stageFlags = spec.stages;
        vk_binding.pImmutableSamplers = nullptr;

        set_bindings[spec.set_index].push_back(vk_binding);
    }

    // Create layouts for each set
    for (uint32_t i = 0; i < DESCRIPTORS_SETS_COUNT; ++i)
    {
        // Sort bindings by index for consistent layout
        std::sort(set_bindings[i].begin(),
                  set_bindings[i].end(),
                  [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
                  { return a.binding < b.binding; });

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext = nullptr;
        layout_ci.bindingCount = static_cast<uint32_t>(set_bindings[i].size());
        layout_ci.pBindings = set_bindings[i].empty() ? nullptr : set_bindings[i].data();

        m_layouts[i] = layout_cache.create_descriptor_layout(&layout_ci);
    }

    m_finalized = true;
    return true;
}

bool
binding_table::validate_single_binding(const reflection::binding& b,
                                       VkShaderStageFlags stage,
                                       const char* stage_name) const
{
    const binding_spec* spec = find_binding(b.name);

    if (!spec)
    {
        ALOG_ERROR("{} shader binding '{}' (binding={}, type={}) not found in binding table",
                   stage_name,
                   b.name.cstr(),
                   b.binding_index,
                   descriptor_type_to_string(b.type));
        return false;
    }

    // Check type compatibility
    if (!are_types_compatible(spec->type, b.type))
    {
        ALOG_ERROR("{} shader binding '{}' type mismatch: shader expects {}, table has {}",
                   stage_name,
                   b.name.cstr(),
                   descriptor_type_to_string(b.type),
                   descriptor_type_to_string(spec->type));
        return false;
    }

    // Check binding index matches
    if (spec->binding_index != b.binding_index)
    {
        ALOG_ERROR(
            "{} shader binding '{}' index mismatch: shader expects binding={}, table has "
            "binding={}",
            stage_name,
            b.name.cstr(),
            b.binding_index,
            spec->binding_index);
        return false;
    }

    // Check stage flags (shader stage must be subset of declared stages)
    if ((spec->stages & stage) != stage)
    {
        ALOG_ERROR("{} shader binding '{}' stage not declared in binding table",
                   stage_name,
                   b.name.cstr());
        return false;
    }

    return true;
}

bool
binding_table::validate_shader(const reflection::shader_reflection& vertex_refl,
                               const reflection::shader_reflection& frag_refl) const
{
    KRG_check(m_finalized, "Binding table must be finalized before validation");

    // Forward check: every shader binding must exist in the table
    for (const auto& ds : vertex_refl.descriptors)
    {
        for (const auto& b : ds.bindings)
        {
            if (!validate_single_binding(b, VK_SHADER_STAGE_VERTEX_BIT, "Vertex"))
            {
                return false;
            }
        }
    }

    for (const auto& ds : frag_refl.descriptors)
    {
        for (const auto& b : ds.bindings)
        {
            if (!validate_single_binding(b, VK_SHADER_STAGE_FRAGMENT_BIT, "Fragment"))
            {
                return false;
            }
        }
    }

    // Reverse check: for each descriptor set that the shader partially uses,
    // every per_pass table binding in that set must be declared in at least one stage.
    // Pipeline layouts are built from shader reflection, while descriptor sets are
    // allocated from the table's layout. Vulkan requires these layouts to match exactly.
    // If a shader doesn't reference a set at all, it won't bind that set's descriptor,
    // so no layout conflict exists and we skip that set.

    // Collect sets that the shader actually uses
    std::array<bool, DESCRIPTORS_SETS_COUNT> shader_uses_set{};
    for (const auto& ds : vertex_refl.descriptors)
    {
        if (ds.set_index < DESCRIPTORS_SETS_COUNT)
        {
            shader_uses_set[ds.set_index] = true;
        }
    }
    for (const auto& ds : frag_refl.descriptors)
    {
        if (ds.set_index < DESCRIPTORS_SETS_COUNT)
        {
            shader_uses_set[ds.set_index] = true;
        }
    }

    bool valid = true;
    for (const auto& spec : m_bindings)
    {
        if (spec.scope != binding_scope::per_pass)
        {
            continue;
        }

        if (!shader_uses_set[spec.set_index])
        {
            continue;
        }

        bool found = vertex_refl.find_binding(spec.set_index, spec.binding_index) != nullptr ||
                     frag_refl.find_binding(spec.set_index, spec.binding_index) != nullptr;

        if (!found)
        {
            ALOG_ERROR(
                "Binding table entry '{}' (set={}, binding={}) is per_pass but not declared "
                "in any shader stage — pipeline layout will have fewer descriptors than the "
                "allocated descriptor set",
                spec.name.cstr(),
                spec.set_index,
                spec.binding_index);
            valid = false;
        }
    }

    return valid;
}

bool
binding_table::validate_shader(const reflection::shader_reflection& refl) const
{
    KRG_check(m_finalized, "Binding table must be finalized before validation");

    const char* stage_name = "Compute";
    VkShaderStageFlags stage = VK_SHADER_STAGE_COMPUTE_BIT;

    if (refl.stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        stage_name = "Vertex";
        stage = VK_SHADER_STAGE_VERTEX_BIT;
    }
    else if (refl.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        stage_name = "Fragment";
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    for (const auto& ds : refl.descriptors)
    {
        for (const auto& b : ds.bindings)
        {
            if (!validate_single_binding(b, stage, stage_name))
            {
                return false;
            }
        }
    }

    // Reverse check: for sets the shader actually uses, every per_pass table
    // binding must be declared. See two-stage overload for rationale.
    std::array<bool, DESCRIPTORS_SETS_COUNT> shader_uses_set{};
    for (const auto& ds : refl.descriptors)
    {
        if (ds.set_index < DESCRIPTORS_SETS_COUNT)
        {
            shader_uses_set[ds.set_index] = true;
        }
    }

    bool valid = true;
    for (const auto& spec : m_bindings)
    {
        if (spec.scope != binding_scope::per_pass)
        {
            continue;
        }

        if (!shader_uses_set[spec.set_index])
        {
            continue;
        }

        if (refl.find_binding(spec.set_index, spec.binding_index) == nullptr)
        {
            ALOG_ERROR(
                "{} shader missing table binding '{}' (set={}, binding={}) — "
                "pipeline layout will not match allocated descriptor set",
                stage_name,
                spec.name.cstr(),
                spec.set_index,
                spec.binding_index);
            valid = false;
        }
    }

    return valid;
}

bool
binding_table::validate_material_bindings(
    const std::unordered_map<utils::id, VkDescriptorType>& material_bindings) const
{
    KRG_check(m_finalized, "Binding table must be finalized before validation");

    for (const auto& [name, type] : material_bindings)
    {
        const binding_spec* spec = find_binding(name);

        if (!spec)
        {
            ALOG_ERROR("Material binding '{}' not found in binding table", name.cstr());
            return false;
        }

        if (spec->scope != binding_scope::per_material)
        {
            ALOG_ERROR("Material binding '{}' is declared as per_pass, expected per_material",
                       name.cstr());
            return false;
        }

        if (!are_types_compatible(spec->type, type))
        {
            ALOG_ERROR("Material binding '{}' type mismatch: material has {}, table expects {}",
                       name.cstr(),
                       descriptor_type_to_string(type),
                       descriptor_type_to_string(spec->type));
            return false;
        }
    }

    return true;
}

void
binding_table::bind_buffer(const utils::id& name, vk_utils::vulkan_buffer& buf)
{
    KRG_check(m_finalized, "Binding table must be finalized before binding");

    const binding_spec* spec = find_binding(name);
    KRG_check(spec, "Unknown binding name");
    KRG_check(is_buffer_descriptor(spec->type), "Binding is not a buffer type");
    KRG_check(spec->scope == binding_scope::per_pass,
              "Cannot bind per_material binding via bind_buffer");

    m_bound[name] = {&buf, nullptr, VK_NULL_HANDLE, VK_NULL_HANDLE};
}

void
binding_table::bind_image(const utils::id& name,
                          vk_utils::vulkan_image& img,
                          VkImageView view,
                          VkSampler sampler)
{
    KRG_check(m_finalized, "Binding table must be finalized before binding");

    const binding_spec* spec = find_binding(name);
    KRG_check(spec, "Unknown binding name");
    KRG_check(is_image_descriptor(spec->type), "Binding is not an image type");
    KRG_check(spec->scope == binding_scope::per_pass,
              "Cannot bind per_material binding via bind_image");

    m_bound[name] = {nullptr, &img, view, sampler};
}

VkDescriptorSet
binding_table::build_set(uint32_t set_index, vk_utils::descriptor_allocator& allocator)
{
    KRG_check(m_finalized, "Binding table must be finalized before building sets");
    KRG_check(set_index < DESCRIPTORS_SETS_COUNT, "Set index out of range");

    // Get all per_pass bindings for this set
    auto pass_bindings = get_pass_bindings(set_index);
    if (pass_bindings.empty())
    {
        return VK_NULL_HANDLE;
    }

    // Check all bindings are bound
    for (const auto* spec : pass_bindings)
    {
        auto it = m_bound.find(spec->name);
        if (it == m_bound.end())
        {
            ALOG_ERROR("Missing binding for '{}' in set {}", spec->name.cstr(), set_index);
            return VK_NULL_HANDLE;
        }
    }

    // Allocate descriptor set
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (!allocator.allocate(&set, m_layouts[set_index]))
    {
        ALOG_ERROR("Failed to allocate descriptor set for set {}", set_index);
        return VK_NULL_HANDLE;
    }

    // Build write descriptors
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkDescriptorImageInfo> image_infos;

    // Pre-allocate to avoid reallocation invalidating pointers
    buffer_infos.reserve(pass_bindings.size());
    image_infos.reserve(pass_bindings.size());

    for (const auto* spec : pass_bindings)
    {
        const auto& res = m_bound[spec->name];

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = spec->binding_index;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = spec->type;

        if (is_buffer_descriptor(spec->type))
        {
            // For uniform buffers, use get_offset() (actual data size) to stay within limits
            // For storage buffers, use get_alloc_size() (full allocation)
            VkDeviceSize range = res.buffer->get_alloc_size();
            if (spec->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                spec->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                range = res.buffer->get_offset();
                if (range == 0)
                {
                    range = res.buffer->get_alloc_size();
                }
            }
            buffer_infos.push_back({res.buffer->buffer(), 0, range});
            write.pBufferInfo = &buffer_infos.back();
        }
        else if (is_image_descriptor(spec->type))
        {
            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (spec->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                layout = VK_IMAGE_LAYOUT_GENERAL;
            }
            image_infos.push_back({res.sampler, res.view, layout});
            write.pImageInfo = &image_infos.back();
        }

        writes.push_back(write);
    }

    // Update descriptor set
    auto device = glob::glob_state().getr_render_device().vk_device();
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return set;
}

void
binding_table::begin_frame()
{
    m_bound.clear();
}

const binding_spec*
binding_table::find_binding(const utils::id& name) const
{
    for (const auto& b : m_bindings)
    {
        if (b.name == name)
        {
            return &b;
        }
    }
    return nullptr;
}

const binding_spec*
binding_table::find_binding(uint32_t set_index, uint32_t binding_index) const
{
    for (const auto& b : m_bindings)
    {
        if (b.set_index == set_index && b.binding_index == binding_index)
        {
            return &b;
        }
    }
    return nullptr;
}

std::vector<const binding_spec*>
binding_table::get_pass_bindings(uint32_t set_index) const
{
    std::vector<const binding_spec*> result;
    for (const auto& b : m_bindings)
    {
        if (b.set_index == set_index && b.scope == binding_scope::per_pass)
        {
            result.push_back(&b);
        }
    }
    return result;
}

bool
binding_table::validate_resources(const vulkan_render_graph& graph) const
{
    KRG_check(m_finalized, "Binding table must be finalized before resource validation");

    const auto& resources = graph.get_resources();
    bool valid = true;

    for (const auto& spec : m_bindings)
    {
        // Skip per_material bindings - bound per-draw, not from render graph
        if (spec.scope == binding_scope::per_material)
        {
            continue;
        }

        if (resources.find(spec.name) == resources.end())
        {
            ALOG_ERROR("Binding '{}' (set={}, binding={}) not found in render graph resources",
                       spec.name.cstr(),
                       spec.set_index,
                       spec.binding_index);
            valid = false;
        }
    }

    return valid;
}

}  // namespace kryga::render
