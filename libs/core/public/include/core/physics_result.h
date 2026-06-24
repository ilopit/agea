#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>

namespace kryga
{
namespace core
{

// Upper bound on chunks reported for a single destructible in one result. The
// transforms ride INLINE in the POD result (see below), so this caps the message
// size — pick it generously above realistic fracture counts (voronoi default is 8
// cells). Exceeding it is a loud assert at publish, not silent truncation; raise
// this constant (and re-check the physics_results ring sizing) if a destructible
// legitimately needs more pieces.
constexpr uint32_t k_max_result_chunks = 64;

// Physics-emitted result, pushed onto subsystem_queues().physics.out by the
// physics thread (physics_command_processor) and drained on the model thread by
// physics_translator::drain_results. This is the read-back channel physics needs and
// audio/render do not: physics is bidirectional (the model both drives it and
// reads chunk transforms / broken state back), so a one-way command queue alone
// can't carry it.
//
// One result is published per broken destructible per physics step (unbroken
// handles publish nothing — the model already knows they're intact). Results are
// LATEST-WINS on the model side: drain keeps the newest per handle and dropping a
// superseded one on a full ring is harmless (the next step republishes).
//
// POD by design (crosses the physics->model boundary by value through an SPSC
// ring). The chunk transforms are an INLINE fixed-capacity array rather than a
// heap vector: it keeps the message trivially destructible (no RAII / owning
// pointer to free) AND removes per-step heap churn entirely — nothing is
// allocated or freed on the result path. The cost is the fixed k_max_result_chunks
// cap and a fatter ring slot; both are deliberate (see the constant above).
struct physics_result
{
    uint64_t handle = 0;
    bool broken = false;
    bool expired = false;

    // World-space transform per chunk, in chunk index order. Only [0, chunk_count)
    // is meaningful; valid only while broken.
    uint32_t chunk_count = 0;
    glm::mat4 chunk_transforms[k_max_result_chunks]{};
};

}  // namespace core
}  // namespace kryga
