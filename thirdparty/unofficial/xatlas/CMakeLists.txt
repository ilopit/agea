FetchContent_GetProperties(xatlas)

add_library(xatlas_unofficial STATIC
    "${xatlas_SOURCE_DIR}/source/xatlas/xatlas.cpp"
)

target_include_directories(xatlas_unofficial PUBLIC "${xatlas_SOURCE_DIR}/source")

# Suppress warnings in thirdparty code
if(MSVC)
    target_compile_options(xatlas_unofficial PRIVATE /W0)
else()
    target_compile_options(xatlas_unofficial PRIVATE -w)
endif()

add_library(kryga::xatlas_unofficial ALIAS xatlas_unofficial)
