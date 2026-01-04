#include "packages/tbs/model/hex_grid.h"
#include "packages/tbs/model/hex_tile.h"

namespace kryga
{
namespace tbs
{

KRG_gen_class_cd_default(hex_grid);

bool
hex_grid::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    hex_tile::construct_params cp;

    for (int r = 0; r < 100; ++r)
        for (int q = -100; q < 100; ++q)
        {
            cp.hex_q = q;
            cp.hex_r = r;

            auto dl = spawn_component<hex_tile>(m_root_component, AID("hex_tile"), cp);
        }

    return true;
}

void
hex_grid::add_content(int q, int r, const utils::id& proto_id)
{
    m_content.emplace_back(q, r, proto_id);
}

void
hex_grid::add_content(const hex_tile_entry& entry)
{
    m_content.push_back(entry);
}

void
hex_grid::clear_content()
{
    m_content.clear();
}

hex_tile*
hex_grid::get_tile_at(const hex_coord& coord) const
{
    auto it = m_tiles.find(coord);
    if (it != m_tiles.end())
    {
        return it->second;
    }
    return nullptr;
}

bool
hex_grid::has_tile_at(const hex_coord& coord) const
{
    return m_tiles.find(coord) != m_tiles.end();
}

std::vector<hex_tile*>
hex_grid::get_neighbors(const hex_coord& coord) const
{
    std::vector<hex_tile*> result;
    result.reserve(6);

    auto neighbors = coord.neighbors();
    for (const auto& neighbor_coord : neighbors)
    {
        auto* tile = get_tile_at(neighbor_coord);
        if (tile)
        {
            result.push_back(tile);
        }
    }

    return result;
}

void
hex_grid::register_tile(hex_tile* tile)
{
    if (tile)
    {
        m_tiles[tile->get_hex_coord()] = tile;
    }
}

void
hex_grid::unregister_tile(const hex_coord& coord)
{
    m_tiles.erase(coord);
}

void
hex_grid::clear_tiles()
{
    m_tiles.clear();
}

}  // namespace tbs
}  // namespace kryga
