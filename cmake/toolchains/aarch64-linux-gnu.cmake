include_guard()

get_filename_component(target_triple ${CMAKE_CURRENT_LIST_FILE} NAME_WE)

if(NOT CMAKE_SYSTEM_NAME)
  set(CMAKE_SYSTEM_NAME Linux)
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR)
  string(REGEX MATCH "^[a-zA-Z0-9]+" CMAKE_SYSTEM_PROCESSOR ${target_triple})
  if(NOT CMAKE_SYSTEM_PROCESSOR)
    message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR unknown")
  endif()
endif()

find_program(GCC_COMPILER ${target_triple}-gcc)
if(NOT GCC_COMPILER)
  message(FATAL_ERROR "${target_triple}-gcc not found")
endif()

get_filename_component(gcc_toolchain ${GCC_COMPILER} DIRECTORY)
get_filename_component(gcc_toolchain ${gcc_toolchain} DIRECTORY)

if(NOT CMAKE_SYSROOT)
  execute_process(COMMAND ${GCC_COMPILER} -print-sysroot
    OUTPUT_VARIABLE CMAKE_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT CMAKE_SYSROOT)
    message(FATAL_ERROR "CMAKE_SYSROOT unknown")
  endif()
endif()

if(NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER ${target_triple}-gcc)
endif()

if(NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER ${target_triple}-g++)
endif()

get_filename_component(cc  ${CMAKE_C_COMPILER} NAME_WE)
get_filename_component(cxx ${CMAKE_CXX_COMPILER} NAME_WE)
if(cc STREQUAL clang OR cxx STREQUAL clang++)
  message(STATUS "+++ GCC Toolchain Root: ${gcc_toolchain}")
  foreach(flag
    CMAKE_C_COMPILER_TARGET
    CMAKE_CXX_COMPILER_TARGET
    CMAKE_ASM_COMPILER_TARGET)
    set(${flag} ${target_triple})
  endforeach()

  foreach(flag
    CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN
    CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN
    CMAKE_ASM_COMPILER_EXTERNAL_TOOLCHAIN)
    set(${flag} ${gcc_toolchain})
  endforeach()

  foreach(flag
    CMAKE_EXE_LINKER_FLAGS
    CMAKE_SHARED_LINKER_FLAGS
    CMAKE_MODULE_LINKER_FLAGS)
    string(APPEND ${flag} " -fuse-ld=lld")
  endforeach()

  unset(flag)
endif()
unset(cc)
unset(cxx)

if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
  find_program(QEMU_EMULATOR NAMES qemu-${CMAKE_SYSTEM_PROCESSOR}-static qemu-${CMAKE_SYSTEM_PROCESSOR})
  if(QEMU_EMULATOR)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${QEMU_EMULATOR} -L ${CMAKE_SYSROOT})
  endif()
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

unset(gcc_toolchain)
unset(target_triple)
