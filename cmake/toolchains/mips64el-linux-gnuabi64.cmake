include_guard()

# cmake-format: off

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR mips64el)

set(CMAKE_C_COMPILER       mips64el-linux-gnuabi64-gcc)
set(CMAKE_CXX_COMPILER     mips64el-linux-gnuabi64-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_BUILD_WITH_INSTALL_RPATH    ON)

# cmake-format: on
