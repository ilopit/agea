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

class object_constructor_context
{
public:
    enum class construction_type
    {
        nav = 0,
        class_obj,
        instance_obj,
        mirror_obj
    };

    object_constructor_context();

    ~object_constructor_context();

    const utils::path&
    get_full_path() const;

    bool
    make_full_path(const utils::path& relative_path, utils::path& p) const;

    bool
    make_full_path(const utils::id& id, utils::path& p) const;

    bool
    add_obj(std::shared_ptr<smart_object> obj);

    smart_object*
    find_class_obj(const utils::id& id);

    smart_object*
    find_obj(const utils::id& id);

    smart_object*
    find_class_obj(const utils::id& id, architype a_type);

    smart_object*
    find_obj(const utils::id& id, architype a_type);

    object_constructor_context&
    set_prefix_path(const utils::path& v)
    {
        m_path_prefix = v;
        return *this;
    }

    object_constructor_context&
    set_class_global_set(cache_set* v)
    {
        m_class_global_set = v;
        return *this;
    }

    object_constructor_context&
    set_class_local_set(cache_set* v)
    {
        m_class_local_set = v;
        return *this;
    }

    object_constructor_context&
    set_instance_global_set(cache_set* v)
    {
        m_instance_global_set = v;
        return *this;
    }

    object_constructor_context&
    set_instance_local_set(cache_set* v)
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

    object_constructor_context::construction_type
    get_construction_type()
    {
        return m_construction_type;
    }

    void
    set_construction_type(object_constructor_context::construction_type t)
    {
        m_construction_type = t;
    }

    cache_set*
    get_class_global_set() const
    {
        return m_class_global_set;
    }

    cache_set*
    get_class_local_set() const
    {
        return m_class_local_set;
    }

    cache_set*
    get_instance_global_set() const
    {
        return m_instance_global_set;
    }

    cache_set*
    get_instance_local_set() const
    {
        return m_instance_local_set;
    }

private:
    utils::path m_path_prefix;

    cache_set* m_class_global_set = nullptr;
    cache_set* m_class_local_set = nullptr;
    cache_set* m_instance_global_set = nullptr;
    cache_set* m_instance_local_set = nullptr;

    std::unordered_map<utils::id, std::pair<bool, utils::path>> m_object_mapping;

    object_constructor_context::construction_type m_construction_type =
        object_constructor_context::construction_type::nav;
    line_cache<smart_object_ptr>* m_ownable_cache_ptr;
};
}  // namespace model
}  // namespace agea