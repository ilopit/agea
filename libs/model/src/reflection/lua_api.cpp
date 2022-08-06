#include "model/reflection/lua_api.h"

#include <sol/sol.hpp>

namespace agea::reflection
{

lua_api::lua_api()
    : m_state(std::make_unique<sol::state>())
{
    m_state->open_libraries(sol::lib::base);
}

lua_api::~lua_api()
{
}

sol::state&
lua_api::state()
{
    return *m_state;
}

}  // namespace agea::reflection

agea::singletone_autodeleter agea::glob::lua_api::s_closure;
