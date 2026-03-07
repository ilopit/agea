#pragma once

#include "core/model_minimal.h"

#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"
#include "core/path_resolver.h"

#include <utils/path.h>

namespace kryga
{
namespace core
{
class object_load_context
{
    friend class object_load_context_builder;

public:
    object_load_context();
    ~object_load_context();

    bool
    make_full_path(const utils::path& relative_path, utils::path& p) const;

    bool
    make_full_path(const utils::id& id, utils::path& p) const;

    bool
    add_obj(std::shared_ptr<root::smart_object> obj);

    bool
    remove_obj(const root::smart_object& obj);

    root::smart_object*
    find_proto_obj(const utils::id& id);

    root::smart_object*
    find_obj(const utils::id& id);

    root::smart_object*
    find_proto_obj(const utils::id& id, architype a_type);

    root::smart_object*
    find_obj(const utils::id& id, architype a_type);

    // clang-format off
    object_load_context& set_prefix_path         (const utils::path& v)                     { m_path_resolver.set_prefix_path(v); return *this; }
    object_load_context& set_objects_mapping      (const std::shared_ptr<object_mapping>& v) { m_path_resolver.set_objects_mapping(v); return *this; }

    cache_set*          get_instance_local_set() const  { return m_instance_local_set; }
    package*            get_package() const             { return m_package; }
    const utils::path&  get_prefix_path() const         { return m_path_resolver.get_prefix_path(); }
    level*              get_level() const               { return m_level; }
    object_mapping&     get_objects_mapping() const     { return m_path_resolver.get_objects_mapping(); }

    const path_resolver& get_path_resolver() const      { return m_path_resolver; }

    // clang-format on
    void
    reset_loaded_objects(std::vector<root::smart_object*>& old_object,
                         std::vector<root::smart_object*>& result)
    {
        result = std::move(m_loaded_objects);
        m_loaded_objects = std::move(old_object);
    }

    void
    reset_loaded_objects(std::vector<root::smart_object*>& old_object)
    {
        m_loaded_objects = std::move(old_object);
    }

    std::vector<root::smart_object*>
    reset_loaded_objects()
    {
        return std::move(m_loaded_objects);
    }

    void
    push_object_loaded(root::smart_object* o)
    {
        m_loaded_objects.push_back(o);
    }

private:
    path_resolver m_path_resolver;

    cache_set* m_proto_local_set = nullptr;
    cache_set* m_instance_local_set = nullptr;

    package* m_package = nullptr;
    level* m_level = nullptr;

    std::vector<root::smart_object*> m_loaded_objects;

    line_cache<root::smart_object_ptr>* m_ownable_cache_ptr;
};
}  // namespace core
}  // namespace kryga