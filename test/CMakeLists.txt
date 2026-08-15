find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(NAME test_mksfoex
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_mksfoex.py $<TARGET_FILE:vita-mksfoex>
)

add_test(NAME test_pack_vpk
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_pack_vpk.py $<TARGET_FILE:vita-pack-vpk>
)

add_test(NAME test_libs_gen_2
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_libs_gen_2.py $<TARGET_FILE:vita-libs-gen-2>
)

add_test(NAME test_elf_create
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_elf_create.py $<TARGET_FILE:vita-elf-create>
)

add_test(NAME test_make_fself
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_make_fself.py $<TARGET_FILE:vita-make-fself> $<TARGET_FILE:psp2rela>
)

add_test(NAME test_cmake_sourcepath
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_cmake_sourcepath.py ${CMAKE_SOURCE_DIR}/cmake_toolchain/vita.cmake
)

add_test(NAME test_cmake_secretsafe
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_cmake_secretsafe.py ${CMAKE_SOURCE_DIR}/cmake_toolchain/vita.cmake
)
