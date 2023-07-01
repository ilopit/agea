#include "vulkan_render/shader_reflection_utils.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <utils/dynamic_object.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

namespace agea
{
namespace render
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

}  // namespace
void
shader_reflection_utils::spvr_get_fields(SpvReflectTypeDescription& parent, gpu_dynobj_builder& dl)
{
    for (uint32_t i = 0; i < parent.member_count; ++i)
    {
        auto& member = parent.members[i];
        auto type = agea::render::gpu_type::g_nan;
        uint32_t alligment = 0;
        uint32_t items_count = 0;

        switch (member.op)
        {
        case SpvOpTypeMatrix:
        {
            AGEA_check(member.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT, "only floats");
            AGEA_check(
                member.traits.numeric.matrix.row_count == member.traits.numeric.matrix.column_count,
                "h != w");
            type = (agea::render::gpu_type::id)((uint32_t)::agea::render::gpu_type::g_mat2 +
                                                member.traits.numeric.matrix.column_count - 2);

            alligment = 16;
            break;
        }
        case SpvOpTypeFloat:
        {
            type = agea::render::gpu_type::id::g_float;
            alligment = sizeof(float);
            break;
        }
        case SpvOpTypeVector:
        {
            type = (agea::render::gpu_type::id)((uint32_t)::agea::render::gpu_type::g_vec2 +
                                                member.traits.numeric.vector.component_count - 2);
            alligment = 16;
            break;
        }
        case SpvOpTypeArray:
        {
            if (member.type_name)
            {
                continue;
            }

            // TODO, FIX

            type = agea::render::gpu_type::id::g_float;

            items_count = member.traits.array.dims[0];

            alligment = 16;
            break;
        }
        case SpvOpTypeInt:
        {
            AGEA_check(member.type_flags & SPV_REFLECT_TYPE_FLAG_INT, "only floats");

            type = member.traits.numeric.scalar.signedness ? agea::render::gpu_type::id::g_int
                                                           : agea::render::gpu_type::id::g_unsigned;

            alligment = sizeof(uint32_t);

            break;
        }
        default:
            // AGEA_never("Unhendled op type");
            continue;
            break;
        }

        if (items_count)
        {
            dl.add_array(AID(member.struct_member_name), type, alligment, items_count, 1);
        }
        else
        {
            dl.add_field(AID(member.struct_member_name), type, alligment);
        }
    }
}

bool
shader_reflection_utils::spvr_to_dyn_layout(SpvReflectTypeDescription* obj, gpu_dynobj_builder& dl)
{
    if (obj->op == SpvOp::SpvOpTypeStruct && (obj->member_count == 1) &&
        obj->members[0].op == SpvOpTypeRuntimeArray)
    {
        SpvReflectTypeDescription& buffer_type = obj->members[0];

        dl.set_id(AID(buffer_type.type_name));
        spvr_get_fields(buffer_type, dl);
    }
    else if (obj->op == SpvOp::SpvOpTypeStruct)
    {
        dl.set_id(AID(obj->type_name));
        spvr_get_fields(*obj, dl);
    }
    return true;
}

bool
shader_reflection_utils::convert_spvr_to_dyn_layout(const utils::id& field_name,
                                                    SpvReflectTypeDescription& obj,
                                                    gpu_dynobj_builder& dl)
{
    auto type = agea::render::gpu_type::g_nan;
    uint32_t alligment = 0;
    uint32_t items_count = 0;

    switch (obj.op)
    {
    case SpvOpTypeMatrix:
    {
        AGEA_check(obj.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT, "Only floats");
        AGEA_check(obj.traits.numeric.matrix.row_count == obj.traits.numeric.matrix.column_count,
                   "h != w");
        type = (agea::render::gpu_type::id)((uint32_t)::agea::render::gpu_type::g_mat2 +
                                            obj.traits.numeric.matrix.column_count - 2);

        alligment = 16;
        break;
    }
    case SpvOpTypeFloat:
    {
        type = agea::render::gpu_type::id::g_float;
        alligment = sizeof(float);
        break;
    }
    case SpvOpTypeVector:
    {
        type = (agea::render::gpu_type::id)((uint32_t)::agea::render::gpu_type::g_vec2 +
                                            obj.traits.numeric.vector.component_count - 2);
        alligment = 16;
        break;
    }
    case SpvOpTypeArray:
    {
        // TODO, FIX

        type = agea::render::gpu_type::id::g_float;

        items_count = obj.traits.array.dims[0];

        alligment = 16;
        break;
    }
    case SpvOpTypeInt:
    {
        AGEA_check(obj.type_flags & SPV_REFLECT_TYPE_FLAG_INT, "Only ints");

        type = obj.traits.numeric.scalar.signedness ? agea::render::gpu_type::id::g_int
                                                    : agea::render::gpu_type::id::g_unsigned;

        alligment = sizeof(uint32_t);

        break;
    }
    default:
        AGEA_never("Unsupported!");
        break;
    }

    if (items_count)
    {
        dl.add_array(field_name, type, alligment, items_count, 1);
    }
    else
    {
        dl.add_field(field_name, type, 1);
    }

    return true;
}

bool
shader_reflection_utils::build_shader_input_reflection(SpvReflectShaderModule& spv_reflection,
                                                       reflection::shader_reflection& sr,
                                                       std::shared_ptr<shader_data>& sd)
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

    return extract_interface_variable(inputs, sr.input_layout);
}

