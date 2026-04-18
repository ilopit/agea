FetchContent_GetProperties(textedit)

add_library(textedit_unofficial STATIC)

target_include_directories(textedit_unofficial PUBLIC "${textedit_SOURCE_DIR}")

target_sources(textedit_unofficial PRIVATE
    "${textedit_SOURCE_DIR}/TextEditor.h"
    "${textedit_SOURCE_DIR}/TextEditor.cpp"
    "${textedit_SOURCE_DIR}/UnitTests.cpp"
    "${textedit_SOURCE_DIR}/ImGuiDebugPanel.cpp"
    "${textedit_SOURCE_DIR}/LanguageDefinitions.cpp"
)
target_include_directories(textedit_unofficial PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    set(_textedit_force_include "/FI\"imgui_compat.h\"")
else()
    set(_textedit_force_include "-include imgui_compat.h")
endif()
set_source_files_properties(
    "${textedit_SOURCE_DIR}/TextEditor.cpp"
    "${textedit_SOURCE_DIR}/UnitTests.cpp"
    "${textedit_SOURCE_DIR}/ImGuiDebugPanel.cpp"
    "${textedit_SOURCE_DIR}/LanguageDefinitions.cpp"
    PROPERTIES COMPILE_FLAGS "${_textedit_force_include}"
)
target_link_libraries(textedit_unofficial kryga::imgui_unofficial)
add_library(kryga::textedit_unofficial ALIAS textedit_unofficial)