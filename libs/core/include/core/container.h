#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"

#include "core/object_constructor.h"
#include "core/objects_mapping.h"

#include <utils/id.h>

namespace agea
{
namespace core
{

class container
{
public:
    container(const utils::id& id);
    ~container();

    container(container&&) noexcept;
    container&
    operator=(container&&) noexcept;

    friend class package_manager;

    const utils::id&
    get_id() const
    {
        return m_id;
    }

    const utils::path&
    get_load_path() const
    {
        return m_load_path;
    }

    void
    set_load_path(const utils::path& p) const;

    const utils::path&
    get_save_path() const
    {
        return m_save_root_path;
    }

    void
    set_save_root_path(const utils::path& p) const
    {
        m_save_root_path = p;
    }

    utils::path
    get_relative_path(const utils::path& p) const;

    cache_set&
    get_local_cache()
    {
        return m_instance_local_cs;
    }

    object_load_context&
    get_load_context() const
    {
        return *m_occ.get();
    }

    void
    init_global_cache_reference(cache_set* class_global_set = glob::proto_objects_cache_set::get(),
                                cache_set* instance_global_set = glob::objects_cache_set::get());

    static void
    register_in_global_cache(cache_set& local,
                             cache_set& global,
                             const utils::id& id,
                             const char* extra);

    static void
    unregister_in_global_cache(cache_set& local,
                               cache_set& global,
                               const utils::id& id,
                               const char* extra);

    void
    unload();

protected:
    utils::id m_id;
    mutable utils::path m_load_path;
    mutable utils::path m_save_root_path;

    cache_set* m_proto_global_cs = nullptr;
    cache_set* m_instance_global_cs = nullptr;

    cache_set m_instance_local_cs;

    line_cache<std::shared_ptr<root::smart_object>> m_objects;
    std::shared_ptr<object_mapping> m_mapping;
    std::unique_ptr<object_load_context> m_occ;
};

}  // namespace core

}  // namespace agea