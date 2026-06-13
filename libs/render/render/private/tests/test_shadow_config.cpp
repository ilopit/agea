#include <gtest/gtest.h>

#include "vulkan_render/render_config.h"

#include <gpu_types/gpu_shadow_types.h>

using namespace kryga::render;

class ShadowConfigTest : public ::testing::Test
{
protected:
    render_config cfg;

    void set_shadows(uint32_t atlas, uint32_t csm, uint32_t local, uint32_t cascades)
    {
        cfg.shadows.atlas_size = atlas;
        cfg.shadows.csm_tile_size = csm;
        cfg.shadows.local_tile_size = local;
        cfg.shadows.cascade_count = cascades;
    }
};

// --- Atlas size increase: tiles unchanged ---

TEST_F(ShadowConfigTest, atlas_increase_preserves_tiles)
{
    set_shadows(4096, 1024, 512, 4);
    cfg.validate();

    EXPECT_EQ(cfg.shadows.atlas_size, 4096u);
    EXPECT_EQ(cfg.shadows.csm_tile_size, 1024u);
    EXPECT_EQ(cfg.shadows.local_tile_size, 512u);
}

TEST_F(ShadowConfigTest, atlas_increase_from_2k_to_8k)
{
    set_shadows(2048, 512, 256, 4);
    cfg.validate();
    EXPECT_EQ(cfg.shadows.csm_tile_size, 512u);
    EXPECT_EQ(cfg.shadows.local_tile_size, 256u);

    cfg.shadows.atlas_size = 8192;
    cfg.validate();
    EXPECT_EQ(cfg.shadows.csm_tile_size, 512u);
    EXPECT_EQ(cfg.shadows.local_tile_size, 256u);
}

// --- Atlas size decrease: tiles scale down to fit ---

TEST_F(ShadowConfigTest, atlas_decrease_halves_csm)
{
    set_shadows(2048, 1024, 512, 4);
    cfg.validate();

    // 4 * 1024 = 4096 > 2048, csm shrinks first to preserve cascades
    // max_csm = min(2048/4, 2048-64) = min(512, 1984) = 512
    EXPECT_EQ(cfg.shadows.atlas_size, 2048u);
    EXPECT_EQ(cfg.shadows.csm_tile_size, 512u);
    EXPECT_EQ(cfg.shadows.cascade_count, 4u);
}

TEST_F(ShadowConfigTest, atlas_decrease_halves_local)
{
    set_shadows(2048, 512, 2048, 4);
    cfg.validate();

    // csm_tile = 512. The 16 local tiles (max_local_lights=8 * 2 hemispheres)
    // pack as a grid below the CSM row: at 256 cols=8, rows=2 -> 512+2*256=1024
    // fits; at 512 rows=4 -> 512+2048 > 2048 overflows. So 256.
    EXPECT_EQ(cfg.shadows.local_tile_size, 256u);
}

TEST_F(ShadowConfigTest, atlas_1024_4_cascades)
{
    set_shadows(1024, 1024, 512, 4);
    cfg.validate();

    // max_csm = min(1024/4, 1024-64) = min(256, 960) = 256
    // csm halves: 1024→512→256. 4*256=1024 OK
    EXPECT_EQ(cfg.shadows.csm_tile_size, 256u);
    EXPECT_EQ(cfg.shadows.cascade_count, 4u);
    // 16 local tiles below csm=256: at 128 cols=8, rows=2 -> 256+256=512 fits;
    // at 256 rows=4 -> 256+1024 > 1024 overflows. So 128.
    EXPECT_EQ(cfg.shadows.local_tile_size, 128u);
}

TEST_F(ShadowConfigTest, atlas_1024_2_cascades)
{
    set_shadows(1024, 1024, 512, 2);
    cfg.validate();

    // 2 * 1024 = 2048 > 1024, halve: 512. 2*512=1024 OK
    EXPECT_EQ(cfg.shadows.csm_tile_size, 512u);
    // 16 local tiles below csm=512: at 128 cols=8, rows=2 -> 512+256=768 fits;
    // at 256 rows=4 -> 512+1024 > 1024 overflows. So 128.
    EXPECT_EQ(cfg.shadows.local_tile_size, 128u);
}

TEST_F(ShadowConfigTest, atlas_1024_1_cascade)
{
    set_shadows(1024, 1024, 512, 1);
    cfg.validate();

    // CSM must leave room for at least minimum local tile
    // 1024 - 512 = 512 >= 64 (min), so csm = 512
    EXPECT_EQ(cfg.shadows.csm_tile_size, 512u);
    // 16 local tiles below csm=512 in a 1024 atlas -> max 128 (as 2-cascade case).
    EXPECT_EQ(cfg.shadows.local_tile_size, 128u);
}

// --- Cascade count drives CSM tile ---

TEST_F(ShadowConfigTest, cascade_count_1_max_csm)
{
    set_shadows(4096, 4096, 512, 1);
    cfg.validate();

    // CSM must leave room for at least min local tile (64)
    // 4096 - 64 = 4032, nearest pow2 <= 4032 = 2048
    EXPECT_EQ(cfg.shadows.csm_tile_size, 2048u);
    EXPECT_EQ(cfg.shadows.local_tile_size, 512u);
}

