#include "model/reflection/lua_api.h"

#include <sol/sol.hpp>

sol::state&
agea::get_lua_state()
{
    static sol::state s;
    return s;
}
