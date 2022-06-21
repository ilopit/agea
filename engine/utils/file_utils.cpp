#include "utils/file_utils.h"

#include <fstream>
#include <filesystem>
#include <set>
#include "utils/agea_log.h"

namespace agea
{
namespace utils
{

bool
file_utils::load_file(const utils::path& str, std::vector<char>& blob)
{
    std::ifstream file(str.fs(), std::ios_base::binary | std::ios_base::ate);

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
file_utils::compare_files(const utils::path& lpath, const utils::path& rpath)
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

    if (l != r)
    {
        ALOG_INFO("{0} {1} are different", lpath.str(), rpath.str());
        return false;
    }

    return true;
}

namespace
{
std::set<std::string>
get_files(const path& path)
{
    std::set<std::string> files;
    for (auto& resource : std::filesystem::recursive_directory_iterator(path.fs()))
    {
        if (resource.is_directory())
        {
            continue;
        }

        auto rel = std::filesystem::relative(resource, path.fs());

        files.insert(rel.generic_string());
    }
    return files;
}

}  // namespace

bool
file_utils::compare_folders(const utils::path& a, const utils::path& b)
{
    auto left_files = get_files(a);
    auto right_files = get_files(b);

    if (left_files != right_files)
    {
        return false;
    }

    for (auto& f : left_files)
    {
        auto left = a;
        left.append(f);
        auto right = a;
        right.append(f);

        if (!compare_files(left, right))
        {
            return false;
        }
    }

    return true;
}
}  // namespace utils
}  // namespace agea
