#pragma once

#include <vector>
#include <string>

namespace agea
{
struct string_utils
{
    static void
    split(const std::string& s, const std::string& del, std::vector<std::string>& result);

    static std::vector<std::string>
    split(const std::string& s, const std::string& del);

    static std::string
    file_extention(const std::string& file_path);

    static bool
    ends_with(const std::string& src, const std::string& ending);

    static bool
    starts_with(const std::string& src, const std::string& begin);

    static bool
    convert_hext_string_to_bytes(size_t size, const char* s, uint8_t* ptr);
};
}  // namespace agea
