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

struct package_types_builder
{
    virtual ~package_types_builder()
    {
    }

    virtual bool
    build(package& sp)
    {
        return true;
    }
    virtual bool
    destroy(package& sp)
    {
        return true;
    }

    ::agea::reflection::reflection_type*
    add(package& sp, ::agea::reflection::reflection_type* rt);
};

struct package_render_types_builder
{
    virtual ~package_render_types_builder()
    {
    }

    virtual bool
    build(package& sp)
    {
        return true;
    }

    virtual bool
    destroy(package& sp)
    {
        return true;
    }
};

struct package_types_custom_loader
{
    virtual ~package_types_custom_loader()
    {
    }

    virtual bool
    load(package& sp)
    {
        return true;
    }
    virtual bool
    destroy(package& sp)
    {
        return true;
    }
};

struct package_object_builder
{
    virtual ~package_object_builder()
    {
    }

    virtual bool
    build(package& sp)
    {
        return true;
    }
    virtual bool
    destroy(package& sp)
    {
        return true;
    }
};

struct package_types_default_objects_builder
{
    virtual ~package_types_default_objects_builder()
    {
    }

    virtual bool
    build(package& sp)
    {
        return true;
    }
    virtual bool
    destroy(package& sp)
    {
        return true;
    }
};

struct package_render_custom_resource_builder
{
    virtual ~package_render_custom_resource_builder()
    {
    }

    virtual bool
    build(package& sp)
    {
        return true;
    }
    virtual bool
    destroy(package& sp)
    {
        return true;
    }
};

class package : public container
{
public:
    package(const utils::id& id);

    ~package();

    friend class package_manager;

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

    virtual bool
    init();

    void
    unload();

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

    cache_set&
    get_proto_local_cs()
    {
        return m_proto_local_cs;
    }

    friend class package_types_builder;

    template <typename T>
    result_code
    destroy_default_class_obj()
    {
        return object_constructor::destroy_default_class_obj_impl<T>(*m_occ);
    }

    template <typename T>
    void
    register_package_extension()
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

    // clang-format off
    void load_types();
    void destroy_types();
    
    std::unique_ptr<package_types_builder>& 
    types_builder()
    {
        return m_type_builder;
    }

    void load_custom_types();
    void destroy_custom_types();

    void load_render_types();
    void destroy_render_types();

    void finalize_reflection();
    void create_default_types_objects();
    void destroy_default_types_objects();
    void load_render_resources();
    void destroy_render_resources();

    void build_objects();

    const std::unordered_map<utils::id, reflection::reflection_type*>&
    get_reflection_types() const
    {
        return m_rts;
    }

    void
    complete_load()
    {
        init();
        load_types();
        load_render_types();
        finalize_reflection();
        load_render_resources();
        create_default_types_objects();
       m_state = package_state::loaded;
    }

    // clang-format on

private:
    package_state m_state = package_state::unloaded;
    package_type m_type = package_type::pt_nan;

    cache_set m_proto_local_cs;

    std::unique_ptr<package_types_builder> m_type_builder;
    std::unique_ptr<package_types_custom_loader> m_types_custom_loader;
    std::unique_ptr<package_render_types_builder> m_render_types_loader;
    std::unique_ptr<package_render_custom_resource_builder> m_render_resources_loader;
    std::unique_ptr<package_types_default_objects_builder> m_default_object_builder;

    std::unique_ptr<package_object_builder> m_object_builder;
    std::unordered_map<utils::id, reflection::reflection_type*> m_rts;
};

}  // namespace core
}  // namespace agea