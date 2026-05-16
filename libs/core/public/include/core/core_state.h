#pragma once

namespace kryga
{
namespace gs
{
class state;
}

namespace core
{

struct state_mutator__model
{
    static void
    set(gs::state& es);
};

struct state_mutator__lua_api
{
    static void
    set(gs::state& es);
};

}  // namespace core
};  // namespace kryga
