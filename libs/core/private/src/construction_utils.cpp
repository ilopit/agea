#include "core/construction_utils.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>

#include <utils/kryga_log.h>

namespace kryga
{
namespace core
{

result_code
diff_object_properties(const root::smart_object& left,
                       const root::smart_object& right,
                       std::vector<reflection::property*>& diff)
{
    if (&left == &right)
    {
        return result_code::ok;
    }

    if (left.get_type_id() != right.get_type_id())
    {
        return result_code::failed;
    }

    auto& properties = left.get_reflection()->m_serialization_properties;

    reflection::property_context__compare compare_ctx{
        .p = nullptr, .src_obj = &left, .dst_obj = &right};

    for (auto& p : properties)
    {
        compare_ctx.p = p.get();

        auto same = p->compare_handler(compare_ctx) == result_code::ok;

        if (!same)
        {
            diff.push_back(p.get());
        }
    }

    return result_code::ok;
}

result_code
snapshot_object_properties(root::smart_object& from, root::smart_object& to)
{
    // Snapshot the FULL property set, not just the serializable subset: play mode
    // can mutate non-serializable runtime/editor properties too, and a true reset
    // must restore those. Sub-object/collection props are skipped by the handler.
    auto& properties = from.get_reflection()->m_properties;

    // ctor = nullptr: the snapshot path never allocates a sub-object (those props
    // are skipped), so no object_constructor is required.
    reflection::property_context__copy cctx{.src_property = nullptr,
                                            .dst_property = nullptr,
                                            .src_obj = &from,
                                            .dst_obj = &to,
                                            .ctor = nullptr};

    for (auto& p : properties)
    {
        cctx.src_property = p.get();
        cctx.dst_property = p.get();

        if (auto rc = p->snapshot_handler(cctx); rc != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return rc;
        }
    }

    return result_code::ok;
}

result_code
destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc)
{
    return result_code::failed;
}

}  // namespace core
}  // namespace kryga
