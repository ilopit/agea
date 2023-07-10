#include "vulkan_render/shader_reflection_utils.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <utils/dynamic_object.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>
#include <utils/agea_log.h>

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

struct reflection_context : utils::dynobj_field_context
{
    VkDescriptorType descritor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint32_t binding_idx = 0;
    uint32_t set_idx = 0;
    uint32_t descriptors_count = 0;
    VkShaderStageFlags stage = VK_SAMPLER_CREATE_FLAG_BITS_MAX_ENUM;
};

struct push_constant_context : utils::dynobj_field_context
{
    VkPushConstantRange vk_constants;
};

}  // namespace

bool
shader_reflection_utils::convert_spvr_to_dyn_layout(const utils::id& field_name,
                                                    SpvReflectTypeDescription& obj,
                                                    gpu_dynobj_builder& dl)
{
    auto type = agea::render::gpu_type::nan;

    utils::dynobj_field df;
    df.alligment = 1;
    df.id = field_name;

    switch (obj.op)
    {
    case SpvOpTypeMatrix:
    {
        AGEA_check(obj.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT, "Only floats");
        AGEA_check(obj.traits.numeric.matrix.row_count == obj.traits.numeric.matrix.column_count,
                   "h != w");
        type = (agea::render::gpu_type::id)((uint32_t)::agea::render::gpu_type::g_mat2 +
                                            obj.traits.numeric.matrix.column_count - 2);

        df.alligment = 16;
        break;
    }
    case SpvOpTypeFloat:
    {
        type = agea::render::gpu_type::id::g_float;
        df.alligment = sizeof(float);
        break;
    }
    case SpvOpTypeVector:
    {
        type = (agea::render::gpu_type::id)((uint32_t)::agea::render::gpu_type::g_vec2 +
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
            type = agea::render::gpu_type::id::g_float;
        }
        else
        {
            gpu_dynobj_builder gdb;
            gdb.set_id(AID(obj.type_name));

            for (auto from = obj.members; from < obj.members + obj.member_count; from++)
            {
                convert_spvr_to_dyn_layout(AID(from->struct_member_name), *from, gdb);
            }

            df.sub_field_layout = gdb.finalize();
        }

        break;
    }
    case SpvOpTypeInt:
    {
        AGEA_check(obj.type_flags & SPV_REFLECT_TYPE_FLAG_INT, "Only ints");

        type = obj.traits.numeric.scalar.signedness ? agea::render::gpu_type::id::g_int
                                                    : agea::render::gpu_type::id::g_unsigned;

        df.alligment = sizeof(uint32_t);

        break;
    }
    case SpvOpTypeStruct:
    {
        gpu_dynobj_builder gdb;
        gdb.set_id(AID(obj.type_name));

        for (auto from = obj.members; from < obj.members + obj.member_count; from++)
        {
            convert_spvr_to_dyn_layout(AID(from->struct_member_name), *from, gdb);
        }

        df.sub_field_layout = gdb.finalize();

        break;
    }
    case SpvOpTypeRuntimeArray:
    {
        gpu_dynobj_builder gdb;
        gdb.set_id(AID(obj.type_name));

        for (auto from = obj.members; from < obj.members + obj.member_count; from++)
        {
            convert_spvr_to_dyn_layout(AID(from->struct_member_name), *from, gdb);
        }
        df.items_count = uint64_t(-1);
        df.sub_field_layout = gdb.finalize();
        break;
    }
    default:
        AGEA_never("Unsupported!");
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
    gdb.set_id(AID("interface"));

    for (uint64_t i = 0; i < inputs.size(); ++i)
    {
        auto input = inputs[i];

        if (i != input->location)
        {
            if (input->location == uint32_t(-1))
            {
                break;
            }

            while (gdb.get_layout()->get_fields().size() < (input->location - 1))
            {
                gdb.add_empty();
            }
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
                                                        reflection::shader_reflection& sr)
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
shader_reflection_utils::build_shader_descriptor_sets_reflection(
    SpvReflectShaderModule& spv_reflection, reflection::shader_reflection& sr)
{
    uint32_t count = 0;
    auto result = spvReflectEnumerateDescriptorSets(&spv_reflection, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&spv_reflection, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::sort(sets.begin(), sets.end(),
              [](SpvReflectDescriptorSet* l, SpvReflectDescriptorSet* r)
              { return l->set < r->set; });

    gpu_dynobj_builder gdb;
    gdb.set_id(AID("descriptors"));

    for (uint64_t i = 0; i < sets.size(); ++i)
    {
        auto set_id = AID(std::to_string(sets[i]->set));

        gpu_dynobj_builder set_gdb;

        set_gdb.set_id(AID("descriptor_set"));

        auto input = sets[i];

        std::vector<SpvReflectDescriptorBinding*> bindings(input->bindings,
                                                           input->bindings + input->binding_count);

        std::sort(bindings.begin(), bindings.end(),
                  [](SpvReflectDescriptorBinding* l, SpvReflectDescriptorBinding* r)
                  { return l->binding < r->binding; });

        for (auto b : bindings)
        {
            gpu_dynobj_builder binding_gdb;

            auto binding_id = AID(std::to_string(b->binding));

            binding_gdb.set_id(AID("binding"));

            if (!convert_spvr_to_dyn_layout(AID(b->name), *b->type_description, binding_gdb))
            {
                return false;
            }

            auto ctx = std::make_unique<reflection_context>();
            ctx->descritor_type = (VkDescriptorType)b->descriptor_type;
            ctx->binding_idx = b->binding;
            ctx->set_idx = input->set;
            ctx->stage = spv_reflection.shader_stage;

            ctx->descriptors_count = 1;
            for (uint32_t dim_idx = 0; dim_idx < b->array.dims_count; ++dim_idx)
            {
                ctx->descriptors_count *= b->array.dims[dim_idx];
            }

            set_gdb.add_field<reflection_context>(binding_id, binding_gdb.finalize(), 1,
                                                  std::move(ctx));
        }

        gdb.add_field(set_id, set_gdb.finalize(), 1);
    }

    sr.descriptor_sets = gdb.finalize();

    return true;
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

    return build_shader_push_constants(spvmodule.h, sr);
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

    std::vector<SpvReflectBlockVariable*> pconstants(count);
    result = spvReflectEnumeratePushConstantBlocks(&spv_reflection, &count, pconstants.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    gpu_dynobj_builder gdb;

    if (!convert_spvr_to_dyn_layout(AID("constants"), *pconstants[0]->type_description, gdb))
    {
        return false;
    }

    auto ctx = std::make_unique<push_constant_context>();

    ctx->vk_constants.offset = pconstants[0]->offset;
    ctx->vk_constants.size = pconstants[0]->size;
    ctx->vk_constants.stageFlags = spv_reflection.shader_stage;

    gdb.last_field().context = std::move(ctx);

    sr.constants_layout = gdb.finalize();

    return true;
}

VkDescriptorSetLayoutBinding
shader_reflection_utils::convert_dynobj_to_vk_binding(const utils::dynobj_view<gpu_type>& obj)
{
    VkDescriptorSetLayoutBinding layout_binding;
    auto t = obj.context<reflection_context>();

    auto id = obj.subobj(0).id().str();

    layout_binding.descriptorType = t->descritor_type;

    if (agea::string_utils::starts_with(id, "dyn_"))
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

    layout_binding.binding = t->binding_idx;
    layout_binding.stageFlags = t->stage;
    layout_binding.descriptorCount = t->descriptors_count;
    layout_binding.pImmutableSamplers = nullptr;

    return layout_binding;
}

void
shader_reflection_utils::convert_dynobj_to_layout_data(const utils::dynobj_view<gpu_type>& obj,
                                                       vulkan_descriptor_set_layout_data& layout)
{
    auto binds_count = obj.field_count();

    layout.bindings.resize(binds_count);
    layout.set_number = std::atoi(obj.id().cstr());

    for (uint64_t bind_idx = 0; bind_idx < binds_count; ++bind_idx)
    {
        auto bin_obj = obj.subobj(bind_idx);

        layout.bindings[bind_idx] = convert_dynobj_to_vk_binding(bin_obj);
    }
}

void
shader_reflection_utils::convert_dynobj_to_vk_push_constants(
    const utils::dynobj_view<gpu_type>& obj, VkPushConstantRange& range)
{
    range = obj.context<push_constant_context>()->vk_constants;

    obj.print_to_std();
}

}  // namespace render
}  // namespace agea