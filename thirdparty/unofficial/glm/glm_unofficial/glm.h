#pragma once

// GLM_FORCE_RADIANS, GLM_FORCE_DEPTH_ZERO_TO_ONE, GLM_ENABLE_EXPERIMENTAL
// are set via target_compile_definitions on gpu_types (propagated PUBLIC).

#pragma warning(push)
#pragma warning(disable : 4201)

#include <glm/glm.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#pragma warning(pop)