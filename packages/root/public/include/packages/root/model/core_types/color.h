#pragma once

#include <ar/ar_defines.h>

#include <glm_unofficial/glm.h>

namespace agea
{
namespace root
{
// clang-format off
AGEA_ar_struct(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::agea::core::color>,
               compare_handler     = ::agea::reflection::utils::cpp_default__compare<::agea::core::color>,
               serialize_handler   = ::agea::root::custom::color__save,
               deserialize_handler = ::agea::root::custom::color__load);
struct color : ::glm::vec4
// clang-format on
{
    glm::vec4 m_data;
};

}  // namespace root
}  // namespace agea