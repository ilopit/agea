#include "core/reflection/lua_api.h"

#include <global_state/global_state.h>
#include <sol2_unofficial/sol.h>

namespace
{

int
l_my_print(lua_State* L)
{
    int n = lua_gettop(L); /* number of arguments */
    int i;

    auto* lua = agea::glob::glob_state().get_lua();

    for (i = 1; i <= n; i++)
    { /* for each argument */
        size_t l;
        const char* s = luaL_tolstring(L, i, &l); /* convert it to string */
        if (i > 1)
        {                             /* not the first element? */
            lua_writestring("\t", 1); /* add a tab before it */
            lua->write_buffer("\t", 1);
        }
        lua_writestring(s, l); /* print it */
        lua->write_buffer(s, l);
        lua_pop(L, 1); /* pop result */
    }
    lua_writeline();
    return 0;
}

const struct luaL_Reg printlib[] = {
    {"print", l_my_print}, {NULL, NULL} /* end of array */
};

int
luaopen_luamylib(lua_State* L)
{
    lua_getglobal(L, "_G");
    luaL_setfuncs(L, printlib, 0);
    lua_pop(L, 1);

    return 1;
}
}  // namespace

namespace agea
{

namespace reflection
{

lua_api::lua_api()
    : m_state(std::make_unique<sol::state>())
{
    m_state->open_libraries(sol::lib::base, sol::lib::io);

    m_state->require("agea", luaopen_luamylib, true);
}

lua_api::~lua_api()
{
}

sol::state&
lua_api::state()
{
    return *m_state;
}

void
lua_api::write_buffer(const char* c, size_t n)
{
    m_buffer.append(c, n);
}

const std::string&
lua_api::buffer()
{
    return m_buffer;
}

}  // namespace reflection
}  // namespace agea