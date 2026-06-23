#include "core/object_load_context.h"

#include <packages/root/model/smart_object.h>

#include "core/model_system.h"
#include "core/package.h"
#include "global_state/global_state.h"

#include <vfs/vfs.h>

#include "utils/kryga_log.h"
#include "utils/check.h"

namespace kryga
{
namespace core
{

object_load_context::object_load_context()

{
}

object_load_context::~object_load_context() = default;

bool
object_load_context::resolve(const utils::id& id, vfs::rid& out) const
{
    if (m_vfs_root.empty())
    {
        return false;
    }

    auto& vfs = glob::glob_state().getr_vfs();
    auto result = vfs.find_object(m_vfs_root, id.str());
    if (!result)
    {
        return false;
    }

    out = *result;
    return true;
}

bool
object_load_context::resolve(const utils::path& relative, vfs::rid& out) const
{
    if (m_vfs_root.empty())
    {
        return false;
    }

    out = m_vfs_root / relative.str();
    return true;
}

bool
object_load_context::add_obj(std::shared_ptr<root::smart_object> obj)
{
    KRG_check(m_ownable_cache_ptr, "Ownable cache must be set!");
    KRG_check(m_local_set, "Local set must be set!");
    KRG_check(obj, "Object must exist!");

    auto& obj_ref = *obj;

    // TODO(object-lifecycle): class objects (default_obj/readonly CDOs) go into
    // the container's ownable cache here just like instances. When a CDO is first
    // loaded mid-play via the level's context it lands in the level's m_objects
    // and level::rollback() sweeps it (forcing default_obj skips in rollback and
    // render_cmd_destroy). Route class objects to the shared/global domain only.
    // See docs/issues/object-lifecycle.md.
    m_ownable_cache_ptr->emplace_back(std::move(obj));

    m_local_set->map.add_item(obj_ref);
    glob::glob_state().getr_model().caches.map.add_item(obj_ref);

    return true;
}

bool
object_load_context::remove_obj(const root::smart_object& obj)
{
    if (m_local_set)
    {
        m_local_set->map.remove_item(obj);
    }
    glob::glob_state().getr_model().caches.map.remove_item(obj);

    return true;
}

root::smart_object*
object_load_context::find_obj(const utils::id& id)
{
    auto* obj = m_local_set ? m_local_set->objects.get_item(id) : nullptr;

    if (!obj)
    {
        obj = glob::glob_state().getr_model().caches.objects.get_item(id);
    }

    return obj;
}

root::smart_object*
object_load_context::find_obj(const utils::id& id, architype a_type)
{
    root::smart_object* obj = nullptr;

    if (m_local_set)
    {
        auto* c = m_local_set->map.get_cache(a_type);
        if (c)
        {
            obj = c->get_item(id);
        }
    }

    if (!obj)
    {
        obj = glob::glob_state().getr_model().caches.objects.get_item(id);
    }

    return obj;
}

}  // namespace core
}  // namespace kryga
