#include "core/package.h"

#include "core/caches/caches_map.h"

#include "core/object_load_context.h"
#include "core/object_constructor.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>
#include <core/global_state.h>

#include <map>
#include <filesystem>

namespace agea::core
{

package::package(package&&) noexcept = default;

package&
package::operator=(package&&) noexcept = default;

void
package::init()
{
    m_occ = std::make_unique<object_load_context>();
    m_occ->set_package(this)
        .set_proto_local_set(&m_proto_local_cs)
        .set_ownable_cache(&m_objects)
        .set_proto_global_set(glob::state::getr().get_class_set())
        .set_instance_global_set(glob::state::getr().get_instance_set())
        .set_instance_local_set(&m_instance_local_cs);
}

package::package(const utils::id& id, package_type t)
    : container(id)
    , m_type(t)
{
}

package::~package()
{
}

void
package::register_in_global_cache()
{
    container::register_in_global_cache(m_instance_local_cs,
                                        *glob::state::getr().get_instance_set(), m_id, "instance");
    container::register_in_global_cache(m_proto_local_cs, *glob::state::getr().get_class_set(),
                                        m_id, "proto");

    m_occ->set_global_load_mode(true);
}

void
package::unregister_in_global_cache()
{
    container::unregister_in_global_cache(
        m_instance_local_cs, *glob::state::getr().get_instance_set(), m_id, "instance");
    container::unregister_in_global_cache(m_proto_local_cs, *glob::state::getr().get_class_set(),
                                          m_id, "proto");

    m_occ->set_global_load_mode(false);
}

dynamic_package::dynamic_package(const utils::id& id)
    : package(id, package_type::pt_dynamic)
{
    set_occ(std::make_unique<object_load_context>());
}

void
dynamic_package::unload()
{
    container::unload();

    m_mapping->clear();
    m_proto_local_cs.clear();

    m_state = package_state::unloaded;
}

static_package::static_package(const utils::id& id)
    : package(id, package_type::pt_static)
{
}

void
static_package::load_types()
{
    if (m_type_builder)
    {
        m_type_builder->build(*this);
    }
}

void
static_package::load_custom_types()
{
    if (m_types_custom_loader)
    {
        m_types_custom_loader->load(*this);
    }
}

void
static_package::load_render_resources()
{
    if (m_render_resources_loader)
    {
        m_render_resources_loader->build(*this);
    }
}

void
static_package::load_render_types()
{
    if (m_render_types_loader)
    {
        m_render_types_loader->build(*this);
    }
}

void
static_package::register_types()
{
    if (m_type_register)
    {
        m_type_register->build(*this);
    }
}

void
static_package::build_objects()
{
    if (m_object_builder)
    {
        m_object_builder->build(*this);
    }
}

}  // namespace agea::core
