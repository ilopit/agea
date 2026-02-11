#pragma once

#include <animation/animation_instance.h>

#include <glm_unofficial/glm.h>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/maths/soa_transform.h>

#include <utils/id.h>

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

class animation_system
{
public:
    animation_system();
    ~animation_system();

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
                    render::vulkan_render_data* render_data);

    void
    destroy_instance(const utils::id& instance_id);

    void
    set_blend_layers(const utils::id& instance_id,
                     const std::vector<blend_layer>& layers);

    void
    set_ik_two_bone(const utils::id& instance_id,
                    const ik_two_bone_params& params);

    void
    set_ik_aim(const utils::id& instance_id,
               const ik_aim_params& params);

    void
    clear_ik(const utils::id& instance_id);

    void
    tick(float dt);

    const ozz::animation::Skeleton*
    get_skeleton(const utils::id& id) const;

    int32_t
    get_joint_count(const utils::id& skel_id) const;

    bool
    has_animation(const utils::id& skel_id, const utils::id& anim_id) const;

private:
    struct registered_skeleton
    {
        ozz::animation::Skeleton skeleton;
        std::vector<glm::mat4> inverse_bind_matrices;
        std::vector<int32_t> joint_remaps;
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
};

}  // namespace animation
}  // namespace kryga
