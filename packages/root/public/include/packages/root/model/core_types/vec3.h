#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace kryga
{
namespace root
{
// clang-format off
KRG_ar_struct(
    copy_handler      = ::kryga::reflection::utils::cpp_default__copy<::kryga::root::vec2>,
    compare_handler   = ::kryga::reflection::utils::cpp_default__compare<::kryga::root::vec2>,
    save_handler      = ::kryga::root::vec3__save,
    load_handler      = ::kryga::root::vec3__load,
    json_save_handler = ::kryga::root::vec3__json_save,
    json_load_handler = ::kryga::root::vec3__json_load,
    mcp_schema        = "array:number:3",
    mcp_hint          = "3D vector [x y z]"
);
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
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_x(float v)
    // clang-format on
    {
        x = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_x() const
    // clang-format on
    {
        return x;
    }

    // y
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_y(float v)
    // clang-format on
    {
        y = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_y() const
    // clang-format on
    {
        return y;
    }

    // z
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_z(float v)
    // clang-format on
    {
        z = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_z() const
    // clang-format on
    {
        return z;
    }
};

}  // namespace root
}  // namespace kryga