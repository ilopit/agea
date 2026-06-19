#pragma once

#include "packages/tbs/model/hex_grid.ar.h"
#include "packages/tbs/model/hex_coord.h"
#include "packages/tbs/model/hex_tile.h"

#include "packages/root/model/game_object.h"

#include <utils/id.h>

#include <unordered_map>
#include <vector>

namespace kryga
{
namespace tbs
{

// Entry defining a tile to spawn in the grid
struct hex_tile_entry
{
    int q = 0;
    int r = 0;
    utils::id proto_id;

    hex_tile_entry() = default;
    hex_tile_entry(int q_, int r_, const utils::id& proto)
        : q(q_)
        , r(r_)
        , proto_id(proto)
    {
    }

    hex_coord
    get_coord() const
    {
        return hex_coord(q, r);
    }
};

// Grid container that spawns and manages hex tiles
// clang-format off
KRG_ar_class(
    mcp_hint = "Hex grid container — spawns and manages hex_tile components in an axial coordinate "
               "system"
);
class hex_grid : public ::kryga::root::game_object
// clang-format on
{
    KRG_gen_meta__hex_grid();

public:
    KRG_gen_class_meta(hex_grid, ::kryga::root::game_object);
    KRG_gen_construct_params
    {
        float hex_size = 1.0f;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);
};

}  // namespace tbs
}  // namespace kryga
