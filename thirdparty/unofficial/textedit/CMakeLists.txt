add_library(textedit_unofficial STATIC)

set(textedit_prefix ${PROJECT_SOURCE_DIR}/thirdparty/upstream)

target_include_directories(textedit_unofficial PUBLIC "${textedit_prefix}/textedit")

target_sources(textedit_unofficial PRIVATE 
    "${textedit_prefix}/textedit/TextEditor.h"
    "${textedit_prefix}/textedit/TextEditor.cpp"
    "${textedit_prefix}/textedit/UnitTests.cpp"
    "${textedit_prefix}/textedit/ImGuiDebugPanel.cpp"
    "${textedit_prefix}/textedit/LanguageDefinitions.cpp"
)
target_link_libraries(textedit_unofficial agea::imgui_unofficial)
add_library(agea::textedit_unofficial ALIAS textedit_unofficial)