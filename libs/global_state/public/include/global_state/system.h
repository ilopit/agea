#pragma once

#include <span>
#include <string_view>

namespace kryga::gs
{

class system
{
public:
    virtual ~system() = default;
    virtual std::string_view system_name() const = 0;
    virtual std::span<const std::string_view> system_deps() const = 0;
};

}  // namespace kryga::gs
