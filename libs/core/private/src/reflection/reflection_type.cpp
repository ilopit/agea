#include "core/reflection/reflection_type.h"

#include "core/reflection/reflection_type_utils.h"
#include "global_state/global_state.h"
#include <utils/kryga_log.h>
#include <stack>

namespace kryga::reflection
{

reflection_type::reflection_type(int i, const kryga::utils::id& n)
    : type_id(i)
    , type_name(n)
{
    ::kryga::glob::glob_state().get_rm()->add_type(this);
}

reflection_type::~reflection_type()
{
    auto& rm = glob::glob_state().getr_rm();

    rm.unload_type(type_id, type_name);
}

void
reflection::reflection_type_registry::add_type(reflection_type* rt)
{
    ALOG_TRACE("Loaded {} {}", rt->type_id, rt->type_name.cstr());

    {
        auto& v = m_types[rt->type_id];

        KRG_check(!v, "Shouldn't exist");
        v = rt;
    }
    {
        auto& v = m_types_by_name[rt->type_name];

        KRG_check(!v, "Shouldn't exist");
        v = rt;
    }
}

reflection::reflection_type*
reflection::reflection_type_registry::get_type(const kryga::utils::id& id)
{
    auto itr = m_types_by_name.find(id);

    return itr != m_types_by_name.end() ? itr->second : nullptr;
}

void
reflection::reflection_type_registry::unload_type(const int type_id, const kryga::utils::id& id)
{
    ALOG_TRACE("Unloading {} {}", type_id, id.cstr());
    {
        auto itr = m_types.find(type_id);
        if (itr != m_types.end())
        {
            auto& rt = itr->second;
            KRG_check(rt->type_class != reflection_type::reflection_type_class::kryga_class ||
                           AID("smart_object") == id || get_type(rt->parent->type_id),
                       "Parent for classes should always exist");
            m_types.erase(itr);
        }
    }
    {
        auto itr = m_types_by_name.find(id);
        if (itr != m_types_by_name.end())
        {
            auto& rt = itr->second;

            KRG_check(rt->type_class != reflection_type::reflection_type_class::kryga_class ||
                           AID("smart_object") == id || get_type(rt->parent->type_id),
                       "Parent for classes should always exist");
            m_types_by_name.erase(itr);
        }
    }
}

reflection::reflection_type*
reflection::reflection_type_registry::get_type(const int id)
{
    auto itr = m_types.find(id);

    return itr != m_types.end() ? itr->second : nullptr;
}

#define KRG_override_if_null(prop) prop = prop ? prop : parent->prop

void
reflection_type::override()
{
    if (parent)
    {
        if (arch == core::architype::unknown)
        {
            arch = parent->arch;
        }

        KRG_override_if_null(save);
        KRG_override_if_null(load);
        KRG_override_if_null(copy);
        KRG_override_if_null(instantiate);
        KRG_override_if_null(compare);
        KRG_override_if_null(to_string);
        KRG_override_if_null(render_constructor);
        KRG_override_if_null(render_destructor);
    }
}

std::string
reflection_type::as_string() const
{
    std::string result;

    result += std::format("{0}:{1}:{2}\n", module_id.cstr(), type_name.cstr(), type_id);
    result += "parent:" + (parent ? parent->type_name.str() : "no") + "\n";

    result += "properties:\n";

    for (auto p : m_properties)
    {
        result += std::format("  {0}:{1}:{2}\n",
                              p->rtype ? p->rtype->type_name.str() : std::string("custom"), p->name,
                              p->rtype ? p->rtype->type_id : -1);
    }
    result += "functions:\n";

    for (auto p : m_functions)
    {
        result += std::format("  {0}\n", p->name);
    }

    return result;
}

}  // namespace kryga::reflection