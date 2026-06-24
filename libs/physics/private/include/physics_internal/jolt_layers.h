#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace kryga::physics::jolt_layers
{

// Object layers
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_OBJECT_LAYERS = 2;

// Broad phase layers
namespace bp
{
static constexpr JPH::BroadPhaseLayer NON_MOVING{0};
static constexpr JPH::BroadPhaseLayer MOVING{1};
static constexpr unsigned int NUM_LAYERS = 2;
}  // namespace bp

class bp_layer_interface_impl final : public JPH::BroadPhaseLayerInterface
{
public:
    bp_layer_interface_impl()
    {
        m_object_to_broad_phase[NON_MOVING] = bp::NON_MOVING;
        m_object_to_broad_phase[MOVING] = bp::MOVING;
    }

    unsigned int
    GetNumBroadPhaseLayers() const override
    {
        return bp::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer
    GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        return m_object_to_broad_phase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char*
    GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        if (static_cast<JPH::BroadPhaseLayer::Type>(layer) ==
            static_cast<JPH::BroadPhaseLayer::Type>(bp::NON_MOVING))
        {
            return "NON_MOVING";
        }
        return "MOVING";
    }
#endif

private:
    JPH::BroadPhaseLayer m_object_to_broad_phase[NUM_OBJECT_LAYERS]{};
};

class object_vs_bp_filter_impl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool
    ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override
    {
        switch (a)
        {
        case NON_MOVING:
            return b == bp::MOVING;
        case MOVING:
            return true;
        default:
            return false;
        }
    }
};

class object_layer_pair_filter_impl final : public JPH::ObjectLayerPairFilter
{
public:
    bool
    ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
    {
        switch (a)
        {
        case NON_MOVING:
            return b == MOVING;
        case MOVING:
            return true;
        default:
            return false;
        }
    }
};

}  // namespace kryga::physics::jolt_layers
