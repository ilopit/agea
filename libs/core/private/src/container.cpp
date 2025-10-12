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
    : m_mapping(std::make_shared<core::object_mapping>())
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
    m_occ->set_construction_type(object_load_type::nav);
}

void
container::set_occ(std::unique_ptr<object_load_context> occ)
{
    m_occ = std::move(std::move(occ));
}

}  // namespace core
}  // namespace agea