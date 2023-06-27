#include "utils/dynamic_object.h"

#include "utils/dynamic_object_builder.h"
#include "utils/agea_log.h"

namespace agea
{
namespace utils
{

base_view::base_view(uint64_t offset,
                     uint8_t* data,
                     const dynobj_field* cur_field,
                     const std::shared_ptr<dynobj_layout>& layout)
    : m_offset(offset)
    , m_data(data)
    , m_cur_field(cur_field)
    , m_layout(layout)
{
}

uint32_t
base_view::get_type_id(const dynobj_field* f)
{
    return f->type;
}

uint64_t
base_view::get_idx_offset(const dynobj_field* f, uint64_t idx)
{
    return math_utils::align_as(f->type_size, f->items_alighment) * idx;
}

base_view
base_view::build_for_subobject(uint64_t field_idx, uint64_t idx)
{
    auto sub_field = &m_cur_field->sub_field_layout->get_fields()[field_idx];

    AGEA_check(sub_field->is_array(), "Should be an array!");

    auto offset = get_idx_offset(sub_field, idx);

    return base_view(sub_field->offset + m_offset + offset, m_data, sub_field,
                     m_cur_field->sub_field_layout);
}

base_view
base_view::build_for_subobject(uint64_t field_idx)
{
    auto sub_field = &m_cur_field->sub_field_layout->get_fields()[field_idx];

    return base_view(sub_field->offset + m_offset, m_data, sub_field,
                     m_cur_field->sub_field_layout);
}

const dynobj_field*
base_view::sub_field_by_idx(uint64_t idx) const
{
    if (idx >= m_cur_field->sub_field_layout->get_fields().size())
    {
        return nullptr;
    }

    return &m_cur_field->sub_field_layout->get_fields()[idx];
}

bool
base_view::write_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* data)
{
    ALOG_INFO("W id:{0} from:{1} size:{2}", f->id.cstr(), f->offset + offset, f->type_size);

    memcpy(m_data + offset + f->offset, data, f->type_size);

    return true;
}

bool
base_view::read_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* dst)
{
    ALOG_INFO("R id:{0} from:{1} size:{2}", f->id.cstr(), f->offset + offset, f->type_size);

    memcpy(dst, m_data + offset + f->offset, f->type_size);

    return true;
}

bool
base_view::is_array() const
{
    return m_cur_field->is_array();
}

bool
base_view::is_array(const dynobj_field* f)
{
    return f->is_array();
}

bool
base_view::is_object() const
{
    return (bool)m_cur_field->sub_field_layout;
}

bool
base_view::is_object(const dynobj_field* f)
{
    return f->is_obj();
}

bool
base_view::valid() const
{
    return m_cur_field;
}

uint64_t
base_view::size(uint64_t idx) const
{
    return m_layout->get_fields()[idx].size;
}

uint64_t
base_view::offset(uint64_t idx) const
{
    return m_layout->get_fields()[idx].offset;
}

const dynobj_field*
base_view::dyn_field(const std::shared_ptr<dynobj_layout>& layout, uint64_t idx)
{
    return &layout->get_fields()[idx];
}

uint64_t
base_view::get_offest(uint64_t idx)
{
    return m_cur_field->offset +
           math_utils::align_as(m_cur_field->type_size, m_cur_field->items_alighment) * idx;
}

dynamic_object::dynamic_object(const std::shared_ptr<dynobj_layout>& l)
    : m_obj_data(l->get_object_size())
    , m_layout(l)
{
}

const dynobj_field*
dynamic_object::get_dyn_field(uint64_t pos)
{
    return &m_layout->get_fields()[pos];
}

}  // namespace utils
}  // namespace agea