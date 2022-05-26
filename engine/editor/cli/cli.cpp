#include "editor/cli/cli.h"

#include "model/level.h"
#include "reflection/property.h"
#include "utils/string_utility.h"
#include "model/caches/materials_cache.h"

#include <array>
#include <algorithm>

namespace agea
{
namespace editor
{

namespace
{
reflection::property*
find_property(model::smart_object* obj, const std::string& name)
{
    for (auto& categories : obj->reflection()->m_editor_properties)
    {
        for (auto& p : categories.second)
        {
            if (p->name == name)
            {
                return p.get();
            }
        }
    }

    return nullptr;
}

model::smart_object*
find_obj(const std::string& object_id)
{
    model::smart_object* obj = nullptr;

    auto composite_id = string_utils::split(object_id, ":");

    if (composite_id.size() == 1)
    {
        obj = glob::level::get()->find_object(object_id);
    }
    if (composite_id.size() == 2)
    {
        auto& type = composite_id[0];
        auto& name = composite_id[1];
        if (type == "obj")
        {
            obj = glob::level::get()->find_object(name);
        }
        else if (type == "com")
        {
            obj = glob::level::get()->find_component(name);
        }
        else if (type == "mat")
        {
            auto sobj = glob::materials_cache::get()->get(name);
            if (sobj)
            {
                obj = sobj.get();
            }
        }
    }

    return obj;
}

}  // namespace

bool
cli::print_properties(const std::string& object_id, std::string& result)
{
    auto obj = find_obj(object_id);
    if (!obj)
    {
        return false;
    }

    static fixed_size_buffer buffer;
    buffer.fill('\0');

    for (auto& categories : obj->reflection()->m_editor_properties)
    {
        result += "=== ";
        result += categories.first;
        result += " ===\n";
        for (auto& p : categories.second)
        {
            result += p->name;
            result += " = ";
            reflection::property::save_to_string(*p, (::agea::blob_ptr)obj, buffer);
            result += buffer.data();
            result += "\n";
        }
    }

    if (!result.empty())
    {
        result.pop_back();
    }

    return true;
}

bool
cli::get_property(const std::string& object_id,
                  const std::string& property_name,
                  const std::string& subproperty_hint,
                  fixed_size_buffer& result)
{
    auto obj = find_obj(object_id);
    if (!obj)
    {
        return false;
    }

    auto p = find_property(obj, property_name);

    if (!p)
    {
        return false;
    }

    if (subproperty_hint.empty())
    {
        if (reflection::property::save_to_string(*p, (::agea::blob_ptr)obj, result))
        {
            return false;
        }
    }
    else
    {
        if (!p->save_to_string_with_hint((::agea::blob_ptr)obj, subproperty_hint, result))
        {
            return false;
        }
    }

    return true;
}

bool
cli::set_property(const std::string& object_id,
                  const std::string& property_name,
                  const std::string& subproperty_hint,
                  const std::string& value)
{
    auto obj = find_obj(object_id);
    if (!obj)
    {
        return false;
    }

    static std::string buffer;
    buffer.clear();

    auto p = find_property(obj, property_name);

    if (!p)
    {
        return false;
    }
    if (subproperty_hint.empty())
    {
        if (!p->load_from_string((::agea::blob_ptr)obj, value))
        {
            return false;
        }
    }
    else
    {
        if (!p->load_from_string_with_hint((::agea::blob_ptr)obj, subproperty_hint, value))
        {
            return false;
        }
    }

    obj->editor_update();

    return true;
}

}  // namespace editor
}  // namespace agea