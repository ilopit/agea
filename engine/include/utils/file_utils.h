#pragma once

#include <vector>
#include <string>

namespace agea
{
struct file_utils
{
    static bool
    load_file(const std::string& file, std::vector<char>& blob);
};
}  // namespace agea
