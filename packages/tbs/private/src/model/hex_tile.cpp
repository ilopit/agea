#include "packages/tbs/model/hex_tile.h"

#include <core/caches/cache_set.h>
#include <global_state/global_state.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>

namespace kryga
{
namespace tbs
{

KRG_gen_class_cd_default(hex_tile);

bool
hex_tile::construct(construct_params& params)
{
    m_hex_q = params.hex_q;
    m_hex_r = params.hex_r;

    hex_coord coord(m_hex_q, m_hex_r);
    params.position = coord.to_world(1.0);

    if (!base_class::construct(params))
    {
        return false;
    }

    m_hex_size = params.hex_size;

    if (m_level)
    {
        m_mesh = glob::glob_state()
                     .get_instance_set()
                     ->objects.get_item(AID("msh_hexagon3d"))
                     ->as<root::mesh>();

        m_material = glob::glob_state()
                         .get_instance_set()
                         ->objects.get_item(AID("mt_solid_color_lit"))
                         ->as<root::material>();
    }

    // update_world_position();

    return true;
}

void
hex_tile::set_hex_coord(const hex_coord& coord)
{
    m_hex_q = coord.q;
    m_hex_r = coord.r;
    update_world_position();
}

void
hex_tile::set_hex_size(float size)
{
    m_hex_size = size;
    update_world_position();
}

void
hex_tile::update_world_position()
{
    hex_coord coord(m_hex_q, m_hex_r);
    glm::vec3 world_pos = coord.to_world(m_hex_size);
    set_position(root::vec3{world_pos.x, world_pos.y, world_pos.z});
}

}  // namespace tbs
}  // namespace kryga
