#include "core/package.h"

#include "core/caches/caches_map.h"

#include "core/object_load_context.h"
#include "core/object_constructor.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>
#include <global_state/global_state.h>
#include <core/reflection/reflection_type.h>
#include <map>
#include <stack>
#include <filesystem>

namespace agea::core
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
    container::unload();

    m_mapping->clear();
    m_proto_local_cs.clear();

    m_state = package_state::unloaded;
}

bool
package::init()
{
    m_occ = std::make_unique<object_load_context>();
    m_occ->set_package(this)
        .set_proto_local_set(&m_proto_local_cs)
        .set_ownable_cache(&m_objects)
        .set_instance_local_set(&m_instance_local_cs);

    m_occ->push_construction_type(object_load_type::class_obj);

    auto path = glob::glob_state().get_resource_locator()->resource(category::packages,
                                                                    m_id.str() + ".apkg");

    ALOG_INFO("Loading package [{0}] at path [{1}]", m_id.cstr(), path.str());

    std::string name, extension;
    path.parse_file_name_and_ext(name, extension);

    if (name.empty() || extension.empty() || extension != "apkg")
    {
        ALOG_ERROR("Loading package failed, {0} {1}", name, extension);
        return false;
    }

    auto mapping = std::make_shared<object_mapping>();
    if (!mapping->buiild_object_mapping(path / "package.acfg"))
    {
        ALOG_LAZY_ERROR;
        return false;
    }
    m_occ->set_prefix_path(path).set_objects_mapping(mapping);

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
package::finalize_relfection()
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
                rt->m_serilalization_properties.push_back(p);
            }
            rt->m_editor_properties[p->category].push_back(p);
        }
    }
}

void
package::create_default_types_objects()
{
    for (auto [id, rt] : m_rts)
    {
        if (rt->type_class == reflection::reflection_type::reflection_type_class::agea_class)
        {
            auto result = object_constructor::create_default_default_class_proto(id, *m_occ);
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

::agea::reflection::reflection_type*
package_types_builder::add(package& sp, ::agea::reflection::reflection_type* rt)
{
    sp.m_rts[rt->type_name] = rt;
    return rt;
}

}  // namespace agea::core
