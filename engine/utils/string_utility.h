#pragma once

#include <vector>
#include <string>

namespace agea
{
struct string_utils
{
    static std::vector<std::string> split(const std::string& s, const std::string& del);
    static std::string file_extention(const std::string& file_path);
    static bool convert_hext_string_to_bytes(size_t size, const char* s, uint8_t* ptr);
};
}  // namespace agea
