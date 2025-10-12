#pragma once

#include "packages/root/model/core_types/vec2.h"
#include "packages/root/model/core_types/vec3.h"
#include "packages/root/model/core_types/vec4.h"
#include "packages/root/model/core_types/color.h"

#include <error_handling/error_handling.h>

#include <resource_locator/resource_locator.h>

#include <utils/check.h>
#include <utils/defines_utils.h>
#include <utils/path.h>
#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <glm_unofficial/glm.h>

#include <array>
#include <string>
#include <stdint.h>
#include <memory>
#include <unordered_map>

namespace agea
{
using blob_ptr = uint8_t*;
}  // namespace agea
