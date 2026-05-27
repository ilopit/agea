#pragma once

#include <span>
#include <string_view>

namespace kryga::gs
{

class state;

class system
{
public:
    virtual ~system() = default;
    virtual std::string_view
    name() const = 0;
    virtual std::span<const std::string_view>
    deps() const = 0;

    virtual void
    on_connect(state&)
    {
    }
    virtual void
    on_init(state&)
    {
    }
};

}  // namespace kryga::gs
