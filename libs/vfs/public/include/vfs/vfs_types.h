#pragma once

#include <utils/path.h>

#include <filesystem>

namespace kryga
{

class temp_dir_context
{
public:
    temp_dir_context() = default;
    ~temp_dir_context();

    temp_dir_context(utils::path s)
        : m_folder(std::move(s))
    {
    }

    temp_dir_context(temp_dir_context&& other) noexcept
        : m_folder(std::move(other.m_folder))
    {
    }

    temp_dir_context&
    operator=(temp_dir_context&& other) noexcept
    {
        if (this != &other)
        {
            m_folder = std::move(other.m_folder);
        }

        return *this;
    }

    const utils::path&
    folder() const
    {
        return m_folder;
    }

private:
    utils::path m_folder;
};

}  // namespace kryga
