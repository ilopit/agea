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

class object_load_context;

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
    set_load_path(const utils::path& p);

    const utils::path&
    get_save_path() const
    {
        return m_save_root_path;
    }

    void
    set_save_root_path(const utils::path& p)
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

    static void
    unregister_in_global_cache(cache_set& local,
                               cache_set& global,
                               const utils::id& id,
                               const char* extra);

    void
    unload();

    const line_cache<std::shared_ptr<root::smart_object>>
    get_objects() const
    {
        return m_objects;
    }

    void
    set_occ(std::unique_ptr<object_load_context> occ);

protected:
    utils::id m_id;
    utils::path m_load_path;
    utils::path m_save_root_path;

    cache_set m_instance_local_cs;

    line_cache<std::shared_ptr<root::smart_object>> m_objects;
    std::shared_ptr<object_mapping> m_mapping;
    std::unique_ptr<object_load_context> m_occ;
};

}  // namespace core

}  // namespace agea