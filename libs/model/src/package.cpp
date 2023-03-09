#include "model/package.h"

#include "model/caches/empty_objects_cache.h"
#include "model/object_load_context.h"
#include "model/object_constructor.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>

#include <map>
#include <filesystem>

namespace agea
{
namespace model
{

package::package(package&&) noexcept = default;

package&
package::operator=(package&&) noexcept = default;

package::package(const utils::id& id, cache_set* class_global_set, cache_set* instance_global_set)
    : m_occ(std::make_unique<object_load_context>())
    , m_mapping(std::make_shared<model::object_mapping>())
    , m_id(id)
{
    m_occ->set_package(this)
        .set_class_local_set(&m_class_local_set)
        .set_instance_local_set(&m_instance_local_set)
        .set_ownable_cache(&m_objects)
        .set_class_global_set(class_global_set)
        .set_instance_global_set(instance_global_set);
}

package::~package()
{
}

utils::path
package::get_relative_path(const utils::path& p) const
{
    return p.relative(m_save_root_path);
}

smart_object*
package::spawn_class_object_impl(const utils::id& id,
                                 const utils::id& type_id,
                                 smart_object::construct_params& params)
{
    auto proto_obj = m_occ->find_class_obj(type_id);

    if (!proto_obj)
    {
        return nullptr;
    }

    m_occ->set_construction_type(object_load_type::class_obj);
    auto obj = object_constructor::alloc_empty_object(type_id, id, 0, *m_occ);
    if (!obj->META_construct(params))
    {
        return nullptr;
    }
    if (!obj->META_post_construct())
    {
        return nullptr;
    }
    obj->set_state(smart_object_state::constructed);

    m_occ->set_construction_type(object_load_type::mirror_copy);
    obj = object_constructor::alloc_empty_object(type_id, id, 0, *m_occ);
    if (!obj->META_construct(params))
    {
        return nullptr;
    }
    if (!obj->META_post_construct())
    {
        return nullptr;
    }

    obj->set_state(smart_object_state::constructed);

    return obj;
}

}  // namespace model
}  // namespace agea