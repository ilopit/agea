#include "core/package.h"

#include "core/caches/caches_map.h"

#include "core/object_load_context_builder.h"
#include "core/object_constructor.h"

#include <utils/kryga_log.h>

#include <serialization/serialization.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <core/reflection/reflection_type.h>
#include <map>
#include <stack>

namespace kryga::core
{

package::package(package&&) noexcept = default;

package&
package::operator=(package&&) noexcept = default;

package::package(const utils::id& id)
    : container(id)
{
}

package::~package()
{
    unload();
}

void
package::unregister_in_global_cache()
{
    container::unregister_in_global_cache(m_instance_local_cs,
                                          *glob::glob_state().get_instance_set(), m_id, "instance");
    container::unregister_in_global_cache(m_proto_local_cs, *glob::glob_state().get_class_set(),
                                          m_id, "proto");
}

void
package::unload()
{
    if (m_state == package_state::unloaded)
    {
        return;
    }
    ALOG_INFO("Unload {}", get_id().str());

    destroy_default_types_objects();
    destroy_render_resources();
    destroy_render_types();
    destroy_types();

    container::unload();

    m_mapping->clear();
    m_proto_local_cs.clear();

    m_state = package_state::unloaded;
}

bool
package::init()
{
    m_occ = object_load_context_builder()
                .set_package(this)
                .set_proto_local_set(&m_proto_local_cs)
                .set_ownable_cache(&m_objects)
                .set_instance_local_set(&m_instance_local_cs)
                .build();

    auto vfs_root = vfs_paths::package_root(m_id);
    KRG_check(vfs_paths::is_valid_package_root(vfs_root), "Package must be under data://packages/");
    if (!glob::glob_state().getr_vfs().exists(vfs_root))
    {
        ALOG_ERROR("Package not found: {}", vfs_root.str());
        return false;
    }
    m_vfs_root = vfs_root;

    ALOG_INFO("Loading package [{0}] at [{1}]", m_id.cstr(), vfs_root.str());

    // Load mapping from package manifest
    auto mapping = std::make_shared<object_mapping>();
    if (!mapping->build_object_mapping(vfs_root / "package.acfg"))
    {
        ALOG_LAZY_ERROR;
        return false;
    }
    m_occ->set_vfs_mount(vfs_root).set_objects_mapping(mapping);

    return true;
}

void
package::load_types()
{
    if (m_type_builder)
    {
        m_type_builder->build(*this);
    }
}

void
package::destroy_types()
{
    if (m_type_builder)
    {
        m_type_builder->destroy(*this);
    }
}

void
package::load_custom_types()
{
    if (m_types_custom_loader)
    {
        m_types_custom_loader->load(*this);
    }
}

void
package::destroy_custom_types()
{
    if (m_types_custom_loader)
    {
        m_types_custom_loader->destroy(*this);
    }
}

void
package::load_render_resources()
{
    if (m_render_resources_loader)
    {
        m_render_resources_loader->build(*this);
    }
}
void
package::destroy_render_resources()
{
    if (m_render_resources_loader)
    {
        m_render_resources_loader->destroy(*this);
    }
}

void
package::load_render_types()
{
    if (m_render_types_loader)
    {
        m_render_types_loader->build(*this);
    }
}

void
package::destroy_render_types()
{
    if (m_render_types_loader)
    {
        m_render_types_loader->destroy(*this);
    }
}

void
package::load_dynamic_part()
{
    object_constructor ctor(m_occ.get());
    for (auto& i : m_occ->get_objects_mapping().m_items)
    {
        if (i.second.is_class)
        {
            auto obj = ctor.load_package_obj(i.first);
            if (obj)
            {
                ctor.instantiate_obj(*obj.value(), i.first);
            }
        }
    }
    m_occ->reset_loaded_objects();
}

void
package::finalize_reflection()
{
    for (auto [id, rt] : m_rts)
    {
        std::stack<reflection::reflection_type*> to_handle;

        while (rt)
        {
            to_handle.push(rt);

            if (rt->initialized)
            {
                break;
            }
            rt = rt->parent;
        }

        reflection::property_list to_insert;

        while (!to_handle.empty())
        {
            auto& top = to_handle.top();

            top->m_properties.insert(top->m_properties.end(), to_insert.begin(), to_insert.end());

            to_insert = top->m_properties;
            top->initialized = true;
            top->override();

            to_handle.pop();
        }
    }

    for (auto& [id, rt] : m_rts)
    {
        for (auto& p : rt->m_properties)
        {
            if (p->serializable)
            {
                rt->m_serialization_properties.push_back(p);
            }
            rt->m_editor_properties[p->category].push_back(p);
        }
    }
}

void
package::create_default_types_objects()
{
    object_constructor ctor(m_occ.get());
    for (auto& [id, rt] : m_rts)
    {
        if (rt->type_class == reflection::reflection_type::reflection_type_class::kryga_class &&
            !m_occ->find_proto_obj(id))
        {
            auto result = ctor.load_package_obj(id);
            if (!result)
            {
                ALOG_ERROR("Failed to create default type object for {}", id.str());
            }
        }
    }
}

void
package::destroy_default_types_objects()
{
    if (m_default_object_builder)
    {
        m_default_object_builder->destroy(*this);
    }
}

void
package::build_objects()
{
    if (m_object_builder)
    {
        m_object_builder->build(*this);
    }
}

::kryga::reflection::reflection_type*
package_types_builder::add(package& sp, ::kryga::reflection::reflection_type* rt)
{
    sp.m_rts[rt->type_name] = rt;
    return rt;
}

}  // namespace kryga::core
