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

    ~object_constructor_context();

    utils::path
    get_full_path(const utils::path& relative_path) const;

    const utils::path&
    get_full_path() const;

    utils::path
    get_full_path(const utils::id& id) const;

    bool
    add_obj(std::shared_ptr<smart_object> obj);

    smart_object*
    find_class_obj(const utils::id& id);

    smart_object*
    find_class_obj(const utils::id& id, architype a_type);

    object_constructor_context&
    set_prefix_path(const utils::path& v)
    {
        m_path_prefix = v;
        return *this;
    }

    object_constructor_context&
    set_class_global_set(cache_set_ref v)
    {
        m_class_global_set = v;
        return *this;
    }

    object_constructor_context&
    set_class_local_set(cache_set_ref v)
    {
        m_class_local_set = v;
        return *this;
    }

    object_constructor_context&
    set_instance_global_set(cache_set_ref v)
    {
        m_instance_global_set = v;
        return *this;
    }

    object_constructor_context&
    set_instance_local_set(cache_set_ref v)
    {
        m_instance_local_set = v;
        return *this;
    }

    object_constructor_context&
    set_objects_mapping(std::unordered_map<utils::id, std::pair<bool, utils::path>> v)
    {
        m_object_mapping = v;
        return *this;
    }

    object_constructor_context&
    set_ownable_cache(line_cache<smart_object_ptr>* v)
    {
        m_ownable_cache_ptr = v;
        return *this;
    }

    utils::path m_path_prefix;

    cache_set_ref m_class_global_set;
    cache_set_ref m_class_local_set;
    cache_set_ref m_instance_global_set;
    cache_set_ref m_instance_local_set;

    std::unordered_map<utils::id, std::pair<bool, utils::path>> m_object_mapping;

    obj_construction_type m_construction_type = obj_construction_type::obj_construction_type__nav;
    line_cache<smart_object_ptr>* m_ownable_cache_ptr;
};
}  // namespace model
}  // namespace agea