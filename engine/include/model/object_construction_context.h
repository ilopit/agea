#pragma once

#include "core/agea_minimal.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"

namespace agea
{
namespace model
{

class object_constructor_context
{
public:
    object_constructor_context();

    object_constructor_context(cache_set_ref global_map,
                               cache_set_ref local_map,
                               line_cache* ownable_cache);

    ~object_constructor_context();

    bool
    propagate_to_io_cache();

    utils::path
    get_full_path(const utils::path& relative_path) const;

    const utils::path&
    get_full_path() const;

    bool
    propagate_to_co_cache();

    bool
    add_obj(std::shared_ptr<smart_object> obj);

    utils::path m_path_prefix;

    cache_set_ref m_global_set;
    cache_set_ref m_local_set;
    line_cache* m_ownable_cache_ptr;
};
}  // namespace model
}  // namespace agea