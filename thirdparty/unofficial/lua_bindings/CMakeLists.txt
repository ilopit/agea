add_library(sol2_unofficial INTERFACE)

add_library(agea::sol2_unofficial ALIAS sol2_unofficial)

target_include_directories(sol2_unofficial INTERFACE ".")
target_link_libraries(sol2_unofficial INTERFACE sol2::sol2)