TEST_F(ShadowConfigTest, cascade_count_2_half_atlas)
{
    set_shadows(4096, 4096, 512, 2);
    cfg.validate();

    // 2 * 4096 = 8192 > 4096, halve: 2048. 2*2048=4096 OK
    EXPECT_EQ(cfg.shadows.csm_tile_size, 2048u);
    // remaining = 4096 - 2048 = 2048, local 512 OK
    EXPECT_EQ(cfg.shadows.local_tile_size, 512u);
}

TEST_F(ShadowConfigTest, cascade_count_4_quarter_atlas)
{
    set_shadows(4096, 4096, 512, 4);
    cfg.validate();

    // max_csm = min(4096/4, 4096-64) = min(1024, 4032) = 1024
    // csm halves: 4096→2048→1024. 4*1024=4096 OK, cascades preserved
    EXPECT_EQ(cfg.shadows.csm_tile_size, 1024u);
    EXPECT_EQ(cfg.shadows.cascade_count, 4u);
    // remaining = 4096 - 1024 = 3072, local 512 OK
    EXPECT_EQ(cfg.shadows.local_tile_size, 512u);
}

// --- Local tile constrained by remaining height ---

TEST_F(ShadowConfigTest, local_tile_shrinks_when_csm_large)
{
    set_shadows(4096, 2048, 4096, 2);
    cfg.validate();

    // 2 * 2048 = 4096 OK
    EXPECT_EQ(cfg.shadows.csm_tile_size, 2048u);
    // 16 local tiles below csm=2048 in 4096: at 512 cols=8, rows=2 ->
    // 2048+1024=3072 fits; at 1024 rows=4 -> 2048+4096 > 4096. So 512.
    EXPECT_EQ(cfg.shadows.local_tile_size, 512u);
}

TEST_F(ShadowConfigTest, local_tile_cascading_halves)
{
    set_shadows(2048, 1024, 2048, 2);
    cfg.validate();

    // 2 * 1024 = 2048 OK
    EXPECT_EQ(cfg.shadows.csm_tile_size, 1024u);
    // 16 local tiles below csm=1024 in 2048: at 256 cols=8, rows=2 ->
    // 1024+512=1536 fits; at 512 rows=4 -> 1024+2048 > 2048. So 256.
    EXPECT_EQ(cfg.shadows.local_tile_size, 256u);
}

// --- Non-power-of-two inputs get rounded ---

TEST_F(ShadowConfigTest, non_pow2_atlas_rounded)
{
    set_shadows(3000, 512, 256, 4);
    cfg.validate();

    // 3000 rounds to nearest pow2: 2048 or 4096
    EXPECT_TRUE(cfg.shadows.atlas_size == 2048u || cfg.shadows.atlas_size == 4096u);
    // tiles still valid
    EXPECT_LE(cfg.shadows.csm_tile_size * cfg.shadows.cascade_count, cfg.shadows.atlas_size);
}

TEST_F(ShadowConfigTest, non_pow2_csm_rounded)
{
    set_shadows(4096, 700, 256, 4);
    cfg.validate();

    // 700 rounds to 512 or 1024 (nearest pow2), then must fit 4x in 4096
    EXPECT_EQ(cfg.shadows.csm_tile_size & (cfg.shadows.csm_tile_size - 1), 0u);
    EXPECT_LE(cfg.shadows.csm_tile_size * 4, cfg.shadows.atlas_size);
}

// --- Invariants hold for all valid combinations ---

