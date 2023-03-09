#include "utils/dynamic_object_builder.h"

namespace agea
{
namespace utils
{

dynamic_object
basic_dynamic_object_layout_builder::make_obj()
{
    return dynamic_object(m_layout);
}

agea::utils::dynamic_object
basic_dynamic_object_layout_builder::make_empty_obj()
{
    return dynamic_object(m_layout);
}

}  // namespace utils
}  // namespace agea