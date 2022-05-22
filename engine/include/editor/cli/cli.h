#pragma once

#include "core/agea_minimal.h"
#include "utils/weird_singletone.h"

namespace agea
{
namespace editor
{

class cli
{
public:
    static bool
    print_properties(const std::string& object_id, std::string& list);

    static bool
    get_property(const std::string& object_id,
                 const std::string& property_name,
                 const std::string& subproperty_hint,
                 std::string& list);

    static bool
    set_property(const std::string& object_id,
                 const std::string& property_name,
                 const std::string& subproperty_hint,
                 const std::string& value);
};

}  // namespace editor

namespace glob
{

class cli : public weird_singleton<::agea::editor::cli>
{
};

}  // namespace glob

}  // namespace agea