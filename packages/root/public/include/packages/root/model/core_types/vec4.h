#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace kryga
{
namespace root
{

// clang-format off
KRG_ar_struct(copy_handler        = ::kryga::reflection::utils::cpp_default__copy<::kryga::root::vec4>,
               compare_handler     = ::kryga::reflection::utils::cpp_default__compare<::kryga::root::vec4>,
               to_string_handler   = ::kryga::root::vec4__to_string);
struct vec4 : ::glm::vec4
// clang-format on
{
    vec4()
        : glm::vec4()
    {
    }

    vec4(const glm::vec4& v)
        : glm::vec4(v)
    {
    }

    KRG_ar_ctor("category=world");
    vec4(float x, float y, float z, float w)
        : glm::vec4(x, y, z, w)
    {
    }

    KRG_ar_ctor("category=world");
    vec4(float v)
        : glm::vec4(v)
    {
    }

    const glm::vec4&
    as_glm() const
    {
        return *this;
    }

    glm::vec4&
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

    // z
    KRG_ar_function("category=world");
    void
    set_z(float v)
    {
        z = v;
    }

    KRG_ar_function("category=world");
    float
    get_z() const
    {
        return z;
    }

    // w
    KRG_ar_function("category=world");
    void
    set_w(float v)
    {
        w = v;
    }

    KRG_ar_function("category=world");
    float
    get_w() const
    {
        return w;
    }
};

}  // namespace root
}  // namespace kryga