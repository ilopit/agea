#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kryga
{

std::string
base64_encode(const uint8_t* data, size_t len);

std::string
base64_encode(const std::vector<uint8_t>& data);

}  // namespace kryga
