#include "model/caches/objects_cache.h"

#include "model/object_constructor.h"
#include "vulkan_render/render_loader.h"

#include "core/fs_locator.h"

namespace agea
{
namespace model
{

std::shared_ptr<smart_object>
objects_cache::get(const std::string& id)
{
    return m_items.at(id);
}

void
objects_cache::insert(std::shared_ptr<smart_object>& obj)
{
    auto& i = m_items[obj->id()];

    AGEA_check(!i, "We shouldn't reinsert items");

    i = obj;
}

}  // namespace model
}  // namespace agea
