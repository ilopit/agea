#pragma once

#include "packages/root/model/player.ar.h"

#include "packages/root/model/game_object.h"

namespace kryga
{
namespace root
{

class camera_component;
class input_component;

// clang-format off
KRG_ar_class(
    mcp_hint = "Generic player: a game_object owning a camera + input component. Games subclass this to add control logic."
);
class player : public ::kryga::root::game_object
// clang-format on
{
    KRG_gen_meta__player();

public:
    KRG_gen_class_meta(player, ::kryga::root::game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params{};

    camera_component*
    get_camera() const
    {
        return m_camera;
    }

    input_component*
    get_input() const
    {
        return m_input;
    }

    void
    set_target(root::game_object* t)
    {
        m_target = t;
    }

    root::game_object*
    get_target() const
    {
        return m_target;
    }

protected:
    bool
    construct(this_class::construct_params& p);

    camera_component* m_camera = nullptr;
    input_component* m_input = nullptr;
    root::game_object* m_target = nullptr;
};

}  // namespace root
}  // namespace kryga
