#include "utils/dynamic_object.h"

#include "utils/dynamic_object_builder.h"
#include "utils/agea_log.h"

namespace agea
{
namespace utils
{

void
base_view::print(size_t nest_offset, std::string& str, bool no_detailds) const
{
    uint64_t idx = 0;
    while (auto field = field_by_idx(idx++))
    {
        str += std::string(nest_offset, ' ');
        str += "[";
        str += field->id.cstr();
        str += "] ";

        if (!no_detailds)
        {
            // TODO add me later
        }

        if (field->is_array)
        {
            str += "[arr:";
            str += field->items_count == uint64_t(-1) ? "inf" : std::to_string(field->items_count);
            str += "] ";
        }

        str += "[";
        str +=
            field->sub_field_layout ? field->sub_field_layout->get_id().cstr() : field->type_name;
        str += "]\n";

        if (field->sub_field_layout)
        {
            auto v = build_for_subobject(field->index);

            v.print(nest_offset + 2, str, no_detailds);
        }
    }
}

void
base_view::print(std::string& str, bool no_detailds) const
{
    str += "\n\n[";
    str += m_cur_field->id.cstr();
    str += "] ";

    if (!no_detailds)
    {
        // TODO add me later
    }

    str += "[";
    str += m_cur_field->sub_field_layout ? m_cur_field->sub_field_layout->get_id().cstr()
                                         : m_cur_field->type_name;
    str += "]\n";

    if (m_cur_field->sub_field_layout)
    {
        print(2, str, no_detailds);
    }
}

void
base_view::print_to_std() const
{
    std::string buffer;

    print(buffer, true);

    ALOG_INFO("{0}", buffer);
}

dynobj_field_context*
base_view::context(const dynobj_field* f)
{
    return f->context.get();
}

const agea::utils::id&
base_view::id() const
{
    return m_cur_field->id;
}

base_view::base_view(uint64_t offset,
                     uint8_t* data,
                     const dynobj_field* cur_field,
                     const utils::dynobj_layout_sptr& layout)
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

base_view
base_view::build_for_subobject(uint64_t field_idx, uint64_t idx) const
{
    auto sub_field = field_by_idx(m_cur_field->sub_field_layout, field_idx);

    AGEA_check(sub_field->is_array, "Should be an array!");

    auto offset = glob_offset_at_index(sub_field, idx);

    return base_view(offset, m_data, sub_field, m_cur_field->sub_field_layout);
}

base_view
base_view::build_for_subobject(uint64_t field_idx) const
{
    auto sub_field = field_by_idx(m_cur_field->sub_field_layout, field_idx);

    return base_view(glob_offset(sub_field), m_data, sub_field, m_cur_field->sub_field_layout);
}

const dynobj_field*
base_view::field_by_idx(uint64_t idx) const
{
    if (idx < field_count())
    {
        return field_by_idx(m_cur_field->sub_field_layout, idx);
    }

    return nullptr;
}

bool
base_view::write_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* data)
{
    ALOG_INFO("W id:{0} from:{1} size:{2}", f->id.cstr(), f->offset + offset, f->type_size);

    memcpy(m_data + offset, data, f->type_size);

    return true;
}

bool
base_view::read_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* dst)
{
    ALOG_INFO("R id:{0} from:{1} size:{2}", f->id.cstr(), f->offset + offset, f->type_size);

    memcpy(dst, m_data + offset, f->type_size);

    return true;
}

bool
base_view::is_array() const
{
    return m_cur_field->is_array;
}

bool
base_view::is_array(const dynobj_field* f)
{
    return f->is_array;
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
base_view::size(const dynobj_field* f)
{
    return f->size;
}

uint64_t
base_view::field_count() const
{
    return m_cur_field->sub_field_layout->get_fields().size();
}

uint64_t
base_view::glob_offset(uint64_t idx) const
{
    return glob_offset(field_by_idx(idx));
}

uint64_t
base_view::glob_offset(const dynobj_field* f) const
{
    return m_offset + f->offset;
}

uint64_t
base_view::glob_offset_at_index(const dynobj_field* f, uint64_t idx) const
{
    return glob_offset(f) + offset_at_index(f, idx);
}

uint64_t
base_view::offset_at_index(const dynobj_field* f, uint64_t idx)
{
    return math_utils::align_as(f->type_size, f->items_alighment) * idx;
}

const dynobj_field*
base_view::field_by_idx(const utils::dynobj_layout_sptr& layout, uint64_t idx)
{
    AGEA_check(idx < layout->get_fields().size(), "Should be in index range");

    return &layout->get_fields()[idx];
}

dynobj::dynobj(const std::shared_ptr<dynobj_layout>& l)
    : m_obj_data(l->get_object_size())
    , m_layout(l)
{
}

const dynobj_field*
dynobj::get_dyn_field(uint64_t pos)
{
    return &m_layout->get_fields()[pos];
}

}  // namespace utils
}  // namespace agea