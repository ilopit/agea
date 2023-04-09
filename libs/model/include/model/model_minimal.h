#pragma once

#include "core_types/vec2.h"
#include "core_types/vec3.h"
#include "core_types/vec4.h"
#include "core_types/color.h"

#include <utils/check.h>
#include <utils/defines_utils.h>
#include <utils/path.h>
#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <resource_locator/resource_locator.h>

#include <glm_unofficial/glm.h>

#include <array>
#include <string>
#include <stdint.h>
#include <memory>
#include <unordered_map>

namespace agea
{
using blob_ptr = uint8_t*;

enum class result_code
{
    nav = 0,
    ok,
    failed,
    proto_doesnt_exist,
    doesnt_exist,
    serialization_error,
    path_not_found,
    id_not_found
};

}  // namespace agea
