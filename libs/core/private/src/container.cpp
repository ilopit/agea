#include "core/container.h"

#include <packages/root/model/smart_object.h>

#include <utils/check.h>

namespace kryga
{
namespace core
{

container::container(container&&) noexcept = default;

container&
container::operator=(container&&) noexcept = default;

container::container(const utils::id& id)
    : m_id(id)
{
}

container::~container() = default;

void
container::set_vfs_root(const vfs::rid& r)
{
    m_vfs_root = r;
    if (m_occ)
    {
        m_occ->set_vfs_mount(r);
    }
}

utils::path
container::get_relative_path(const utils::path& p) const
{
    return p.relative(m_save_root_path);
}

void
container::unregister_in_global_cache(cache_set& local,
                                      cache_set& global,
                                      const utils::id& id,
                                      const char* extra)
{
    for (auto& i : local.objects.get_items())
    {
        auto& obj = *i.second;

        global.map.remove_item(obj);
    }

    ALOG_INFO("[PKG:{0}], Unregistered {2} {1} object", id.cstr(), local.objects.get_size(), extra);
}

void
container::unload(bool is_package)
{
    m_local_cs.clear();

    // Drop owned objects, but enforce the container's domain invariant. Reverse
    // iteration so swap_and_remove keeps the unvisited prefix valid.
    for (size_t i = m_objects.size(); i-- > 0;)
    {
        const bool instance_obj = m_objects[i]->get_flags().instance_obj;

        if (is_package)
        {
            // Package teardown: a package must hold only package objects.
            KRG_check(!instance_obj, "instance object found in package during unload");
        }
        else if (!instance_obj)
        {
            // Level teardown: a package object (readonly CDO) may have leaked in
            // when first loaded mid-play via the level's load context (see
            // docs/issues/object-lifecycle.md). It is not ours to free — leave it
            // owned and keep iterating the rest.
            continue;
        }

        m_objects.swap_and_remove(m_objects.begin() + i);
    }
}

void
container::set_occ(std::unique_ptr<object_load_context> occ)
{
    m_occ = std::move(occ);
}

}  // namespace core
}  // namespace kryga
