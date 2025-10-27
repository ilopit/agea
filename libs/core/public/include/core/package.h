#pragma once

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
    pt_nan = 0,
    pt_static,
    pt_dynamic
};

class package : public container
{
public:
    package(const utils::id& id, package_type t);

    ~package();

    friend class package_manager;

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

    virtual bool
    init();

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
    unregister_in_global_cache();

protected:
    package_state m_state = package_state::unloaded;
    package_type m_type = package_type::pt_nan;

    cache_set m_proto_local_cs;
};

class static_package;

struct package_types_builder
{
    virtual bool
    build(static_package& sp)
    {
        return true;
    }
    virtual bool
    destroy(static_package& sp)
    {
        return true;
    }

    ::agea::reflection::reflection_type*
    add(static_package& sp, ::agea::reflection::reflection_type* rt);
};

struct package_render_types_builder
{
    virtual bool
    build(static_package& sp)
    {
        return true;
    }

    virtual bool
    destroy(static_package& sp)
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
    virtual bool
    destroy(static_package& sp)
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
    virtual bool
    destroy(static_package& sp)
    {
        return true;
    }
};

struct package_types_default_objects_builder
{
    virtual bool
    build(static_package& sp)
    {
        return true;
    }
    virtual bool
    destroy(static_package& sp)
    {
        return true;
    }
};

struct package_render_custom_resource_builder
{
    virtual bool
    build(static_package& sp)
    {
        return true;
    }
    virtual bool
    destroy(static_package& sp)
    {
        return true;
    }
};

class static_package : public package
{
public:
    friend class package_types_builder;

    static_package(const utils::id& id);

    virtual bool
    init() override;

    template <typename T>
    result_code
    create_default_class_obj()
    {
        return object_constructor::create_default_class_obj_impl<T>(*m_occ);
    }

    template <typename T>
    result_code
    destroy_default_class_obj()
    {
        return object_constructor::destroy_default_class_obj_impl<T>(*m_occ);
    }

    template <typename T>
    void
    register_package_extention()
    {
        static_assert(std::is_base_of_v<package_types_builder, T> ||
                          std::is_base_of_v<package_types_custom_loader, T> ||
                          std::is_base_of_v<package_render_types_builder, T> ||
                          std::is_base_of_v<package_types_default_objects_builder, T> ||
                          std::is_base_of_v<package_render_custom_resource_builder, T> ||
                          std::is_base_of_v<package_object_builder, T>,
                      "Unsupported type");

        if constexpr (std::is_base_of_v<package_types_builder, T>)
        {
            m_type_builder = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_types_custom_loader, T>)
        {
            m_types_custom_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_render_types_builder, T>)
        {
            m_render_types_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_render_custom_resource_builder, T>)
        {
            m_render_resources_loader = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_types_default_objects_builder, T>)
        {
            m_default_object_builder = std::make_unique<T>();
        }
        else if constexpr (std::is_base_of_v<package_object_builder, T>)
        {
            m_object_builder = std::make_unique<T>();
        }
    }

    void
    load_types();

    void
    destroy_types();

    void
    load_custom_types();

    void
    destroy_custom_types();

    void
    load_render_types();

    void
    destroy_render_types();

    void
    finalize_relfection();

    void
    create_default_types_objects();

    void
    destroy_default_types_objects();

    void
    load_render_resources();

    void
    destroy_render_resources();

    void
    build_objects();

    const std::vector<reflection::reflection_type*>
    get_reflection_types() const
    {
        return m_rts;
    }

private:
    std::unique_ptr<package_types_builder> m_type_builder;
    std::unique_ptr<package_types_custom_loader> m_types_custom_loader;
    std::unique_ptr<package_render_types_builder> m_render_types_loader;
    std::unique_ptr<package_render_custom_resource_builder> m_render_resources_loader;
    std::unique_ptr<package_types_default_objects_builder> m_default_object_builder;

    std::unique_ptr<package_object_builder> m_object_builder;
    std::vector<reflection::reflection_type*> m_rts;
};

class dynamic_package : public package
{
public:
    dynamic_package(const utils::id& id);

    void
    unload();
};

}  // namespace core

}  // namespace agea