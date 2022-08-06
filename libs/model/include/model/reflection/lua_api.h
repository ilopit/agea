#pragma once

#include "utils/weird_singletone.h"

#include <memory>

namespace sol
{
class state;
}

namespace agea::reflection
{
class lua_api
{
public:
    lua_api();
    ~lua_api();

    sol::state&
    state();

private:
    std::unique_ptr<sol::state> m_state;
};

}  // namespace agea::reflection

namespace agea::glob
{
struct lua_api : public ::agea::selfcleanable_singleton<::agea::reflection::lua_api>
{
    static singletone_autodeleter s_closure;
};
}  // namespace agea::glob