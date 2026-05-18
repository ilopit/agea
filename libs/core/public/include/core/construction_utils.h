#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include <packages/root/model/smart_object.h>

#include <memory>
#include <vector>

namespace kryga
{

namespace reflection
{
class property;
}

namespace core
{

class object_load_context;

static inline const auto ks_class_default = root::smart_object_flags{.instance_obj = false,
                                                                     .derived_obj = false,
                                                                     .runtime_obj = true,
                                                                     .mirror_obj = false,
                                                                     .default_obj = true,
                                                                     .readonly = true};

static inline const auto ks_class_derived = root::smart_object_flags{.instance_obj = false,
                                                                     .derived_obj = true,
                                                                     .runtime_obj = false,
                                                                     .mirror_obj = false,
                                                                     .default_obj = false,
                                                                     .readonly = true};

static inline const auto ks_instance_derived = root::smart_object_flags{.instance_obj = true,
                                                                        .derived_obj = true,
                                                                        .runtime_obj = false,
                                                                        .mirror_obj = false,
                                                                        .default_obj = false};

result_code
diff_object_properties(const root::smart_object& left,
                       const root::smart_object& right,
                       std::vector<reflection::property*>& diff);

template <typename T>
std::shared_ptr<T>
alloc_empty_object(const utils::id& id = T::AR_TYPE_id())
{
    return T::AR_TYPE_create_empty_obj(id);
}

result_code
destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc);

template <typename T>
result_code
destroy_default_class_obj(object_load_context& occ)
{
    destroy_default_class_obj_impl(T::AR_TYPE_reflection().type_name, occ);
    return result_code::ok;
}

}  // namespace core
}  // namespace kryga
