#pragma once

#include <asset_converter/converter_context.h>

#include <string>

namespace kryga::converter
{

bool
parse_gltf(const std::string& path, parsed_scene& out_scene);

}  // namespace kryga::converter
