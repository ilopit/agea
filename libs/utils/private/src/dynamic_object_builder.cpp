#include "utils/dynamic_object_builder.h"

namespace kryga
{
namespace utils
{

std::shared_ptr<kryga::utils::dynobj_layout>
basic_dynobj_layout_builder::unwrap_subojb(const std::shared_ptr<dynobj_layout>& obj)
{
    return obj->get_fields()[0].sub_field_layout;
}

}  // namespace utils
}  // namespace kryga