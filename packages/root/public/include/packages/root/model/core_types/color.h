#pragma once

#include <ar/ar_defines.h>

#include <glm_unofficial/glm.h>

namespace agea
{
namespace root
{
// clang-format off
AGEA_ar_struct(copy_handler        = ::agea::reflection::utils::cpp_copy<::agea::core::color>,
               compare_handler     = ::agea::reflection::utils::cpp_compare<::agea::core::color>,
               serialize_handler   = ::agea::root::custom::color__serialize,
               deserialize_handler = ::agea::root::custom::color__deserialize);
struct color : ::glm::vec4
// clang-format on
{
    glm::vec4 m_data;
};

}  // namespace root
}  // namespace agea