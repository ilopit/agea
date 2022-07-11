add_library(stb_unofficial STATIC stb_unofficial/stb.cpp)

target_include_directories(stb_unofficial PUBLIC ".")
target_include_directories(stb_unofficial PUBLIC "${PROJECT_SOURCE_DIR}/thirdparty/upstream/stb" )