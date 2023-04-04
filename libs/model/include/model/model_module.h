#pragma once

#include "reflection/module.h"

#include "reflection/model_property_overrides.h"

namespace agea
{
namespace model
{
namespace types
{
extern const utils::id tid_string;

extern const utils::id tid_bool;

extern const utils::id tid_i8;
extern const utils::id tid_i16;
extern const utils::id tid_i32;
extern const utils::id tid_i64;
extern const utils::id tid_u8;
extern const utils::id tid_u16;
extern const utils::id tid_u32;
extern const utils::id tid_u64;

extern const utils::id tid_float;
extern const utils::id tid_double;

extern const utils::id tid_id;
extern const utils::id tid_buffer;
extern const utils::id tid_color;

extern const utils::id tid_vec2;
extern const utils::id tid_vec3;
extern const utils::id tid_vec4;

extern const utils::id tid_obj;
extern const utils::id tid_com;
extern const utils::id tid_txt;
extern const utils::id tid_msh;
extern const utils::id tid_se;
extern const utils::id tid_mat;

}  // namespace types

class model_module : public ::agea::reflection::module
{
public:
    model_module(const ::agea::utils::id& id)
        : ::agea::reflection::module(id)
    {
    }

    virtual bool
    init_types();

    virtual bool
    init_reflection();

    static model_module&
    instance();
};

}  // namespace model
}  // namespace agea