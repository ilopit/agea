#pragma once

#include "core/agea_minimal.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"

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
                               std::vector<std::shared_ptr<smart_object>>* local_objcs);

    ~object_constructor_context();

    bool
    propagate_to_io_cache();

    std::string
    get_full_path(const std::string& relative_path) const;

    const std::string&
    get_full_path() const;

    bool
    propagate_to_co_cache();

    bool
    add_obj(std::shared_ptr<smart_object> obj);

    std::string m_full_path;

    cache_set_ref m_global_set;
    cache_set_ref m_local_set;
    std::vector<std::shared_ptr<smart_object>>* m_local_objecs;
};
}  // namespace model
}  // namespace agea