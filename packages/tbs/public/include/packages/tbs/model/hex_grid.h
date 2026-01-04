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
KRG_ar_class();
class hex_grid : public ::kryga::root::game_object
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

    // Content management (tiles to spawn)
    void
    add_content(int q, int r, const utils::id& proto_id);

    void
    add_content(const hex_tile_entry& entry);

    void
    clear_content();

    const std::vector<hex_tile_entry>&
    get_content() const
    {
        return m_content;
    }

    // Spawned tiles access
    hex_tile*
    get_tile_at(const hex_coord& coord) const;

    bool
    has_tile_at(const hex_coord& coord) const;

    const std::unordered_map<hex_coord, hex_tile*, hex_coord_hash>&
    get_tiles() const
    {
        return m_tiles;
    }

    size_t
    get_tile_count() const
    {
        return m_tiles.size();
    }

    // Get neighbors of a tile that exist in grid
    std::vector<hex_tile*>
    get_neighbors(const hex_coord& coord) const;

    // Register an externally created tile
    void
    register_tile(hex_tile* tile);

    // Unregister a tile
    void
    unregister_tile(const hex_coord& coord);

    // Clear all tiles (does not destroy them)
    void
    clear_tiles();

protected:
    // Content: tile entries to spawn (runtime, not serialized yet)
    std::vector<hex_tile_entry> m_content;

    // Runtime: spawned/registered tiles
    std::unordered_map<hex_coord, hex_tile*, hex_coord_hash> m_tiles;
};

}  // namespace tbs
}  // namespace kryga
