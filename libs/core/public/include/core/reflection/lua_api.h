#pragma once

#include <utils/singleton_instance.h>

#include <memory>
#include <string>

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

    const std::string&
    buffer();

    void
    reset()
    {
        m_buffer.clear();
    }

    void
    write_buffer(const char* c, size_t n);

private:
    std::unique_ptr<sol::state> m_state;
    std::string m_buffer;
};

}  // namespace agea::reflection
