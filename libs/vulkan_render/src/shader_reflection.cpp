#include "vulkan_render/shader_reflection.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <utils/dynamic_object.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

#include <spirv_reflect.h>

namespace agea
{
namespace render
{
namespace
{

void
spvr_get_fields(SpvReflectTypeDescription& parent, agea::utils::dynobj_layout& dl)
{
    utils::dynamic_object_layout_sequence_builder<gpu_type> sb;

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
            sb.add_array(AID(member.struct_member_name), type, alligment, items_count, 1);
        }
        else
        {
            sb.add_field(AID(member.struct_member_name), type, alligment);
        }
    }
    dl = *sb.get_layout();
}

bool
spvr_to_dyn_layout(SpvReflectTypeDescription* obj, utils::dynobj_layout& dl)
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
}  // namespace

using UniqueSPVModule =
    std::unique_ptr<SpvReflectShaderModule, std::function<void(SpvReflectShaderModule*)>>;

bool
build_shader_reflection(render_device* device,
                        reflection::shader_reflection& sr,
                        std::shared_ptr<shader_data>& sd)
{
    UniqueSPVModule spvmodule(new SpvReflectShaderModule,
                              [](SpvReflectShaderModule* spvmodule)
                              {
                                  if (spvmodule)
                                  {
                                      spvReflectDestroyShaderModule(spvmodule);
                                  }
                              });

    auto result =
        spvReflectCreateShaderModule(sd->code().size(), sd->code().data(), spvmodule.get());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(spvmodule.get(), &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(spvmodule.get(), &count, sets.data());
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

            refl_binding.stage = static_cast<VkShaderStageFlagBits>(spvmodule->shader_stage);
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

            if (!spvr_to_dyn_layout(spv_binding.type_description, refl_binding.layout))
            {
                return false;
            }
        }
    }

    result = spvReflectEnumeratePushConstantBlocks(spvmodule.get(), &count, NULL);
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
    result = spvReflectEnumeratePushConstantBlocks(spvmodule.get(), &count, pconstants.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    auto& c = sr.constants;

    c.name = pconstants[0]->name;
    c.offset = pconstants[0]->offset;
    c.size = pconstants[0]->size;
    c.stage = static_cast<VkShaderStageFlagBits>(spvmodule->shader_stage);

    if (!spvr_to_dyn_layout(pconstants[0]->type_description, sr.constants_layout))
    {
        return false;
    }

    return true;
}

}  // namespace render
}  // namespace agea