#include "utils/string_utility.h"

namespace agea
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
    return;
}

std::string
string_utils::file_extention(const std::string& file_path)
{
    auto pos = file_path.rfind('.');

    return file_path.substr(pos);
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

namespace
{
int
char_to_int(char c)
{
    switch (c)
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'a':
    case 'A':
        return 10;
    case 'b':
    case 'B':
        return 11;
    case 'c':
    case 'C':
        return 12;
    case 'd':
    case 'D':
        return 13;
    case 'e':
    case 'E':
        return 14;
    case 'f':
    case 'F':
        return 15;
    default:
        return -1;
    }
}
}  // namespace

bool
string_utils::convert_hext_string_to_bytes(size_t size, const char* s, uint8_t* ptr)
{
    if (size == 0 || (size & 1))
    {
        return false;
    }

    for (int i = 0; i < size; i += 2)
    {
        auto a = char_to_int(s[i]);
        if (a < 0)
        {
            return false;
        }

        auto b = char_to_int(s[i + 1]);
        if (b < 0)
        {
            return false;
        }

        ptr[i / 2] = (uint8_t)(a << 4) | (uint8_t)b;
    }
    return true;
}

}  // namespace agea
