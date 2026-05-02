file(GLOB LIB_SRC "public/include/project_paths/*.h" "private/src/*.cpp")

add_library(project_paths STATIC
   ${LIB_SRC}
)

target_link_libraries(project_paths PUBLIC kryga::utils)

kryga_finalize_library(project_paths)
