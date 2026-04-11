#pragma once

#include <cstdint>
#include <yaml-cpp/yaml.h>

namespace kryga
{
namespace core
{

union object_layer_flags
{
    struct
    {
        bool visible : 1;        // bit 0 — object is rendered
        bool editor_only : 1;    // bit 1 — editor-only, unlit, no shadows, stripped in play
        bool cast_shadows : 1;   // bit 2 — included in shadow passes
        bool receive_light : 1;  // bit 3 — affected by realtime lighting
        bool contribute_gi : 1;  // bit 4 — included in lightmap baking
        bool static_object : 1;  // bit 5 — eligible for static batching
    };
    uint32_t bits;

    object_layer_flags()
        : bits(0)
    {
    }

    explicit object_layer_flags(uint32_t b)
        : bits(b)
    {
    }

    bool
    operator==(const object_layer_flags& o) const
    {
        return bits == o.bits;
    }

    bool
    operator!=(const object_layer_flags& o) const
    {
        return bits != o.bits;
    }

    // --- Presets ---

    // Walls, floors, static props — lit, shadowed, baked, batched
    static object_layer_flags
    default_static()
    {
        object_layer_flags m;
        m.visible = true;
        m.cast_shadows = true;
        m.receive_light = true;
        m.contribute_gi = true;
        m.static_object = true;
        return m;
    }

    // Characters, moving props — lit, shadowed, no bake, no batching
    static object_layer_flags
    default_dynamic()
    {
        object_layer_flags m;
        m.visible = true;
        m.cast_shadows = true;
        m.receive_light = true;
        return m;
    }

    // Light icons, probe markers, gizmos — visible in editor only, unlit
    static object_layer_flags
    editor_gizmo()
    {
        object_layer_flags m;
        m.visible = true;
        m.editor_only = true;
        return m;
    }

    // Skybox, background — visible, lit, no shadows, no bake
    static object_layer_flags
    background()
    {
        object_layer_flags m;
        m.visible = true;
        m.receive_light = true;
        return m;
    }

    // Triggers, volumes, collision shapes — invisible at runtime
    static object_layer_flags
    hidden()
    {
        return object_layer_flags(0);
    }

    // Decals, glass — visible, lit, no shadow casting, not baked
    static object_layer_flags
    transparent()
    {
        object_layer_flags m;
        m.visible = true;
        m.receive_light = true;
        return m;
    }

    // Shadow-only proxy — casts shadows but not rendered
    static object_layer_flags
    shadow_proxy()
    {
        object_layer_flags m;
        m.cast_shadows = true;
        return m;
    }
};

static_assert(sizeof(object_layer_flags) == sizeof(uint32_t), "object_layer_flags must be 4 bytes");

}  // namespace core
}  // namespace kryga

namespace YAML
{
template <>
struct convert<kryga::core::object_layer_flags>
{
    static Node
    encode(const kryga::core::object_layer_flags& m)
    {
        return Node(m.bits);
    }

    static bool
    decode(const Node& node, kryga::core::object_layer_flags& m)
    {
        m.bits = node.as<uint32_t>(0xFFFFFFFFu);
        return true;
    }
};
}  // namespace YAML
