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
target_link_libraries(textedit_unofficial kryga::imgui_unofficial)
add_library(kryga::textedit_unofficial ALIAS textedit_unofficial)