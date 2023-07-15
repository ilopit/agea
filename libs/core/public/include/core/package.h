#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"

#include "core/object_constructor.h"
#include "core/objects_mapping.h"

#include "core/container.h"

namespace agea
{
namespace core
{
enum class package_state
{
    unloaded = 0,
    loaded,
    render_loaded
};

enum class package_type
{
    nan = 0,
    type,
    obj
};

class package : public container
{
public:
    package(const utils::id& id,
            package_type t,
            cache_set* class_global_set = nullptr,
            cache_set* instance_global_set = nullptr);

    ~package();

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

    friend class package_manager;

    package_state
    get_state() const
    {
        return m_state;
    }

    package_type
    get_type() const
    {
        return m_type;
    }

    void
    set_state(package_state v)
    {
        m_state = v;
    }

    template <typename T>
    root::smart_object*
    add_object(const utils::id& id, typename const T::construct_params& p)
    {
        return object_constructor::object_construct(T::AR_TYPE_id(), id, p, *m_occ);
    }

    template <typename T>
    result_code
    register_type()
    {
        return object_constructor::register_package_type<T>(*m_occ);
    }

    void
    init_global_cache_reference(cache_set* class_global_set = glob::proto_objects_cache_set::get(),
                                cache_set* instance_global_set = glob::objects_cache_set::get());

    void
    register_in_global_cache();

    void
    unregister_in_global_cache();

    void
    unload();

private:
    package_state m_state = package_state::unloaded;
    package_type m_type = package_type::nan;

    cache_set m_proto_local_cs;
};

}  // namespace core

}  // namespace agea