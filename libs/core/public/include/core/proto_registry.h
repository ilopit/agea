#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include "core/object_constructor.h"
#include "core/caches/caches_map.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"

#include <packages/root/smart_object.h>

namespace agea
{
namespace core
{

class object_load_context;

class proto_registry
{
public:
    proto_registry();

    template <typename T>
    result_code
    register_type()
    {
        return object_constructor::register_package_type<T>(*m_occ);
    }

    template <typename T>
    root::smart_object*
    register_type_variant(const utils::id& id, typename const T::construct_params& p)
    {
        return object_constructor::object_construct(T::AR_TYPE_id(), id, p, *m_occ);
    }

private:
    std::unique_ptr<object_load_context> m_occ;
    line_cache<root::smart_object_ptr> m_objects;
    cache_set m_proto_local_cs;
};

}  // namespace core

namespace glob
{
struct proto_registry : public singleton_instance<::agea::core::proto_registry, proto_registry>
{
};

}  // namespace glob
}  // namespace agea