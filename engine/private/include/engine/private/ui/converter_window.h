#pragma once

#include "engine/ui.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kryga::ui
{

class converter_window : public window
{
public:
    static const char*
    window_title()
    {
        return "Asset Converter";
    }

    converter_window();
    ~converter_window();

    void
    handle() override;

private:
    std::array<char, 512> m_input_path;
    std::array<char, 512> m_output_path;
    std::array<char, 128> m_name;
    std::array<char, 128> m_existing_package;
    int m_mode_index = 0;
    std::string m_status;
    bool m_status_is_error = false;

    std::vector<std::pair<std::string, bool>> m_dep_checklist;
    bool m_deps_initialized = false;
    std::string m_last_scanned_output_root;

    struct async_state;
    std::unique_ptr<async_state> m_async;
};

}  // namespace kryga::ui
