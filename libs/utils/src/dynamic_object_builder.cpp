#include "utils/dynamic_object_builder.h"

namespace agea
{
namespace utils
{

std::shared_ptr<agea::utils::dynobj_layout>
basic_dynamic_object_layout_builder::unwrap_subojb(const std::shared_ptr<dynobj_layout>& obj)
{
    return obj->get_fields()[0].sub_field_layout;
}

}  // namespace utils
}  // namespace agea