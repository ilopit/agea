#pragma once

#include "core/agea_minimal.h"

namespace agea
{
namespace model
{
class smart_object;
class class_objects_cache;
class objects_cache;

class object_constructor_context
{
public:
    object_constructor_context();
    ~object_constructor_context();

    std::shared_ptr<smart_object>
    extract_last();

    bool
    propagate_to_io_cache();

    bool
    propagate_to_co_cache();

    std::vector<std::shared_ptr<smart_object>> temporary_obj_cache;

    std::shared_ptr<class_objects_cache> class_obj_cache;
    std::shared_ptr<objects_cache> instance_obj_cache;

    // TODO, workaround
    std::shared_ptr<smart_object> last_obj;

    int m_class_object_order = 0;
};
}  // namespace model
}  // namespace agea