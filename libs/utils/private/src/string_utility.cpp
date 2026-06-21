#include "utils/string_utility.h"

namespace kryga
{
std::vector<std::string>
string_utils::split(const std::string& s, const std::string& del)
{
    std::vector<std::string> result;
    split(s, del, result);

    return result;
}

void
string_utils::split(const std::string& s, const std::string& del, std::vector<std::string>& result)
{
    size_t start = 0, end = 0;

    while (end != std::string::npos)
    {
        end = s.find(del, start);

        auto token = s.substr(start, (end == std::string::npos) ? std::string::npos : end - start);

        if (!token.empty())
        {
            result.push_back(token);
        }

        start = ((end > (std::string::npos - del.size())) ? std::string::npos : end + del.size());
    }
}

bool
string_utils::ends_with(const std::string& src, const std::string& ending)
{
    auto sitr = src.rbegin();
    auto eitr = ending.rbegin();

    while (eitr != ending.rend() && sitr != src.rend())
    {
        if (*sitr != *eitr)
        {
            return false;
        }
        ++eitr;
        ++sitr;
    }

    return eitr != ending.rbegin();
}

bool
string_utils::starts_with(const std::string& src, const std::string& begin)
{
    auto sitr = src.begin();
    auto eitr = begin.begin();

    while (eitr != begin.end() && sitr != src.end())
    {
        if (*sitr != *eitr)
        {
            return false;
        }
        ++eitr;
        ++sitr;
    }

    return eitr == begin.end();
}

}  // namespace kryga
