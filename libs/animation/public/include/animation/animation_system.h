#pragma once

#include <animation/animation_instance.h>

#include <global_state/system.h>

#include <glm_unofficial/glm.h>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/maths/soa_transform.h>

#include <utils/id.h>
#include <render_types/render_handle.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace kryga
{

namespace render
{
class vulkan_render;
class vulkan_render_data;
}  // namespace render

namespace animation
{

class animation_system : public gs::system
{
public:
    using render_data_resolver =
        std::function<render::vulkan_render_data*(render::types::render_object_handle)>;

    std::string_view
    name() const override
    {
        return "animation";
    }
    std::span<const std::string_view>
    deps() const override
    {
        static constexpr std::string_view d[] = {"render"};
        return d;
    }
    void
    on_connect(gs::state&) override;

    animation_system();
    ~animation_system() override;

    void
    set_render_data_resolver(render_data_resolver fn)
    {
        m_resolver = std::move(fn);
    }

    void
    register_skeleton(const utils::id& id,
                      ozz::animation::Skeleton&& skel,
                      std::vector<glm::mat4> inverse_bind_matrices,
                      std::vector<int32_t> joint_remaps);

    void
    register_animation(const utils::id& skel_id,
                       const utils::id& anim_id,
                       ozz::animation::Animation&& anim);

    utils::id
    create_instance(const utils::id& instance_id,
                    const utils::id& skeleton_id,
                    const utils::id& clip_id,
                    render::types::render_object_handle render_handle);

    void
    destroy_instance(const utils::id& instance_id);

    void
    tick(float dt);

    const ozz::animation::Skeleton*
    get_skeleton(const utils::id& id) const;

    bool
    has_animation(const utils::id& skel_id, const utils::id& anim_id) const;

    bool
    has_skinned_mesh(const utils::id& skel_id) const;

    void
    set_skinned_mesh_created(const utils::id& skel_id);

    // Dedup home for the shared skinned-mesh render slot (handle model). The
    // skinned mesh is created once per skeleton; the handle lives here so every
    // animated instance sharing the skeleton draws by the same U64 handle.
    void
    set_skinned_mesh_handle(const utils::id& skel_id, render::types::mesh_handle h);

    render::types::mesh_handle
    get_skinned_mesh_handle(const utils::id& skel_id) const;

private:
    struct registered_skeleton
    {
        ozz::animation::Skeleton skeleton;
        std::vector<glm::mat4> inverse_bind_matrices;
        std::vector<int32_t> joint_remaps;
        bool skinned_mesh_created = false;
        render::types::mesh_handle skinned_mesh_handle;  // shared render slot for this skeleton
    };

    struct layer_state
    {
        utils::id clip_id;
        float weight = 1.0f;
        float playback_time = 0.0f;
        std::unique_ptr<ozz::animation::SamplingJob::Context> context;
        std::vector<ozz::math::SoaTransform> locals;
    };

    struct instance_data
    {
        utils::id skeleton_id;
        float playback_speed = 1.0f;
        bool looping = true;
        bool playing = true;

        std::vector<layer_state> layers;
        std::vector<glm::mat4> bone_matrices;
        uint32_t bone_offset = 0;

        // Render-object slot this instance writes bone state into. The raw pointer
        // is a lazily-resolved cache of the handle (see tick); the handle is the
        // stable key that survives the reserved->resident window.
        render::types::render_object_handle render_handle;
        render::vulkan_render_data* render_data = nullptr;

        // IK
        bool has_ik_two_bone = false;
        ik_two_bone_params ik_two_bone;
        bool has_ik_aim = false;
        ik_aim_params ik_aim;
    };

    std::unordered_map<utils::id, registered_skeleton> m_skeletons;
    std::unordered_map<utils::id, std::unordered_map<utils::id, ozz::animation::Animation>>
        m_animations;
    std::unordered_map<utils::id, instance_data> m_instances;
    render_data_resolver m_resolver;
};

}  // namespace animation
}  // namespace kryga
