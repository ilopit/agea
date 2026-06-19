#pragma once

#include <vector>
#include <string>

namespace kryga
{
struct string_utils
{
    static void
    split(const std::string& s, const std::string& del, std::vector<std::string>& result);

    static std::vector<std::string>
    split(const std::string& s, const std::string& del);

    static bool
    ends_with(const std::string& src, const std::string& ending);

    static bool
    starts_with(const std::string& src, const std::string& begin);
};
}  // namespace kryga
