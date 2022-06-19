#pragma once

#include <vector>
#include <string>

namespace agea
{
struct file_utils
{
    static bool
    load_file(const std::string& file, std::vector<char>& blob);

    static bool
    compare_files(const std::string& a, const std::string& b);

    static bool
    compare_folders(const std::string& a, const std::string& b);
};
}  // namespace agea
