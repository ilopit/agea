#include "core/proto_registry.h"

#include "core/object_load_context.h"

namespace agea
{

glob::proto_registry::type glob::proto_registry::type::s_instance;

namespace core
{

proto_registry::proto_registry()
    : m_occ(std::make_unique<object_load_context>())
{
    m_occ->set_package(nullptr)
        .set_proto_local_set(&m_proto_local_cs)
        .set_ownable_cache(&m_objects)
        .set_proto_global_set(nullptr);
}

}  // namespace core
}  // namespace agea
