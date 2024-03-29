#pragma once

#include "utils/path.h"

#include <vector>
#include <string>

namespace agea
{
namespace utils
{

struct file_utils
{
    static bool
    load_file(const path& file, std::vector<uint8_t>& blob);

    static bool
    save_file(const path& file, const std::vector<uint8_t>& blob);

    static bool
    compare_files(const path& a, const path& b);

    static bool
    compare_folders(const path& a, const path& b);
};
}  // namespace utils
}  // namespace agea
