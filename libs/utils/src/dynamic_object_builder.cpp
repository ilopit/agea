#include "utils/dynamic_object_builder.h"

namespace agea
{
namespace utils
{

dynamic_object
basic_dynamic_object_layout_builder::make_obj()
{
    return dynamic_object(m_layout, m_layout->get_object_size());
}

dynamic_object
basic_dynamic_object_layout_builder::make_empty_obj()
{
    return dynamic_object(m_layout);
}

}  // namespace utils
}  // namespace agea