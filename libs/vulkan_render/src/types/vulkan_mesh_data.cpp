#include "vulkan_render/types/vulkan_mesh_data.h"

#include "vulkan_render/utils/vulkan_converters.h"

#include <utils/dynamic_object_builder.h>

namespace agea
{
namespace render
{

vertex_input_description
convert_to_vertex_input_description(agea::utils::dynamic_object_layout& dol)
{
    render::vertex_input_description description{};

    VkVertexInputBindingDescription main_binding = {};
    main_binding.binding = 0;
    main_binding.stride = dol.get_object_size();
    main_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    description.bindings.push_back(main_binding);

    VkVertexInputAttributeDescription att = {};
    att.binding = 0;
    att.location = 0;

    for (auto& f : dol.get_fields())
    {
        auto vk_format = vk_utils::convert_to_vk_format((utils::agea_type::id)f.type);

        AGEA_check(vk_format != VK_FORMAT_UNDEFINED, "Should never happens");

        att.format = vk_format;
        att.offset = f.offset;

        description.attributes.push_back(att);
        att.location++;
    }

    return description;
}

mesh_data::~mesh_data()
{
}

}  // namespace render
}  // namespace agea
