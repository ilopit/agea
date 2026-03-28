FetchContent_GetProperties(stb)

add_library(stb_unofficial STATIC stb_unofficial/stb.cpp)

target_include_directories(stb_unofficial PUBLIC ".")
target_include_directories(stb_unofficial PUBLIC "${stb_SOURCE_DIR}" )

add_library(kryga::stb_unofficial ALIAS stb_unofficial)