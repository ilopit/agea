#pragma once

#include <utils/id.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace animation
{

struct blend_layer
{
    utils::id clip_id;
    float weight = 1.0f;
};

struct ik_two_bone_params
{
    int32_t start_joint = -1;
    int32_t mid_joint = -1;
    int32_t end_joint = -1;
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 pole_vector = glm::vec3(0.0f, 1.0f, 0.0f);
    float weight = 1.0f;
    float soften = 1.0f;
};

struct ik_aim_params
{
    int32_t joint = -1;
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 forward = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float weight = 1.0f;
};

}  // namespace animation
}  // namespace kryga
