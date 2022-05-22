#pragma once

#include "core/agea_minimal.h"

namespace agea
{
namespace model
{
class smart_object;
class class_objects_cache;
class objects_cache;

struct object_constructor_context
{
    object_constructor_context();
    ~object_constructor_context();

    std::shared_ptr<smart_object>
    extract_last();

    bool
    propagate_to_obj_cache();

    std::vector<std::shared_ptr<smart_object>> temporary_cache;

    std::shared_ptr<class_objects_cache> class_cache;
    std::shared_ptr<objects_cache> obj_cache;

    // TODO, workaround
    std::shared_ptr<smart_object> last_obj;
};
}  // namespace model
}  // namespace agea