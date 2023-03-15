#pragma once

#include "model/model_minimal.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"
#include "model/object_constructor.h"
#include "model/objects_mapping.h"

namespace agea
{
namespace model
{
enum class package_state
{
    unloaded = 0,
    loaded,
    render_loaded
};

class package
{
public:
    package(const utils::id& id,
            cache_set* class_global_set = nullptr,
            cache_set* instance_global_set = nullptr);

    ~package();

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

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

    const utils::path&
    get_save_path() const
    {
        return m_save_root_path;
    }

    utils::path
    get_relative_path(const utils::path& p) const;

    void
    set_save_root_path(const utils::path& path) const
    {
        m_save_root_path = path;
    }

    void
    set_load_path(const utils::path& path) const
    {
        m_load_path = path;
    }

    cache_set&
    get_class_cache()
    {
        return m_class_local_set;
    }

    cache_set&
    get_cache()
    {
        return m_instance_local_set;
    }

    package_state
    get_state() const
    {
        return m_state;
    }

    void
    set_state(package_state v)
    {
        m_state = v;
    }

    line_cache<smart_object*>&
    get_objects()
    {
        return m_package_instances;
    }

    object_load_context&
    get_load_context()
    {
        return *m_occ.get();
    }

    template <typename T>
    smart_object*
    spawn_object(const utils::id& id, typename const T::construct_params& p)
    {
        return object_constructor::construct_package_object(T::META_type_id(), id, p, *m_occ);
    }

    template <typename T>
    result_code
    register_type()
    {
        return object_constructor::register_package_type<T>(*m_occ);
    }

    void
    init_global_cache_reference(cache_set* class_global_set = glob::class_objects_cache_set::get(),
                                cache_set* instance_global_set = glob::objects_cache_set::get());

    void
    register_in_global_cache();

private:
    utils::id m_id;
    mutable utils::path m_load_path;
    mutable utils::path m_save_root_path;

    package_state m_state = package_state::unloaded;

    cache_set* m_class_global_set = nullptr;
    cache_set* m_instance_global_set = nullptr;

    cache_set m_class_local_set;
    cache_set m_instance_local_set;

    line_cache<std::shared_ptr<smart_object>> m_objects;
    line_cache<smart_object*> m_package_instances;
    std::shared_ptr<object_mapping> m_mapping;
    std::unique_ptr<object_load_context> m_occ;
};

}  // namespace model

}  // namespace agea