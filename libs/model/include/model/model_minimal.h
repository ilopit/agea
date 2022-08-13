#pragma once

#include <utils/check.h>
#include <utils/defines_utils.h>
#include <utils/path.h>
#include <utils/id.h>

#include <resource_locator/resource_locator.h>

#include <glm_unofficial/glm.h>
#include "core_types/vec3.h"

#include <string>
#include <stdint.h>
#include <memory>
#include <unordered_map>

namespace agea
{
using blob_ptr = uint8_t*;
using fixed_size_buffer = std::array<char, 128>;
}  // namespace agea