TEST_F(ShadowConfigTest, invariants_always_hold)
{
    uint32_t atlas_sizes[] = {1024, 2048, 4096, 8192};
    uint32_t tile_sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
    uint32_t cascade_counts[] = {1, 2, 3, 4};

    for (auto atlas : atlas_sizes)
    {
        for (auto csm : tile_sizes)
        {
            for (auto local : tile_sizes)
            {
                for (auto cascades : cascade_counts)
                {
                    set_shadows(atlas, csm, local, cascades);
                    cfg.validate();

                    // CSM row fits horizontally
                    EXPECT_LE(cfg.shadows.csm_tile_size * cfg.shadows.cascade_count,
                              cfg.shadows.atlas_size)
                        << "atlas=" << atlas << " csm=" << csm << " local=" << local
                        << " cascades=" << cascades;

                    // Local fits in remaining height
                    uint32_t remaining = cfg.shadows.atlas_size - cfg.shadows.csm_tile_size;
                    EXPECT_LE(cfg.shadows.local_tile_size, remaining)
                        << "atlas=" << atlas << " csm=" << csm << " local=" << local
                        << " cascades=" << cascades;

                    // All are power of 2
                    EXPECT_EQ(cfg.shadows.atlas_size & (cfg.shadows.atlas_size - 1), 0u);
                    EXPECT_EQ(cfg.shadows.csm_tile_size & (cfg.shadows.csm_tile_size - 1), 0u);
                    EXPECT_EQ(cfg.shadows.local_tile_size & (cfg.shadows.local_tile_size - 1), 0u);

                    // Minimums respected
                    EXPECT_GE(cfg.shadows.csm_tile_size, (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN);
                    EXPECT_GE(cfg.shadows.local_tile_size, (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN);
                }
            }
        }
    }
}

// --- Edge cases ---

TEST_F(ShadowConfigTest, minimum_atlas_maximum_cascades)
{
    set_shadows(1024, 4096, 4096, 4);
    cfg.validate();

    EXPECT_EQ(cfg.shadows.atlas_size, 1024u);
    // csm shrinks first: max_csm = min(1024/4, 1024-64) = 256
    EXPECT_EQ(cfg.shadows.csm_tile_size, 256u);
    EXPECT_EQ(cfg.shadows.cascade_count, 4u);
    uint32_t remaining = cfg.shadows.atlas_size - cfg.shadows.csm_tile_size;
    EXPECT_LE(cfg.shadows.local_tile_size, remaining);
}

TEST_F(ShadowConfigTest, csm_already_at_minimum)
{
    set_shadows(1024, 64, 64, 4);
    cfg.validate();

    EXPECT_EQ(cfg.shadows.csm_tile_size, 64u);
    EXPECT_EQ(cfg.shadows.local_tile_size, 64u);
}

// --- Limit query methods ---

TEST_F(ShadowConfigTest, max_cascades_query)
{
    set_shadows(4096, 1024, 512, 4);
    EXPECT_EQ(cfg.shadows.max_cascades(), 4u);

    cfg.shadows.csm_tile_size = 2048;
    EXPECT_EQ(cfg.shadows.max_cascades(), 2u);

    cfg.shadows.atlas_size = 8192;
    EXPECT_EQ(cfg.shadows.max_cascades(), 4u); // capped at KGPU_CSM_CASCADE_COUNT_MAX
}

TEST_F(ShadowConfigTest, max_csm_tile_query)
{
    set_shadows(4096, 1024, 512, 4);
    // max_csm = min(4096/4, 4096-64) = min(1024, 4032) = 1024
    EXPECT_EQ(cfg.shadows.max_csm_tile(), 1024u);

    cfg.shadows.cascade_count = 2;
    // max_csm = min(4096/2, 4096-64) = min(2048, 4032) = 2048
    EXPECT_EQ(cfg.shadows.max_csm_tile(), 2048u);

    cfg.shadows.cascade_count = 1;
    // max_csm = min(4096/1, 4096-64) = min(4096, 4032) = 4032 → pow2 = 2048
    EXPECT_EQ(cfg.shadows.max_csm_tile(), 2048u);
}

TEST_F(ShadowConfigTest, max_local_tile_query)
{
    set_shadows(4096, 1024, 512, 4);
    // 16 local tiles (max_local_lights=8 * 2) pack as a grid below the CSM row.
    // Largest tile whose full grid fits: 512 (cols=8, rows=2 -> 1024+1024=2048
    // <= 4096); 1024 -> rows=4 -> 1024+4096 > 4096.
    EXPECT_EQ(cfg.shadows.max_local_tile(), 512u);

    cfg.shadows.csm_tile_size = 2048;
    // 512: 2048+1024=3072 fits; 1024: 2048+4096 > 4096. Still 512.
    EXPECT_EQ(cfg.shadows.max_local_tile(), 512u);

    cfg.shadows.csm_tile_size = 256;
    // 512: 256+1024=1280 fits; 1024: 256+4096 > 4096. Still 512.
    EXPECT_EQ(cfg.shadows.max_local_tile(), 512u);
}

TEST_F(ShadowConfigTest, max_local_lights_clamped)
{
    set_shadows(4096, 1024, 512, 4);
    cfg.shadows.max_local_lights = 4;
    cfg.validate();
    EXPECT_EQ(cfg.shadows.max_local_lights, 4u);

    cfg.shadows.max_local_lights = 0;
    cfg.validate();
    EXPECT_EQ(cfg.shadows.max_local_lights, 0u);

    cfg.shadows.max_local_lights = 99;
    cfg.validate();
    EXPECT_EQ(cfg.shadows.max_local_lights, (uint32_t)KGPU_MAX_SHADOWED_LOCAL_LIGHTS);
}

TEST_F(ShadowConfigTest, limit_queries_consistent_with_validate)
{
    uint32_t atlas_sizes[] = {1024, 2048, 4096, 8192};
    uint32_t tile_sizes[] = {64, 128, 256, 512, 1024, 2048};
    uint32_t cascade_counts[] = {1, 2, 3, 4};

    for (auto atlas : atlas_sizes)
    {
        for (auto csm : tile_sizes)
        {
            for (auto cascades : cascade_counts)
            {
                set_shadows(atlas, csm, 512, cascades);
                cfg.validate();

                // After validate, values must be within the limits
                EXPECT_LE(cfg.shadows.cascade_count, cfg.shadows.max_cascades());
                EXPECT_LE(cfg.shadows.csm_tile_size, cfg.shadows.max_csm_tile());
                EXPECT_LE(cfg.shadows.local_tile_size, cfg.shadows.max_local_tile());
            }
        }
    }
}
