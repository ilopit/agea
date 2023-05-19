#include "core/container.h"

#include "core/objects_mapping.h"

#include "core/object_load_context.h"

namespace agea
{
namespace core
{

container::container(container&&) noexcept = default;

container&
container::operator=(container&&) noexcept = default;

container::container(const utils::id& id)
    : m_occ(std::make_unique<object_load_context>())
    , m_mapping(std::make_shared<core::object_mapping>())
    , m_id(id)
{
}

container::~container()
{
}

void
container::set_load_path(const utils::path& p) const
{
    m_load_path = p;
    m_occ->set_prefix_path(p);
}

utils::path
container::get_relative_path(const utils::path& p) const
{
    return p.relative(m_save_root_path);
}

void
container::init_global_cache_reference(
    cache_set* class_global_set /*= glob::class_objects_cache_set::get()*/,
    cache_set* instance_global_set /*= glob::objects_cache_set::get()*/)
{
    AGEA_check(class_global_set && instance_global_set, "Should NOT be empty!");

    m_proto_global_cs = class_global_set;
    m_instance_global_cs = instance_global_set;

    m_occ->set_proto_global_set(m_proto_global_cs).set_instance_global_set(m_instance_global_cs);
}

void
container::register_in_global_cache(cache_set& local,
                                    cache_set& global,
                                    const utils::id& id,
                                    const char* extra)
{
    for (auto& i : local.objects->get_items())
    {
        auto& obj = *i.second;

        global.map->add_item(obj);
    }

    ALOG_INFO("[PKG:{0}], Registered {2} {1} object", id.cstr(), local.objects->get_size(), extra);
}

void
container::unregister_in_global_cache(cache_set& local,
                                      cache_set& global,
                                      const utils::id& id,
                                      const char* extra)
{
    for (auto& i : local.objects->get_items())
    {
        auto& obj = *i.second;

        global.map->remove_item(obj);
    }

    ALOG_INFO("[PKG:{0}], Unregistered {2} {1} object", id.cstr(), local.objects->get_size(),
              extra);
}

void
container::unload()
{
    m_instance_local_cs.clear();
    m_objects.clear();
    m_mapping->clear();
}

}  // namespace core
}  // namespace agea