#pragma once

#include "model/model_minimal.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"
#include "utils/path.h"

namespace agea
{
namespace model
{

enum obj_construction_type
{
    obj_construction_type__nav = 0,
    obj_construction_type__class,
    obj_construction_type__instance
};

class object_constructor_context
{
public:
    object_constructor_context();

    object_constructor_context(const utils::path& prefix_path,
                               cache_set_ref class_global_map,
                               cache_set_ref class_local_map,
                               cache_set_ref instance_global_map,
                               cache_set_ref instance_local_map,
                               line_cache* ownable_cache);

    ~object_constructor_context();

    utils::path
    get_full_path(const utils::path& relative_path) const;

    const utils::path&
    get_full_path() const;

    bool
    add_obj(std::shared_ptr<smart_object> obj);

    smart_object*
    find_class_obj(const utils::id& id);

    utils::path m_path_prefix;

    obj_construction_type m_construction_type = obj_construction_type::obj_construction_type__nav;

    cache_set_ref m_class_global_set;
    cache_set_ref m_class_local_set;
    cache_set_ref m_instance_global_set;
    cache_set_ref m_instance_local_set;

    line_cache* m_ownable_cache_ptr;
};
}  // namespace model
}  // namespace agea