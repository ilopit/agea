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
               serialize_handler   = ::kryga::root::vec3__save,
               load_derive_handler = ::kryga::root::vec3__load,
               to_string_handler   = ::kryga::root::vec3__to_string);
struct vec3 : ::glm::vec3
// clang-format on
{
    vec3()
        : glm::vec3()
    {
    }

    vec3(const glm::vec3& v)
        : glm::vec3(v)
    {
    }

    KRG_ar_ctor("category=world");
    vec3(float x, float y, float z)
        : glm::vec3(x, y, z)
    {
    }

    KRG_ar_ctor("category=world");
    vec3(float v)
        : glm::vec3(v)
    {
    }

    const glm::vec3&
    as_glm() const
    {
        return *this;
    }

    glm::vec3&
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
};

}  // namespace root
}  // namespace kryga