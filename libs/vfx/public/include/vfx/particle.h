#pragma once

#include <glm_unofficial/glm.h>

namespace kryga
{
namespace vfx
{

struct particle
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec4 color{1.0f};
    float size = 1.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
};

}  // namespace vfx
}  // namespace kryga
