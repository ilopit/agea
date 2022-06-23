#include "model/caches/class_object_cache.h"

#include "utils/agea_log.h"
#include "model/smart_object.h"

namespace agea
{
namespace model
{

smart_object*
class_objects_cache::get(const core::id& class_id)
{
    auto itr = m_objects.find(class_id);

    return itr != m_objects.end() ? itr->second.obj.get() : nullptr;
}

void
class_objects_cache::insert(std::shared_ptr<smart_object> obj, const std::string& path, int order)
{
    auto class_id = obj->get_id();
    ALOG_INFO("CO Cache: Inserted [{0}::{1}] into cache", obj->get_type_id().cstr(),
              class_id.cstr());
    AGEA_check(m_objects.end() == m_objects.find(class_id), "Should not be overriden!");

    class_object_context ctx{obj, path, order};

    m_objects.insert({class_id, ctx});
}

void
class_objects_cache::insert(std::shared_ptr<smart_object> obj)
{
    insert(std::move(obj), "", -1);
}

bool
class_objects_cache::exists(const core::id& class_id)
{
    return m_objects.find(class_id) != m_objects.end();
}

std::vector<class_objects_cache::class_object_context>
class_objects_cache::get_order()
{
    std::vector<class_objects_cache::class_object_context> result;

    for (auto& o : m_objects)
    {
        if (o.second.order != -1)
        {
            result.push_back(o.second);
        }
    }

    return result;
}

}  // namespace model
}  // namespace agea