bool
shader_reflection_utils::extract_interface_variable(
    std::vector<SpvReflectInterfaceVariable*>& inputs, utils::dynobj_layout_sptr& r)
{
    std::sort(inputs.begin(), inputs.end(),
              [](SpvReflectInterfaceVariable* l, SpvReflectInterfaceVariable* r)
              { return l->location < r->location; });

    gpu_dynobj_builder gdb;

    for (uint64_t i = 0; i < inputs.size(); ++i)
    {
        auto input = inputs[i];

        if (i != input->location)
        {
            if (input->location == uint32_t(-1))
            {
                break;
            }
            AGEA_never("");
        }

        if (!convert_spvr_to_dyn_layout(AID(input->name), *input->type_description, gdb))
        {
            return false;
        }
    }

    r = gdb.finalize();

    return true;
}

bool
shader_reflection_utils::build_shader_output_reflection(SpvReflectShaderModule& spv_reflection,
                                                        reflection::shader_reflection& sr,
                                                        std::shared_ptr<shader_data>& sd)
{
    uint32_t count = 0;
    auto result = spvReflectEnumerateOutputVariables(&spv_reflection, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectInterfaceVariable*> inputs(count);
    result = spvReflectEnumerateOutputVariables(&spv_reflection, &count, inputs.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    return extract_interface_variable(inputs, sr.output_layout);
}

bool
shader_reflection_utils::are_layouts_compatible(const utils::dynobj_layout_sptr& l,
                                                const utils::dynobj_layout_sptr& r,
                                                bool non_strict_name_check)
{
    if (l.get() == r.get())
    {
        return true;
    }

    AGEA_check(l && r, "Should exists!");

    auto size = l->get_fields().size();

    if (size != r->get_fields().size())
    {
        return false;
    }

    for (auto i = 0; i < size; ++i)
    {
        if (!compare_dynfield(l->get_fields()[i], r->get_fields()[i], non_strict_name_check))
        {
            return false;
        }
    }

    return true;
}

bool
shader_reflection_utils::are_types_compatible(uint32_t lraw, uint32_t rraw)
{
    auto l = (agea::render::gpu_type::id)lraw;
    auto r = (agea::render::gpu_type::id)rraw;

    if (l == r)
    {
        return true;
    }

    if (l == agea::render::gpu_type::g_color)
    {
        return r == agea::render::gpu_type::g_vec4;
    }
    else if (r == agea::render::gpu_type::g_color)
    {
        return l == agea::render::gpu_type::g_vec4;
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
                                          bool non_strict_name_check)
{
    if (!compare_ids(l.id, r.id, non_strict_name_check))
    {
        return false;
    }

    if (!are_types_compatible(l.type, r.type))
    {
        return false;
    }

    if (l.items_count != r.items_count)
    {
        return false;
    }

    if (l.index != r.index)
    {
        return false;
    }

    return are_layouts_compatible(l.sub_field_layout, r.sub_field_layout, non_strict_name_check);
}

bool
shader_reflection_utils::build_shader_reflection(render_device* device,
                                                 reflection::shader_reflection& sr,
                                                 std::shared_ptr<shader_data>& sd)
{
    SpvReflectShaderModuleHandle spvmodule;

    auto result = spvReflectCreateShaderModule(sd->code().size(), sd->code().data(), &spvmodule.h);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    if (!build_shader_input_reflection(spvmodule.h, sr, sd))
    {
        return false;
    }

    if (!build_shader_output_reflection(spvmodule.h, sr, sd))
    {
        return false;
    }

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&spvmodule.h, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&spvmodule.h, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    for (size_t set_idx = 0; set_idx < sets.size(); ++set_idx)
    {
        const SpvReflectDescriptorSet& spv_set = *(sets[set_idx]);

        auto& reflection_set = sr.sets.emplace_back();
        reflection_set.set = spv_set.set;

        for (uint32_t binding_idx = 0; binding_idx < spv_set.binding_count; ++binding_idx)
        {
            const SpvReflectDescriptorBinding& spv_binding = *(spv_set.bindings[binding_idx]);

            auto& refl_binding = reflection_set.binding.emplace_back();

            refl_binding.stage = static_cast<VkShaderStageFlagBits>(spvmodule.h.shader_stage);
            refl_binding.name = spv_binding.name;
            refl_binding.binding = spv_binding.binding;
            refl_binding.set = spv_set.set;
            refl_binding.descriptor_type = (VkDescriptorType)spv_binding.descriptor_type;

            if (agea::string_utils::starts_with(refl_binding.name, "dyn_"))
            {
                if (refl_binding.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                {
                    refl_binding.descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                }
                else if (refl_binding.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    refl_binding.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }
            }

            refl_binding.descriptors_count = 1;
            for (uint32_t dim_idx = 0; dim_idx < spv_binding.array.dims_count; ++dim_idx)
            {
                refl_binding.descriptors_count *= spv_binding.array.dims[dim_idx];
            }

            gpu_dynobj_builder gdb;
            if (!spvr_to_dyn_layout(spv_binding.type_description, gdb))
            {
                return false;
            }

            refl_binding.layout = gdb.finalize();
        }
    }

    result = spvReflectEnumeratePushConstantBlocks(&spvmodule.h, &count, NULL);
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

    std::vector<SpvReflectBlockVariable*> pconstants(count);
    result = spvReflectEnumeratePushConstantBlocks(&spvmodule.h, &count, pconstants.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    auto& c = sr.constants;

    c.name = pconstants[0]->name;
    c.offset = pconstants[0]->offset;
    c.size = pconstants[0]->size;
    c.stage = static_cast<VkShaderStageFlagBits>(spvmodule.h.shader_stage);

    gpu_dynobj_builder gdb;

    if (!spvr_to_dyn_layout(pconstants[0]->type_description, gdb))
    {
        return false;
    }

    sr.constants_layout = gdb.finalize();

    return true;
}

}  // namespace render
}  // namespace agea