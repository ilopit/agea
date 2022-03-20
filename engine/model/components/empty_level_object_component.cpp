#include "empty_level_object_component.h"

#include "vulkan_render/vulkan_render_data.h"

namespace agea
{
namespace model
{
bool
empty_level_object_component::prepare_for_rendering()
{
    AGEA_return_nok(base_class::prepare_for_rendering());

    m_render_object->visible = false;

    return true;
}

}  // namespace model
}  // namespace agea
