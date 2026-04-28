add_executable(kryga_cook
    main.cpp
)

target_link_libraries(kryga_cook PRIVATE
    kryga::cook
    kryga::utils
)

kryga_finalize_executable(kryga_cook)
kryga_ide_path(kryga_cook)
