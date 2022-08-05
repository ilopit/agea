#pragma once

namespace sol
{
class state;
}

namespace agea
{
sol::state&
get_lua_state();
}