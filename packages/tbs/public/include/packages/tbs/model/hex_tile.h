#pragma once

#include "packages/tbs/model/hex_tile.ar.h"
#include "packages/tbs/model/hex_coord.h"

#include "packages/base/model/components/mesh_component.h"

namespace kryga
{
namespace tbs
{

// Represents a single hex tile in the game world
// Position is derived from hex coordinates
KRG_ar_class();
class hex_tile : public ::kryga::base::mesh_component
{
    KRG_gen_meta__hex_tile();

public:
    KRG_gen_class_meta(hex_tile, ::kryga::base::mesh_component);
    KRG_gen_construct_params
    {
        int hex_q = 0;
        int hex_r = 0;
        float hex_size = 1.0f;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);

    // Get hex coordinate
    hex_coord
    get_hex_coord() const
    {
        return hex_coord(m_hex_q, m_hex_r);
    }

    // Set hex coordinate and update world position
    void
    set_hex_coord(const hex_coord& coord);

    // Get hex size (center to vertex distance)
    float
    get_hex_size() const
    {
        return m_hex_size;
    }

    // Set hex size and update world position
    void
    set_hex_size(float size);

    // Recalculate world position from hex coordinates
    void
    update_world_position();

protected:
    KRG_ar_property("category=Hex", "serializable=true");
    int m_hex_q = 0;

    KRG_ar_property("category=Hex", "serializable=true");
    int m_hex_r = 0;

    KRG_ar_property("category=Hex", "serializable=true");
    float m_hex_size = 1.0f;
};

}  // namespace tbs
}  // namespace kryga
