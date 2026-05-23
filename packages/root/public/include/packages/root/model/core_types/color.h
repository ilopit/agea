#pragma once

#include <ar/ar_defines.h>

#include <glm_unofficial/glm.h>

namespace kryga
{
namespace root
{
// clang-format off
KRG_ar_struct(
    copy_handler    = ::kryga::reflection::utils::cpp_default__copy<::kryga::core::color>,
    compare_handler = ::kryga::reflection::utils::cpp_default__compare<::kryga::core::color>,
    save_handler    = ::kryga::root::custom::color__save,
    mcp_schema      = "array:number:4",
    mcp_hint        = "RGBA color [r g b a]"
);
struct color : ::glm::vec4
// clang-format on
{
    glm::vec4 m_data;
};

}  // namespace root
}  // namespace kryga