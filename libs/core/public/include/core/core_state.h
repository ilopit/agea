#pragma once

namespace agea
{
namespace gs
{
class state;
}

namespace core
{

struct state_mutator__caches
{
    static void
    set(gs::state& es);
};

struct state_mutator__level_manager
{
    static void
    set(gs::state& es);
};

struct state_mutator__package_manager
{
    static void
    set(gs::state& es);
};

struct state_mutator__id_generator
{
    static void
    set(gs::state& es);
};

struct state_mutator__reflection_manager
{
    static void
    set(gs::state& es);
};

struct state_mutator__lua_api
{
    static void
    set(gs::state& es);
};

struct state_mutator__current_level
{
    static void
    set(class level& lvl, gs::state& es);
};
}  // namespace core
};  // namespace agea