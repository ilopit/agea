#include "shader_system/shader_reflection.h"

#include <utils/dynamic_object.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>
#include <utils/kryga_log.h>

#include <algorithm>

namespace kryga::render
{
namespace
{
struct SpvReflectShaderModuleHandle
{
    SpvReflectShaderModule h;

    ~SpvReflectShaderModuleHandle()
    {
        spvReflectDestroyShaderModule(&h);
    }
};

VkShaderStageFlagBits
convert_spv_stage(SpvExecutionModel model)
{
    switch (model)
    {
    case SpvExecutionModelVertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case SpvExecutionModelFragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SpvExecutionModelGLCompute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case SpvExecutionModelGeometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case SpvExecutionModelTessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case SpvExecutionModelTessellationEvaluation:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    default:
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }
}

gpu_type::id
spv_type_to_gpu_type(SpvReflectTypeDescription& desc)
{
    switch (desc.op)
    {
    case SpvOpTypeFloat:
        return gpu_type::g_float;
    case SpvOpTypeInt:
        return desc.traits.numeric.scalar.signedness ? gpu_type::g_int : gpu_type::g_unsigned;
    case SpvOpTypeVector:
        return (gpu_type::id)((uint32_t)gpu_type::g_vec2 +
                              desc.traits.numeric.vector.component_count - 2);
    case SpvOpTypeMatrix:
        return (gpu_type::id)((uint32_t)gpu_type::g_mat2 + desc.traits.numeric.matrix.column_count -
                              2);
    default:
        return gpu_type::nan;
    }
}

}  // namespace

bool
shader_reflection_utils::convert_spvr_to_dyn_layout(const utils::id& field_name,
                                                    SpvReflectTypeDescription& obj,
                                                    gpu_dynobj_builder& dl)
{
    auto type = kryga::render::gpu_type::nan;

    utils::dynobj_field df;
    df.alligment = 1;
    df.id = field_name;

    switch (obj.op)
    {
    case SpvOpTypeMatrix:
    {
        KRG_check(obj.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT, "Only floats");
        KRG_check(obj.traits.numeric.matrix.row_count == obj.traits.numeric.matrix.column_count,
                  "h != w");
        type = (kryga::render::gpu_type::id)((uint32_t)::kryga::render::gpu_type::g_mat2 +
                                             obj.traits.numeric.matrix.column_count - 2);

        df.alligment = 16;
        break;
    }
    case SpvOpTypeFloat:
    {
        type = kryga::render::gpu_type::id::g_float;
        df.alligment = sizeof(float);
        break;
    }
    case SpvOpTypeVector:
    {
        type = (kryga::render::gpu_type::id)((uint32_t)::kryga::render::gpu_type::g_vec2 +
                                             obj.traits.numeric.vector.component_count - 2);
        df.alligment = 16;
        break;
    }
    case SpvOpTypeArray:
    {
        // TODO, FIX
        df.items_count = obj.traits.array.dims[0];

        if (obj.member_count == 0)
        {
            type = kryga::render::gpu_type::id::g_float;
        }
        else
        {
            gpu_dynobj_builder gdb;
            gdb.set_id(obj.type_name ? AID(obj.type_name) : AID("array"));

            for (auto from = obj.members; from < obj.members + obj.member_count; from++)
            {
                auto member_name =
                    from->struct_member_name ? AID(from->struct_member_name) : AID("element");
                convert_spvr_to_dyn_layout(member_name, *from, gdb);
            }

            df.sub_field_layout = gdb.finalize();
        }

        break;
    }
    case SpvOpTypeInt:
    {
        KRG_check(obj.type_flags & SPV_REFLECT_TYPE_FLAG_INT, "Only ints");

        type = obj.traits.numeric.scalar.signedness ? kryga::render::gpu_type::id::g_int
                                                    : kryga::render::gpu_type::id::g_unsigned;

        df.alligment = sizeof(uint32_t);

        break;
    }
    case SpvOpTypeStruct:
    {
        gpu_dynobj_builder gdb;
        gdb.set_id(obj.type_name ? AID(obj.type_name) : AID("struct"));

        for (auto from = obj.members; from < obj.members + obj.member_count; from++)
        {
            auto member_name =
                from->struct_member_name ? AID(from->struct_member_name) : AID("member");
            convert_spvr_to_dyn_layout(member_name, *from, gdb);
        }

        df.sub_field_layout = gdb.finalize();

        break;
    }
    case SpvOpTypeRuntimeArray:
    {
        gpu_dynobj_builder gdb;
        gdb.set_id(obj.type_name ? AID(obj.type_name) : AID("runtime_array"));

        for (auto from = obj.members; from < obj.members + obj.member_count; from++)
        {
            auto member_name =
                from->struct_member_name ? AID(from->struct_member_name) : AID("element");
            convert_spvr_to_dyn_layout(member_name, *from, gdb);
        }
        df.items_count = uint64_t(-1);
        df.sub_field_layout = gdb.finalize();
        break;
    }
    case SpvOpTypeSampledImage:
    case SpvOpTypeImage:
    case SpvOpTypeSampler:
        // Opaque types - no layout needed
        return true;
    default:
        KRG_never("Unsupported!");
        break;
    }

    dl.finalize_field(type, df);

    dl.add_field(std::move(df));

    return true;
}

bool
shader_reflection_utils::build_shader_input_reflection(SpvReflectShaderModule& spv_reflection,
                                                       reflection::shader_reflection& sr)
{
    uint32_t count = 0;
    auto result = spvReflectEnumerateInputVariables(&spv_reflection, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectInterfaceVariable*> inputs(count);
    result = spvReflectEnumerateInputVariables(&spv_reflection, &count, inputs.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    return extract_interface_variables(inputs, sr.input_interface);
}

bool
shader_reflection_utils::extract_interface_variables(
    std::vector<SpvReflectInterfaceVariable*>& inputs, reflection::interface_block& block)
{
    std::sort(inputs.begin(), inputs.end(),
              [](SpvReflectInterfaceVariable* l, SpvReflectInterfaceVariable* r)
              { return l->location < r->location; });

    gpu_dynobj_builder gdb;
    gdb.set_id(AID("interface"));

    block.variables.clear();

    for (uint64_t i = 0; i < inputs.size(); ++i)
    {
        auto input = inputs[i];

        // Skip built-in variables (location == -1)
        if (input->location == uint32_t(-1))
        {
            continue;
        }

        // Fill gaps with empty slots
        while (gdb.get_layout()->get_fields().size() < input->location)
        {
            gdb.add_empty();
        }

        // Add interface variable metadata
        reflection::interface_variable var;
        var.name = input->name ? AID(input->name) : AID("var");
        var.location = input->location;
        var.type = spv_type_to_gpu_type(*input->type_description);
        block.variables.push_back(var);

        if (!convert_spvr_to_dyn_layout(var.name, *input->type_description, gdb))
        {
            return false;
        }
    }

    block.layout = gdb.finalize();

    return true;
}

bool
shader_reflection_utils::build_shader_output_reflection(SpvReflectShaderModule& spv_reflection,
                                                        reflection::shader_reflection& sr)
{
    uint32_t count = 0;
    auto result = spvReflectEnumerateOutputVariables(&spv_reflection, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectInterfaceVariable*> outputs(count);
    result = spvReflectEnumerateOutputVariables(&spv_reflection, &count, outputs.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    return extract_interface_variables(outputs, sr.output_interface);
}

bool
shader_reflection_utils::build_shader_descriptor_sets_reflection(
    SpvReflectShaderModule& spv_reflection, reflection::shader_reflection& sr)
{
    uint32_t count = 0;
    auto result = spvReflectEnumerateDescriptorSets(&spv_reflection, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> spv_sets(count);
    result = spvReflectEnumerateDescriptorSets(&spv_reflection, &count, spv_sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::sort(spv_sets.begin(), spv_sets.end(),
              [](SpvReflectDescriptorSet* l, SpvReflectDescriptorSet* r)
              { return l->set < r->set; });

    for (uint64_t i = 0; i < spv_sets.size(); ++i)
    {
        auto spv_set = spv_sets[i];

        auto& refl_ds = sr.descriptors.emplace_back();
        refl_ds.set_index = spv_set->set;

        std::vector<SpvReflectDescriptorBinding*> spv_bindings(
            spv_set->bindings, spv_set->bindings + spv_set->binding_count);

        std::sort(spv_bindings.begin(), spv_bindings.end(),
                  [](SpvReflectDescriptorBinding* l, SpvReflectDescriptorBinding* r)
                  { return l->binding < r->binding; });

        for (auto spv_binding : spv_bindings)
        {
            auto& refl_binding = refl_ds.bindings.emplace_back();
            refl_binding.binding_index = spv_binding->binding;
            refl_binding.type = (VkDescriptorType)spv_binding->descriptor_type;

            // Determine binding name: prefer instance name, fall back to type name
            // (SPIRV without debug info may not preserve instance names for SSBOs with runtime arrays)
            const char* name_str = nullptr;
            if (spv_binding->name && spv_binding->name[0] != '\0')
            {
                name_str = spv_binding->name;
            }
            else if (spv_binding->type_description && spv_binding->type_description->type_name &&
                     spv_binding->type_description->type_name[0] != '\0')
            {
                name_str = spv_binding->type_description->type_name;
                ALOG_TRACE("Using type name '{}' as binding name for set={}, binding={}",
                           name_str, spv_set->set, spv_binding->binding);
            }

            if (!name_str)
            {
                ALOG_ERROR("Descriptor binding at set={}, binding={} has no name",
                           spv_set->set, spv_binding->binding);
                return false;
            }

            auto binding_name = AID(name_str);
            refl_binding.name = binding_name;

            gpu_dynobj_builder binding_gdb;

            binding_gdb.set_id(AID("binding"));

            if (!convert_spvr_to_dyn_layout(binding_name, *spv_binding->type_description,
                                            binding_gdb))
            {
                return false;
            }

            refl_binding.descriptors_count = 1;

            for (uint32_t dim_idx = 0; dim_idx < spv_binding->array.dims_count; ++dim_idx)
            {
                refl_binding.descriptors_count *= spv_binding->array.dims[dim_idx];
            }

            refl_binding.layout = binding_gdb.finalize();
        }
    }

    return true;
}

bool
shader_reflection_utils::are_layouts_compatible(const utils::dynobj_layout_sptr& l,
                                                const utils::dynobj_layout_sptr& r,
                                                bool non_strict_name_check,
                                                bool ignore_highlevel_naming)
{
    if (l.get() == r.get())
    {
        return true;
    }

    KRG_check(l && r, "Should exists!");

    auto size = l->get_fields().size();

    if (size != r->get_fields().size())
    {
        return false;
    }

    for (auto i = 0; i < size; ++i)
    {
        if (!compare_dynfield(l->get_fields()[i], r->get_fields()[i], non_strict_name_check,
                              ignore_highlevel_naming))
        {
            return false;
        }
    }

    return true;
}

bool
shader_reflection_utils::are_types_compatible(uint32_t lraw, uint32_t rraw)
{
    auto l = (kryga::render::gpu_type::id)lraw;
    auto r = (kryga::render::gpu_type::id)rraw;

    if (l == r)
    {
        return true;
    }

    if (l == kryga::render::gpu_type::g_color)
    {
        return r == kryga::render::gpu_type::g_vec4;
    }
    else if (r == kryga::render::gpu_type::g_color)
    {
        return l == kryga::render::gpu_type::g_vec4;
    }

    return false;
}

namespace
{

bool
compare_ids(const utils::id& l, const utils::id& r, bool non_strict_name_check)
{
    auto ll = l.str();
    auto rr = r.str();

    if (non_strict_name_check && string_utils::starts_with(ll, "out_") &&
        string_utils::starts_with(rr, "in_"))
    {
        return strcmp(ll.data() + 4, rr.data() + 3) == 0;
    }

    return l == r;
}

}  // namespace

bool
shader_reflection_utils::compare_dynfield(const utils::dynobj_field& l,
                                          const utils::dynobj_field& r,
                                          bool non_strict_name_check,
                                          bool ignore_highlevel_naming)
{
    if (!ignore_highlevel_naming)
    {
        if (!compare_ids(l.id, r.id, non_strict_name_check))
        {
            return false;
        }
    }

    ignore_highlevel_naming = false;

    if (!are_types_compatible(l.type, r.type))
    {
        return false;
    }

    if (l.index != r.index)
    {
        return false;
    }

    return are_layouts_compatible(l.sub_field_layout, r.sub_field_layout, non_strict_name_check,
                                  ignore_highlevel_naming);
}

bool
shader_reflection_utils::build_shader_compute_info(SpvReflectShaderModule& spv_reflection,
                                                   reflection::shader_reflection& sr)
{
    if (sr.stage != VK_SHADER_STAGE_COMPUTE_BIT)
    {
        return true;
    }

    reflection::compute_info info;
    info.local_size_x = spv_reflection.entry_points->local_size.x;
    info.local_size_y = spv_reflection.entry_points->local_size.y;
    info.local_size_z = spv_reflection.entry_points->local_size.z;

    sr.compute = info;

    return true;
}

bool
shader_reflection_utils::build_shader_reflection(const uint8_t* spirv_code,
                                                 size_t spirv_size,
                                                 reflection::shader_reflection& sr)
{
    SpvReflectShaderModuleHandle spvmodule;

    auto result = spvReflectCreateShaderModule(spirv_size, spirv_code, &spvmodule.h);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    // Extract stage and entry point
    sr.stage = convert_spv_stage(spvmodule.h.spirv_execution_model);
    if (spvmodule.h.entry_point_name)
    {
        sr.entry_point = AID(spvmodule.h.entry_point_name);
    }

    if (!build_shader_input_reflection(spvmodule.h, sr))
    {
        return false;
    }

    if (!build_shader_output_reflection(spvmodule.h, sr))
    {
        return false;
    }

    if (!build_shader_descriptor_sets_reflection(spvmodule.h, sr))
    {
        return false;
    }

    if (!build_shader_push_constants(spvmodule.h, sr))
    {
        return false;
    }

    return build_shader_compute_info(spvmodule.h, sr);
}

bool
shader_reflection_utils::build_shader_push_constants(SpvReflectShaderModule& spv_reflection,
                                                     reflection::shader_reflection& sr)
{
    uint32_t count = 0;

    auto result = spvReflectEnumeratePushConstantBlocks(&spv_reflection, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }
    if (count == 0)
    {
        return true;
    }

    if (count > 1)
    {
        return false;
    }

    SpvReflectBlockVariable* pconstants = nullptr;
    result = spvReflectEnumeratePushConstantBlocks(&spv_reflection, &count, &pconstants);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    // Require instance name for push constants (e.g., "pc" in "} pc;")
    if (!pconstants->name || pconstants->name[0] == '\0')
    {
        ALOG_ERROR("Push constant block '{}' has no instance name. Add instance name after closing "
                   "brace, e.g., '}} pc;'",
                   pconstants->type_description->type_name
                       ? pconstants->type_description->type_name
                       : "<anonymous>");
        return false;
    }

    gpu_dynobj_builder gdb;
    gdb.set_id(AID("push_constant"));

    // Use instance name (e.g., "pc") as field name, type_description contains struct info
    auto instance_name = AID(pconstants->name);
    if (!convert_spvr_to_dyn_layout(instance_name, *pconstants->type_description, gdb))
    {
        return false;
    }

    sr.constants = reflection::push_constants{
        .offset = pconstants->offset, .size = pconstants->size, .layout = gdb.finalize()};

    return true;
}

VkDescriptorSetLayoutBinding
shader_reflection_utils::convert_to_vk_binding(const reflection::binding& b,
                                               VkShaderStageFlags stage)
{
    VkDescriptorSetLayoutBinding layout_binding;

    layout_binding.descriptorType = b.type;

    if (kryga::string_utils::starts_with(b.name.str(), "dyn_"))
    {
        if (layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }
        else if (layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        }
    }

    layout_binding.binding = b.binding_index;
    layout_binding.stageFlags = stage;
    layout_binding.descriptorCount = b.descriptors_count;
    layout_binding.pImmutableSamplers = nullptr;

    return layout_binding;
}

void
shader_reflection_utils::convert_to_vk_push_constants(const reflection::push_constants& pc,
                                                      VkShaderStageFlags stage,
                                                      VkPushConstantRange& range)
{
    range.offset = pc.offset;
    range.size = pc.size;
    range.stageFlags = stage;
}

}  // namespace kryga::render
