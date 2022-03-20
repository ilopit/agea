#include "string_utility.h"

namespace agea
{
std::vector<std::string>
string_utils::split(const std::string& s, const std::string& del)
{
    std::vector<std::string> result;

    size_t start = 0, end = 0;

    while (end != std::string::npos)
    {
        end = s.find(del, start);

        result.push_back(
            s.substr(start, (end == std::string::npos) ? std::string::npos : end - start));

        start = ((end > (std::string::npos - del.size())) ? std::string::npos : end + del.size());
    }

    return result;
}

std::string
string_utils::file_extention(const std::string& file_path)
{
    auto pos = file_path.rfind('.');

    return file_path.substr(pos);
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

        ptr[i / 2] = (a << 4) | b;
    }
    return true;
}

}  // namespace agea
