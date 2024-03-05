#include "core/package.h"

#include "core/caches/objects_cache.h"
#include "core/caches/caches_map.h"

#include "core/object_load_context.h"
#include "core/object_constructor.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>

#include <map>
#include <filesystem>

namespace agea::core
{

package::package(package&&) noexcept = default;

package&
package::operator=(package&&) noexcept = default;

package::package(const utils::id& id,
                 package_type t,
                 cache_set* proto_global_set,
                 cache_set* instance_global_set)
    : container(id)
    , m_type(t)
{
    m_occ->set_package(this)
        .set_proto_local_set(&m_proto_local_cs)
        .set_ownable_cache(&m_objects)
        .set_proto_global_set(proto_global_set)
        .set_instance_global_set(instance_global_set)
        .set_instance_local_set(&m_instance_local_cs);
}

package::~package()
{
}

void
package::init_global_cache_reference(
    cache_set* proto_global_set /*= glob::class_objects_cache_set::get()*/,
    cache_set* instance_global_set /*= glob::objects_cache_set::get()*/)
{
    AGEA_check(!(m_proto_global_cs || m_instance_global_cs), "Should be empty!");
    AGEA_check(proto_global_set && instance_global_set, "Should NOT be empty!");

    m_proto_global_cs = proto_global_set;
    m_instance_global_cs = instance_global_set;

    m_occ->set_proto_global_set(m_proto_global_cs).set_instance_global_set(m_instance_global_cs);
}

void
package::register_in_global_cache()
{
    container::register_in_global_cache(m_instance_local_cs, *m_instance_global_cs, m_id,
                                        "instance");
    container::register_in_global_cache(m_proto_local_cs, *m_proto_global_cs, m_id, "proto");

    m_occ->set_global_load_mode(true);
}

void
package::unregister_in_global_cache()
{
    container::unregister_in_global_cache(m_instance_local_cs, *m_instance_global_cs, m_id,
                                          "instance");
    container::unregister_in_global_cache(m_proto_local_cs, *m_proto_global_cs, m_id, "proto");

    m_occ->set_global_load_mode(false);
}

dynamic_package::dynamic_package(const utils::id& id,
                                 cache_set* class_global_set,
                                 cache_set* instance_global_set)
    : package(id, package_type::pt_dynamic, class_global_set, instance_global_set)
{
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
    : package(id, package_type::pt_static, nullptr, nullptr)
{
}

void
static_package::finilize_objects()
{
}

void
static_package::load_types()
{
    m_type_loader->load(*this);
}

void
static_package::load_custom_types()
{
    m_types_custom_loader->load(*this);
}

void
static_package::build_render_objects()
{
    m_render_data_loader->load(*this);
}

void
static_package::build_model_objects()
{
    m_object_builder->build(*this);
}

}  // namespace agea::core
