#pragma once

#include <vector>
#include <string>

#include "utils/path.h"

namespace agea
{
namespace utils
{

struct file_utils
{
    static bool
    load_file(const path& file, std::vector<char>& blob);

    static bool
    compare_files(const path& a, const path& b);

    static bool
    compare_folders(const path& a, const path& b);
};
}  // namespace utils
}  // namespace agea
