#include "utils/file_utils.h"

#include <fstream>

namespace agea
{
bool
file_utils::load_file(const std::string& str, std::vector<char>& blob)
{
    std::ifstream file(str, std::ios_base::binary | std::ios_base::ate);

    if (!file.is_open())
    {
        return false;
    }
    size_t size = file.tellg();
    blob.resize(size);

    file.seekg(0);

    file.read(blob.data(), size);

    return true;
}

bool
file_utils::compare_files(const std::string& lpath, const std::string& rpath)
{
    std::vector<char> l, r;

    if (!load_file(lpath, l))
    {
        return false;
    }

    if (!load_file(rpath, r))
    {
        return false;
    }

    return l == r;
}

}  // namespace agea
