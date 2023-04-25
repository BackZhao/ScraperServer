include_guard()

# cmake-format: off

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR loongarch64)

set(CMAKE_C_COMPILER       loongarch64-unknown-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER     loongarch64-unknown-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_BUILD_WITH_INSTALL_RPATH    ON)

# cmake-format: on
