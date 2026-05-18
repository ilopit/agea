#include "core/construction_utils.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>

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

    reflection::property_context__compare compare_ctx{nullptr, &left, &right};

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
destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc)
{
    return result_code::failed;
}

}  // namespace core
}  // namespace kryga
