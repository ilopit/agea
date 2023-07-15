#pragma once

#include "core/model_minimal.h"

#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"
#include "core/object_load_type.h"
#include "core/objects_mapping.h"

#include <utils/path.h>

namespace agea
{
namespace core
{
class object_load_context
{
public:
    object_load_context();
    ~object_load_context();

    bool
    make_full_path(const utils::path& relative_path, utils::path& p) const;

    bool
    make_full_path(const utils::id& id, utils::path& p) const;

    bool
    add_obj(std::shared_ptr<root::smart_object> obj, bool add_global);

    root::smart_object*
    find_proto_obj(const utils::id& id);

    root::smart_object*
    find_obj(const utils::id& id);

    root::smart_object*
    find_proto_obj(const utils::id& id, architype a_type);

    root::smart_object*
    find_obj(const utils::id& id, architype a_type);

    // clang-format off
    object_load_context& set_proto_global_set    (cache_set* v)                             { m_proto_global_set = v; return *this; }
    object_load_context& set_proto_local_set     (cache_set* v)                             { m_proto_local_set = v; return *this; }
    object_load_context& set_construction_type   (object_load_type t)                       { m_construction_type = t; return *this; }
    object_load_context& set_global_load_mode    (bool v)                                   { m_is_global_load_mode = v; return *this; }
    object_load_context& set_instance_global_set (cache_set* v)                             { m_instance_global_set = v; return *this; }
    object_load_context& set_instance_local_set  (cache_set* v)                             { m_instance_local_set = v; return *this; }
    object_load_context& set_level               (level* l)                                 { m_level = l; return *this; }
    object_load_context& set_objects_mapping     (const std::shared_ptr<object_mapping>& v) { m_object_mapping = v; return *this; }
    object_load_context& set_ownable_cache       (line_cache<root::smart_object_ptr>* v)    { m_ownable_cache_ptr = v; return *this; }
    object_load_context& set_package             (package* p)                               { m_package = p; return *this; }
    object_load_context& set_prefix_path         (const utils::path& v)                     { m_path_prefix = v; return *this; }

    cache_set*          get_proto_global_set() const    { return m_proto_global_set; }
    cache_set*          get_proto_local_set() const     { return m_proto_local_set; }
    object_load_type    get_construction_type()         { return m_construction_type; }
    bool                get_global_load_mode()          { return m_is_global_load_mode; }
    cache_set*          get_instance_global_set() const { return m_instance_global_set; }
    cache_set*          get_instance_local_set() const  { return m_instance_local_set; }
    package*            get_package() const             { return m_package; }
    const utils::path&  get_prefix_path() const         { return m_path_prefix; }
    level*              get_level() const               { return m_level; }

    // clang-format on
    void
    reset_loaded_objects(std::vector<root::smart_object*>& objs)
    {
        objs = std::move(m_loaded_objects);
        m_loaded_objects.clear();
    }

    void
    reset_loaded_objects()
    {
        m_loaded_objects.clear();
    }

    void
    push_object_loaded(root::smart_object* o)
    {
        m_loaded_objects.push_back(o);
    }

private:
    object_load_type m_construction_type = object_load_type::nav;
    utils::path m_path_prefix;
    bool m_is_global_load_mode = false;

    cache_set* m_proto_global_set = nullptr;
    cache_set* m_proto_local_set = nullptr;
    cache_set* m_instance_global_set = nullptr;
    cache_set* m_instance_local_set = nullptr;

    package* m_package = nullptr;
    level* m_level = nullptr;

    std::vector<root::smart_object*> m_loaded_objects;
    std::shared_ptr<object_mapping> m_object_mapping;

    line_cache<root::smart_object_ptr>* m_ownable_cache_ptr;
};
}  // namespace core
}  // namespace agea