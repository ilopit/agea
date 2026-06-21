#pragma once

#include "packages/nevermatch/model/door_trigger.ar.h"

#include "packages/root/model/game_object.h"

namespace kryga::nevermatch
{

// Example of the per-object extension axis: a reflected custom entity defined in a
// game package. Because it is KRG_ar_class, it is registered in reflection and can be
// placed in levels by type id, with its serializable properties persisted in .aobj.
//
// clang-format off
KRG_ar_class(
    mcp_hint = "Example doorway entity that names the level it leads to."
);
class door_trigger : public ::kryga::root::game_object
// clang-format on
{
    KRG_gen_meta__door_trigger();

public:
    KRG_gen_class_meta(door_trigger, ::kryga::root::game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params
    {
    };

    bool
    construct(construct_params& p);

    void
    begin_play() override;

    void
    on_tick(float dt) override;

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Trigger",
        access       = all,
        serializable = true,
        mcp_hint     = "Level id this door leads to."
    );
    utils::id m_target_level;
    // clang-format on
};

}  // namespace kryga::nevermatch
