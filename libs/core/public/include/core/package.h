#pragma once

#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"

#include "core/object_constructor.h"
#include "core/objects_mapping.h"

#include "core/container.h"

#define AGEA_ar_package_types_loader                                       \
    class package_types_loader : public ::agea::core::package_types_loader \
    {                                                                      \
    public:                                                                \
        virtual bool                                                       \
        load(::agea::core::static_package& sp) override;                   \
    };

#define AGEA_ar_package_custom_types_loader                                               \
    struct package_types_custom_loader : public ::agea::core::package_types_custom_loader \
    {                                                                                     \
        virtual bool                                                                      \
        load(static_package& sp) override;                                                \
    };

#define AGEA_ar_package_render_types_loader                                               \
    struct package_render_types_loader : public ::agea::core::package_render_types_loader \
    {                                                                                     \
        virtual bool                                                                      \
        load(static_package& sp) override;                                                \
    };

#define AGEA_ar_package_render_resources_loader                                                   \
    struct package_render_resources_loader : public ::agea::core::package_render_resources_loader \
    {                                                                                             \
        virtual bool                                                                              \
        load(static_package& sp) override;                                                        \
    };

#define AGEA_ar_package_object_builder                                          \
    struct package_object_builder : public ::agea::core::package_object_builder \
    {                                                                           \
        virtual bool                                                            \
        build(static_package& sp) override;                                     \
    };

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
    pt_nan = 0,
    pt_static,
    pt_dynamic
};

class package : public container
{
public:
    package(const utils::id& id,
            package_type t,
            cache_set* class_global_set = nullptr,
            cache_set* instance_global_set = nullptr);

    ~package();

    friend class package_manager;

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

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

    void
    init_global_cache_reference(cache_set* class_global_set = glob::proto_objects_cache_set::get(),
                                cache_set* instance_global_set = glob::objects_cache_set::get());

    void
    register_in_global_cache();

    void
    unregister_in_global_cache();

protected:
    package_state m_state = package_state::unloaded;
    package_type m_type = package_type::pt_nan;

    cache_set m_proto_local_cs;
};

class static_package;

struct package_types_loader
{
    virtual bool
    load(static_package& sp)
    {
        return true;
    }
};

struct package_render_types_loader
{
    virtual bool
    load(static_package& sp)
    {
        return true;
    }
};

struct package_types_custom_loader
{
    virtual bool
    load(static_package& sp)
    {
        return true;
    }
};

struct package_object_builder
{
    virtual bool
    build(static_package& sp)
    {
        return true;
    }
};

struct package_render_resources_loader
{
    virtual bool
    load(static_package& sp)
    {
        return true;
    }
};

class static_package : public package
{
public:
    static_package(const utils::id& id);

    template <typename T>
    result_code
    register_type()
    {
        return object_constructor::register_package_type<T>(*m_occ);
    }

    template <typename T>
    root::smart_object*
    add_object(const utils::id& id, typename const T::construct_params& p)
    {
        return object_constructor::object_construct(T::AR_TYPE_id(), id, p, *m_occ);
    }

    void
    finilize_objects();

    template <typename T>
    void
    register_package_extention()
    {
        static_assert(std::is_base_of_v<package_types_loader, T> ||
                          std::is_base_of_v<package_types_custom_loader, T> ||
                          std::is_base_of_v<package_render_types_loader, T> ||
                          std::is_base_of_v<package_render_resources_loader, T> ||
                          std::is_base_of_v<package_object_builder, T>,
                      "Unsupported type");

        if constexpr (std::is_base_of_v<package_types_loader, T>)
        {
            m_type_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_types_custom_loader, T>)
        {
            m_types_custom_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_render_types_loader, T>)
        {
            m_render_types_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_render_resources_loader, T>)
        {
            m_render_resources_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_object_builder, T>)
        {
            m_object_builder = std::make_unique<T>();
        }
    }

    void
    load_types();

    void
    load_custom_types();

    void
    load_render_types();

    void
    load_render_resources();

    void
    build_objects();

    std::unique_ptr<package_types_loader> m_type_loader;
    std::unique_ptr<package_types_custom_loader> m_types_custom_loader;

    std::unique_ptr<package_render_types_loader> m_render_types_loader;
    std::unique_ptr<package_render_resources_loader> m_render_resources_loader;

    std::unique_ptr<package_object_builder> m_object_builder;
};

class dynamic_package : public package
{
public:
    dynamic_package(const utils::id& id,
                    cache_set* class_global_set,
                    cache_set* instance_global_set);

    void
    unload();
};

}  // namespace core

}  // namespace agea