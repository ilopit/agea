#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace kryga
{
namespace root
{

// clang-format off
KRG_ar_struct(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::kryga::root::vec2>,
               compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::kryga::root::vec2>,
               to_string_handler   = ::kryga::root::vec2__to_string);
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

    KRG_ar_ctor("category=world");
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
    KRG_ar_function("category=world");
    void
    set_x(float v)
    {
        x = v;
    }

    KRG_ar_function("category=world");
    float
    get_x() const
    {
        return x;
    }

    // y
    KRG_ar_function("category=world");
    void
    set_y(float v)
    {
        y = v;
    }

    KRG_ar_function("category=world");
    float
    get_y() const
    {
        return y;
    }
};

}  // namespace root
}  // namespace kryga