#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace root
{

// clang-format off
AGEA_ar_struct(copy_handler        = ::agea::reflection::utils::cpp_default__copy<::agea::root::vec2>,
               compare_handler     = ::agea::reflection::utils::cpp_default__compare<::agea::root::vec2>,
               to_string_handler   = ::agea::root::vec2__to_string);
struct vec2 : ::glm::vec2
// clang-format on
{
    vec2()
        : glm::vec2()
    {
    }

    vec2(const glm::vec2& v)
        : glm::vec2(v)
    {
    }

    AGEA_ar_ctor("category=world");
    vec2(float x, float y)
        : glm::vec2(x, y)
    {
    }

    const glm::vec2&
    as_glm() const
    {
        return *this;
    }

    glm::vec2&
    as_glm()
    {
        return *this;
    }

    // x
    AGEA_ar_function("category=world");
    void
    set_x(float v)
    {
        x = v;
    }

    AGEA_ar_function("category=world");
    float
    get_x() const
    {
        return x;
    }

    // y
    AGEA_ar_function("category=world");
    void
    set_y(float v)
    {
        y = v;
    }

    AGEA_ar_function("category=world");
    float
    get_y() const
    {
        return y;
    }
};

}  // namespace root
}  // namespace agea